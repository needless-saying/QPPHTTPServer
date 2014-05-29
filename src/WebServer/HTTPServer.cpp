/* Copyright (C) 2011 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#include "stdafx.h"
#include <assert.h>
#include <io.h>
#include <algorithm>
#include "HttpServer.h"
#include "HTTPRequest.h"
#include "HTTPResponder.h"
#include "FCGIResponder.h"
#include "HTTPResponderFactory.h"
#include "HTTPConnection.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
HTTPServer::HTTPServer()
{
}

HTTPServer::~HTTPServer()
{
}

bool HTTPServer::mapServerFilePath(const std::string& orgUrl, std::string& serverPath)
{
	// 对url进行 utf-8 解码
	std::string deUrl = decode_url(orgUrl);

	// 获得根目录
	serverPath = _docRoot;
	if(serverPath.back() == '\\') serverPath.erase(--serverPath.end());

	// 与 URL 中的路径部分(参数部分忽略)合并获得完整路径
	std::string::size_type pos = orgUrl.find('?');
	if( pos != std::string::npos )
	{
		serverPath += orgUrl.substr(0, pos);
	}
	else
	{
		serverPath += deUrl;
	}
	
	// URL的正斜杠替换为反斜杠.
	for(std::string::iterator iter = serverPath.begin(); iter != serverPath.end(); ++iter)
	{
		if( *iter == '/' ) *iter = '\\'; 
	}

	// 如果是目录名并且不允许浏览目录,则尝试添加默认文件名
	if(serverPath.back() == '\\' && !isDirectoryVisible())
	{
		// 禁止浏览目录,先尝试打开默认文件
		bool hasDftFile = false;
		str_vec_t dftFileNames;
		split_strings(defaultFileNames(), dftFileNames, ",");
		for(str_vec_t::iterator iter = dftFileNames.begin(); iter != dftFileNames.end(); ++iter)
		{
			std::string dftFilePath(serverPath);
			dftFilePath += *iter;
			if(OSFile::exist(dftFilePath.c_str()))
			{
				serverPath += *iter;
				hasDftFile = true;
				break;
			}
		}

		return hasDftFile;
	}
	else
	{
		return true;
	}
}

std::string HTTPServer::tmpFileName()
{
	char fileName[MAX_PATH + 1] = {0};
	if( 0 == GetTempFileNameA(_tmpRoot.c_str(), _tmpFileNamePre, 0, fileName))
	{
		assert(0);

		/* 无法获取临时文件名,则按序号生成一个 */
		int no = 0;
		_lock.lock();
		no = ++_tmpFileNameNo;
		_lock.unlock();

		std::stringstream fmt;

		if(_tmpRoot.back() == '\\')
		{
			fmt << _tmpRoot << no << ".tmp";
		}
		else
		{
			fmt << _tmpRoot << '\\' << no << ".tmp";
		}

		return fmt.str();
	}
	else
	{
		return fileName;
	}
}

void HTTPServer::catchRequest(IRequest* request, IResponder** responder, IResponderFactory** factory)
{
	// 生成一个 IResponder,并重新在adp上安装.
	if(request->isValid())
	{
		// 对于无效请求,使用默认的HTTPResponder发送一个 400 响应.
		*factory = _responderFactoryList.back();
		*responder = (*factory)->catchRequest(request);
	}
	else
	{
		*responder = NULL;
		*factory = NULL;
		for(auto itr = _responderFactoryList.begin(); itr != _responderFactoryList.end(); ++itr)
		{
			IResponderFactory *fty = *itr;
			IResponder* res = fty->catchRequest(request);
			if(res)
			{
				*factory = fty;
				*responder = res;
				break;
			}
		}
	}
}

int HTTPServer::start(IHTTPConfig *conf)
{
	int ret = SE_SUCCESS;

	do
	{
		/*
		* 复制配置参数
		*/
		_docRoot = conf->docRoot(); /*根目录*/
		_tmpRoot = conf->tmpRoot();
		_isDirectoryVisible = conf->dirVisible(); /*是否允许浏览目录*/
		_dftFileName = conf->defaultFileNames(); /*默认文件名*/
		_ip = conf->ip(); /*服务器IP地址*/
		_port = conf->port(); /*服务器侦听端口*/
		_maxConnections = conf->maxConnections(); /*最大连接数*/
		_maxConnectionsPerIp = conf->maxConnectionsPerIp(); /*每个IP的最大连接数*/
		_maxConnectionSpeed = conf->maxConnectionSpeed(); /*每个连接的速度限制,单位 b/s.*/

		_sessionTimeout = conf->sessionTimeout(); /*会话超时*/
		_recvTimeout = conf->recvTimeout(); /*recv, connect, accept 操作的超时*/
		_sendTimeout = conf->sendTimeout(); /*send 操作的超时*/
		_keepAliveTimeout = conf->keepAliveTimeout();

		/*
		* 创建IO调度器
		*/
		_scheduler = create_scheduler();
		if(NULL == _scheduler)
		{
			assert(0);
			ret = SE_NETWORKFAILD;
			LOGGER_CERROR(theLogger, _T("无法初始化调度器.\r\n"));
			break;
		}

		/*
		* 初始化 Responder Factory List
		*/
		IResponderFactory *dftFactory = new DefaultResponderFactory(this);
		_responderFactoryList.push_back(dftFactory);

		/*
		* 创建侦听对象
		*/
		IOAdapter* listenAdp = IO_create_adapter();
		if(IO_ERROR_SUCESS != listenAdp->bind(ip().c_str(), port()))
		{
			IO_destroy_adapter(listenAdp);
			LOGGER_CERROR(theLogger, _T("无法绑定端口:%d.\r\n"), port());
			ret = SE_BINDFAILED;
			break;
		}
		else if(IO_ERROR_SUCESS != listenAdp->listen())
		{
			IO_destroy_adapter(listenAdp);
			LOGGER_CERROR(theLogger, _T("无法侦听端口[%d].\r\n"), port());
			ret = SE_LISTENFAILED;
			break;
		}

	
		/*
		* 安装第一个状态机(侦听器)开始提供HTTP服务
		*/
		HTTPListener* listener = new HTTPListener();
		install(
			listenAdp,
			IO_EVENT_EPOLLIN,
			listener,
			std::bind(&HTTPServer::handleListener, this, listener, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
			std::bind(&HTTPServer::handleListener, this, listener, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
			std::bind(&HTTPServer::handleListener, this, listener, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
			);
		_listenerList.push_back(std::make_pair(listenAdp, listener));

		/*
		* 调度器开始调度
		*/
		if(0 !=_scheduler->start(GetSystemProcessorNumber() * 2))
		{
			assert(0);
			ret = SE_NETWORKFAILD;
			LOGGER_CERROR(theLogger, _T("调度器无法启动.\r\n"));
			break;
		}

		/*
		* 成功的出口
		*/
		std::string ipAddress = conf->ip();
		if(ipAddress == "")
		{
			get_ip_address(ipAddress);
		}
		LOGGER_CINFO(theLogger, _T("Q++ HTTP Server 正在运行,根目录[%s],地址[%s:%d],最大连接数[%d].\r\n"), 
			AtoT(docRoot()).c_str(), AtoT(ipAddress).c_str(), port(), maxConnections());

		return SE_SUCCESS;
	}while(0);

	/*
	* 失败的出口
	*/
	stop();
	LOGGER_CWARNING(theLogger, _T("Q++ HTTP Server 启动失败.\r\n"));
	return ret;
}

int HTTPServer::stop()
{
	if(_scheduler)
	{
		// 停止调度器
		_scheduler->stop();

		// 删除所有的侦听对象
		for(auto itr = _listenerList.begin(); itr != _listenerList.end(); ++itr)
		{
			uninstall(itr->first);
		}
		_listenerList.clear();

		// 删除所有状态机
		if(_stmList.size() > 0)
		{
			LOGGER_CWARNING(theLogger, _T("退出时还[%d]未卸载的状态机(连接).\r\n"), _stmList.size());
		}
		for(auto itr = _stmList.begin(); itr != _stmList.end(); ++itr)
		{
#ifdef DEBUG
			LOGGER_DEBUG(theLogger, _T("强制卸载状态机") << AtoT(typeid(*(itr->second)).name()) << _T("\r\n")); 
#endif
			// 卸载状态机
			_scheduler->uninstall(itr->first);

			// 关闭 IOAdapter 
			IO_destroy_adapter(itr->first);

			// 删除状态机
			delete itr->second;
		}
		_stmList.clear();

		// 删除 Responder Factory
		for(auto itr = _responderFactoryList.begin(); itr != _responderFactoryList.end(); ++itr)
		{
			delete *itr;
		}
		_responderFactoryList.clear();

		destroy_scheduler(_scheduler);
		_scheduler = NULL;

		LOGGER_CINFO(theLogger, _T("Q++ HTTP Server 停止.\r\n"));
	}
	return SE_SUCCESS;
}

class stmRemoveFunctor
{
private:
	IOAdapter* _adp;
public:
	stmRemoveFunctor(IOAdapter* adp) : _adp(adp) {};
	bool operator () (const std::pair<IOAdapter*, IOStateMachine*>& val)
	{
		return val.first == _adp;
	}
};

int HTTPServer::install(IOAdapter* adp, u_int ev, IOStateMachine* stm, sm_handler_t onStepDone, sm_handler_t onDone, sm_handler_t onAbort)
{
	_lock.lock();
	_stmList.push_back(std::make_pair(adp, stm));
	_lock.unlock();

	_scheduler->install(adp, ev, stm, onStepDone, onDone, onAbort);

	return 0;
}

int HTTPServer::uninstall(IOAdapter* adp)
{
	IOStateMachine* stm = NULL;
	_scheduler->uninstall(adp);

	_lock.lock();
	iosm_list_t::iterator itr = std::find_if(_stmList.begin(), _stmList.end(), stmRemoveFunctor(adp));
	if(itr == _stmList.end())
	{
		assert(0);
	}
	else
	{
		stm = itr->second;
		_stmList.erase(itr);
	}
	_lock.unlock();

	if(stm)
	{
		delete stm;
		IO_destroy_adapter(adp);
	}
	return 0;
}

/*
* 增加一个新连接
*/
void HTTPServer::handleListener(HTTPListener* listener, IOAdapter* adp, IOStateMachine* sm, stm_result_t* res)
{
	if(res->rc == STM_ABORT)
	{
		assert(0);
		LOGGER_ERROR(theLogger, _T("侦听套接字出错.\r\n"));
	}
	else
	{
		IOAdapter* newAdp = listener->getAdapter();
		HTTPConnection *conn = new HTTPConnection(this, newAdp);
		LOGGER_INFO(theLogger, _T("[") << AtoT(conn->getRemoteIPAddr()) << _T(":") << conn->getRemotePort() << _T("] - 新连接被接受.") << _T("\r\n"));

		// 安装一个 CONNECTION 状态机处理HTTP连接
		// 初始事件设置为 IO_EVENT_EPOLLOUT 而不是 IO_EVENT_EPOLLIN 是因为需要 connection 马上运行一次. 新连接的IOAdapter总是可写,所以设置 IO_EVENT_EPOLLOUT 可以让调度器马上调度 conn 运行一次.
		install(newAdp, IO_EVENT_EPOLLOUT, conn,
			std::bind(&HTTPServer::handleConnection, this, conn, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
			std::bind(&HTTPServer::handleConnection, this, conn, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
			std::bind(&HTTPServer::handleConnection, this, conn, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
}

void HTTPServer::handleConnection(HTTPConnection* conn, IOAdapter* adp, IOStateMachine* sm, stm_result_t* res)
{
	if(STM_STEPDONE == res->rc)
	{
		request_statis_t requestInfo;
		conn->queryLastRequest(&requestInfo);
		if(res->st == 1)
		{
			// 请求开始
			char method[100];
			map_method(requestInfo.method, method);
			LOGGER_INFO(theLogger, _T("[") << AtoT(conn->getRemoteIPAddr()) << _T(":") << conn->getRemotePort() << _T("] - [") << method << _T("][") \
				<< AtoT(requestInfo.uri) << _T("]...") << _T("\r\n"));
		}
		else
		{
			// 请求处理结束
			LOGGER_INFO(theLogger, _T("[") << AtoT(conn->getRemoteIPAddr()) << _T(":") << conn->getRemotePort() << _T("] - [") << AtoT(requestInfo.uri) << _T("]")\
				<< _T(" -> [") << requestInfo.sc << _T("],接收[") << AtoT(format_size(requestInfo.bytesRecved)) << _T("],发送[") << AtoT(format_size(requestInfo.bytesSent)) << _T("]")\
				<< _T(",用时[") << AtoT(format_time(requestInfo.usedTime)) << _T("],平均速率[") << AtoT(format_speed(requestInfo.bytesSent, requestInfo.usedTime)) << _T("].")\
				<< _T("\r\n"));

			// setprecision(3) <iomanip>
		}
	}
	else if(STM_DONE == res->rc)
	{
		// 连接关闭
		connection_statis_t connInfo;
		conn->query(&connInfo);

		LOGGER_INFO(theLogger, _T("[") << AtoT(conn->getRemoteIPAddr()) << _T(":") << conn->getRemotePort() << _T("] - 连接关闭,总计:")\
			<< _T("处理连接[") << connInfo.requestCount << _T("]")\
			<< _T(",接收[") << AtoT(format_size(connInfo.bytesRecved)) << _T("],发送[") << AtoT(format_size(connInfo.bytesSent)) << _T("]")\
			<< _T(",用时[") << AtoT(format_time(connInfo.usedTime)) << _T("],平均速率[") << AtoT(format_speed(connInfo.bytesSent, connInfo.usedTime)) << _T("].")\
			<< _T("\r\n"));

		uninstall(adp);
	}
	else if(STM_ABORT == res->rc)
	{
		/*
		* HTTP连接总是在处理HTTP请求,只有请求处理出错才会导致HTTP连接出错,所以在出错的情况下(STM_ABORT)总是要同时显示一个请求终止的信息和一个连接终止的信息
		*/

		// get_error_message 映射 SE_XXX 的错误字符串
		// ..
		// ..

		// 请求处理终止
		request_statis_t requestInfo;
		conn->queryLastRequest(&requestInfo);
		LOGGER_INFO(theLogger, _T("[") << AtoT(conn->getRemoteIPAddr()) << _T(":") << conn->getRemotePort() << _T("] - 请求终止:[") << AtoT(requestInfo.uri) << _T("]")\
			<< _T(" -> [") << requestInfo.sc << _T("],接收[") << AtoT(format_size(requestInfo.bytesRecved)) << _T("],发送[") << AtoT(format_size(requestInfo.bytesSent)) << _T("]")\
			<< _T(",用时[") << AtoT(format_time(requestInfo.usedTime)) << _T("],平均速率[") << AtoT(format_speed(requestInfo.bytesSent, requestInfo.usedTime)) << _T("].")\
			<< _T("\r\n"));

		// 连接终止
		connection_statis_t connInfo;
		conn->query(&connInfo);

		LOGGER_INFO(theLogger, _T("[") << AtoT(conn->getRemoteIPAddr()) << _T(":") << conn->getRemotePort() << _T("] - 连接终止,总计:")\
			<< _T("请求[") << connInfo.requestCount << _T("]")\
			<< _T(",接收[") << AtoT(format_size(connInfo.bytesRecved)) << _T("],发送[") << AtoT(format_size(connInfo.bytesSent)) << _T("]")\
			<< _T(",用时[") << AtoT(format_time(connInfo.usedTime)) << _T("].")\
			<< _T("\r\n"));

		uninstall(adp);
	}
	else
	{
		assert(0);
	}
}