#include "StdAfx.h"
#include "FCGIFactory.h"
#include "HTTPLib.h"
#include "HTTPResponder.h"
#include "FCGIResponder.h"

static unsigned int WINAPI fcgi_spawn_proc(void* param)
{
	FCGIFactory::fcgi_process_context_t* ctx = (FCGIFactory::fcgi_process_context_t*)param;
	return ctx->instPtr->spawn(ctx);
}

FCGIFactory::FCGIFactory(IHTTPServer *svr, IOStateMachineScheduler* scheduler)
	: _svr(svr), _scheduler(scheduler), _fcgiId(1)
{
	memset(&_fcgiServerCtx, 0, sizeof(fcgi_server_ctx_t));
}

FCGIFactory::~FCGIFactory()
{
	release();
}

void FCGIFactory::lock()
{
	_lock.lock();
}

void FCGIFactory::unlock()
{
	_lock.unlock();
}

u_short FCGIFactory::getFCGIId()
{
	u_short newid = 0;
	lock();
	newid = _fcgiId++;
	unlock();
	return newid;
}

void FCGIFactory::init(fcgi_server_ctx_t* ctx)
{
	memcpy(&_fcgiServerCtx, ctx, sizeof(fcgi_server_ctx_t));
}

void FCGIFactory::release()
{
	/* 释放所有连接 */
	for(auto itr = _busyAdpList.begin(); itr != _busyAdpList.end(); ++itr)
	{
		IO_destroy_adapter(*itr);
	}
	_busyAdpList.clear();

	for(auto itr = _idleAdpList.begin(); itr != _idleAdpList.end(); ++itr)
	{
		IO_destroy_adapter(*itr);
	}
	_idleAdpList.clear();

	/* 清空等待队列 */
	_waitingList.clear();

	/* 杀死FCGI进程 */
	for(auto itr = _processList.begin(); itr != _processList.end(); ++itr)
	{
		fcgi_process_context_t* context = *itr;

		/* 如果连接线程正在运行中,等待结束 */
		if(context->thread)
		{
			if( WAIT_OBJECT_0 != WaitForSingleObject((HANDLE)context->thread, 0) )
			{
				TerminateThread((HANDLE)context->thread, 1);
			}
			CloseHandle((HANDLE)context->thread);
		}

		/* 如果FCGI进程正在运行,终止之 */
		if(context->processInfo.hProcess)
		{
			if(WAIT_OBJECT_0 != WaitForSingleObject(context->processInfo.hProcess, 0))
			{
				TerminateProcess(context->processInfo.hProcess, 1);
			}
			CloseHandle(context->processInfo.hThread);
			CloseHandle(context->processInfo.hProcess);
		}

		delete context;
	}
	_processList.clear();
	_fcgiId = 1;
}

// 关键函数,分派一个FCGI连接
void FCGIFactory::dispatch()
{
	if(_waitingList.size() <= 0) return;
	IOAdapter* adp = NULL;
	
	// 检查空闲队列
	if(!_idleAdpList.empty())
	{
		// 从空闲队列中获取一个连接,并加入到忙碌队列中
		adp = _idleAdpList.back();
		_idleAdpList.pop_back();
		_busyAdpList.push_back(adp);
	}
	else
	{
		// 是否允许创建一个新的连接
		if(_busyAdpList.size() < _fcgiServerCtx.maxConnections)
		{
			if(_fcgiServerCtx.port != 0)
			{
				// 远程模式,直接创建一个socket IOAdapter
				IOAdapter* newAdp = IO_create_adapter();
				newAdp->bind(NULL, 0);
				newAdp->connect(_fcgiServerCtx.path, _fcgiServerCtx.port);
				_scheduler->install(newAdp, IO_EVENT_EPOLLOUT, this, NULL, NULL, NULL);

				// 新创建但未就绪的连接进入忙碌队列
				_busyAdpList.push_back(newAdp);
			}
			else
			{
				// 本地模式启动一个线程来完成创建子进程,等待命名管道等耗时操作
				fcgi_process_context_t *ctx = new fcgi_process_context_t;
				memset(ctx, 0, sizeof(fcgi_process_context_t));
				ctx->instPtr = this;
				strcpy(ctx->pipeName, _fcgiServerCtx.path);
				ctx->thread = _beginthreadex(NULL, 0, fcgi_spawn_proc, ctx, 0, NULL);

				if(0 == ctx->thread)
				{
					LOGGER_CERROR(theLogger, _T("无法为创建FCGI本地进程生成一个工作线程,_beginthreadex调用失败,错误码:%d.\r\n"), errno);
					delete ctx;
				}
				else
				{
					_processList.push_back(ctx);
				}
			}
		}
		else
		{
			// 什么都不做,等待空闲连接
		}
	}

	if(adp)
	{
		// 从等待队列中获取一个FCGIResponder
		FCGIResponder* responder = _waitingList.front();
		_waitingList.pop_front();

		// FCGIResponder需要知道哪个连接是和FCGI服务器之间的连接,哪个是和HTTP客户端之间的连接,故有此一函数.
		responder->setFCGIConnection(getFCGIId(), adp);

		// 安装状态机
		_scheduler->install(adp, IO_EVENT_EPOLLOUT, responder, NULL, 
			std::bind(&FCGIFactory::handleResponder, this, responder, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
			std::bind(&FCGIFactory::handleResponder, this, responder, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
}

// FCGIResponder 与FCGI服务器交互完成后触发.
void FCGIFactory::handleResponder(FCGIResponder* responder, IOAdapter* adp, IOStateMachine* sm, stm_result_t* res)
{
}

IResponder* FCGIFactory::catchRequest(IRequest* request)
{
	// 根据uri的正则表达式来决定是否捕获.
	IResponder* responder = NULL;
	std::string scriptFileName;
	if(_svr->mapServerFilePath(request->uri(true), scriptFileName))
	{
		std::string ext;
		get_file_ext(scriptFileName, ext);

		if(match_file_ext(ext, _fcgiServerCtx.exts))
		{
			// 创建一个FCGIResponder,进入等待队列,然后分派
			if(_fcgiServerCtx.maxWaitListSize == 0 || _waitingList.size() < _fcgiServerCtx.maxWaitListSize)
			{
				FCGIResponder* fcgiResponder = new FCGIResponder(_svr, request, _fcgiServerCtx.cacheAll == TRUE);
				_waitingList.push_back(fcgiResponder);

				dispatch();
			}
			else
			{
				// 达到最大连接数,返回503,服务器忙碌
				responder = new HTTPResponder(_svr, request, SC_SERVERBUSY, g_HTTP_Server_Busy);
			}
		}
	}

	return responder;
}

void FCGIFactory::releaseResponder(IResponder* responder)
{
	// 用 typeid 怎么感觉那么恶心呢?
	if(typeid(*responder) == typeid(HTTPResponder))
	{
		// 如果是HTTPResponder,直接删除
	}
	else
	{
		// 如果是FCGIResponder,回收FCGI连接,然后删除.
		FCGIResponder* fcgiResponder = (FCGIResponder*)responder;
		IOAdapter* adp = fcgiResponder->setFCGIConnection(0, NULL);
		_idleAdpList.push_back(adp);
	}
	delete responder;
}

/*
* 用一个单独的线程创建一个FCGI服务子进程,并用命名管道连接
*/
int FCGIFactory::spawn(fcgi_process_context_t* context)
{
	/* pipeName 参数传人时是进程路径 */
	char fcgProcPath[MAX_PATH];
	strcpy(fcgProcPath, context->pipeName);

	/* 生成一个唯一的管道名 */
	unsigned int seed = static_cast<unsigned int>(time( NULL )); /* 确保不同exe实例间不同 */
	seed += reinterpret_cast<unsigned int>(context); /* 确保同一个exe实例中,不同的线程不同 */
	srand(seed);
	sprintf(context->pipeName, "%s\\%04d_%04d", FCGI_PIPE_BASENAME, rand() % 10000, rand() % 10000);
	TRACE("pipename:%s.\r\n", context->pipeName);

	/* 创建一个命名管道 */
	HANDLE hPipe = CreateNamedPipe(AtoT(context->pipeName).c_str(),  PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_READMODE_BYTE,
		PIPE_UNLIMITED_INSTANCES,
		4096, 4096, 0, NULL);
	if( INVALID_HANDLE_VALUE == hPipe )
	{
		LOGGER_CERROR(theLogger, _T("无法创建命名管道,本地FCGI进程生成失败,错误码:%d\r\n"), GetLastError());
	}
	if(!SetHandleInformation(hPipe, HANDLE_FLAG_INHERIT, TRUE))
	{
		LOGGER_CERROR(theLogger, _T("SetHandleInformation()调用失败,本地FCGI进程生成失败,错误码:%d\r\n"), GetLastError());
	}

	/* 以命名管道为STDIN创建一个本地FCGI进程 */
	STARTUPINFO startupInfo;
	memset(&startupInfo, 0, sizeof(STARTUPINFO));
	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.dwFlags = STARTF_USESTDHANDLES;
	startupInfo.hStdInput  = hPipe;
	startupInfo.hStdOutput = INVALID_HANDLE_VALUE;
	startupInfo.hStdError  = INVALID_HANDLE_VALUE;

	if( !CreateProcess(AtoT(fcgProcPath).c_str(), NULL, NULL, NULL, TRUE,

#ifdef _DEBUG /* 调试状态下,创建的PHP-CGI进程带控制台窗口, release时不带控制台窗口 */	
		0, 
#else
		CREATE_NO_WINDOW,
#endif

		NULL, NULL, &startupInfo, &context->processInfo))
	{
		LOGGER_CERROR(theLogger, _T("CreateProcess()调用失败,无法生成本地FCGI进程,错误:%s.\r\n"), AtoT(get_last_error()).c_str());
	}

	/* 等待命名管道 */
	if(!WaitNamedPipe(AtoT(context->pipeName).c_str(), FCGI_CONNECT_TIMEO))
	{
		LOGGER_CERROR(theLogger, _T("连接命名管道失败[%s]\r\n"), AtoT(get_last_error()).c_str());
	}

	/* 连接准备就绪 */
	context->adp = IO_create_adapter(context->pipeName);
	_scheduler->install(context->adp, IO_EVENT_EPOLLOUT, this, NULL, NULL, NULL);
	return 0;
}

bool FCGIFactory::step0(IOAdapter* adp, int ev, stm_result_t* res)
{
	bool adpReady = true;
	if(TEST_BIT(ev, IO_EVENT_EPOLLERR))
	{
		// 本地错误,输出日志
		// ..
		adpReady = false;
	}

	if(TEST_BIT(ev, IO_EVENT_EPOLLHUP))
	{
		// 挂断,输出日志
		// ..
		adpReady = false;
	}

	if(!adpReady)
	{
		// 关闭失败的连接
		// 如果忙碌队列为空,说明所有FCGI连接都失败了, 使所有等待中的FCGIResponder以失败返回,然后清空等待队列
	}
	else
	{
		// 新连接准备就绪,从忙碌队列中移到空闲队列
		// 分派一个连接给等待队列中的FCGIResponder
		dispatch();
	}

	
	return false;
}