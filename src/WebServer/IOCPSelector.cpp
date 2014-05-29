#include "stdafx.h"
#include <algorithm>
#include "IOCP.h"

/************************************************************************/
/*                                                                      */
/************************************************************************/
static u_int __stdcall iocp_worker_thread(void* lpParam)
{
	IOCPSelector* selector = (IOCPSelector*)lpParam;
	return selector->loop();
}

IOCPSelector::IOCPSelector()
	:_iocpBroadcast(NULL), _iocpWorker(NULL), _threads(MIN_IOCP_THREADS), _tids(NULL)
{
	/*
	* 创建一个完成端口广播对象
	*/
	if( NULL == (_iocpBroadcast = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, _threads)) )
	{
		assert(0);
	}

	_threads = GetSystemProcessorNumber();
	if(_threads < MIN_IOCP_THREADS) _threads = MIN_IOCP_THREADS;

	/*
	* 创建一个完成端口工作对象
	*/
	if( NULL == (_iocpWorker = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, _threads)) )
	{
		assert(0);
	}

	/*
	* 创建一个合适大小的同步锁池分配给所有的IOCPAdapter
	*/
	_lockPool.init(_threads * 2);

	/*
	* 创建若干个线程执行 GetQueuedCompletionStatus 函数
	*/
	_tids = new uintptr_t[_threads];
	for(int i = 0; i < _threads; ++i)
	{
		if(0 == (_tids[i] = _beginthreadex(NULL, 0, iocp_worker_thread, this, 0, NULL)))
		{
			assert(0);
		}
	}
}

IOCPSelector::~IOCPSelector()
{
	/*
	* 停止的次序至关重要
	*/
	// 停止工作线程
	if(_tids)
	{
		for(int i = 0; i < _threads; ++i)
		{
			PostQueuedCompletionStatus(_iocpWorker, 0, NULL, NULL);
		}
		for(int i = 0; i < _threads; ++i)
		{
			if( _tids[i] ) 
			{
				WaitForSingleObject(reinterpret_cast<HANDLE>(_tids[i]), INFINITE);
				CloseHandle(reinterpret_cast<HANDLE>(_tids[i])); 
			}
		}
		delete []_tids;
		_tids = NULL;
		_threads = 0;
	}

	// 关闭 iocp 句柄
	if(NULL != _iocpWorker)
	{
		CloseHandle(_iocpWorker);
		_iocpWorker = NULL;
	}

	// 方案1:增大信号量计数时所有调用了 epoll 函数的线程有机会退出等待状态并得到poll返回值 POLL_EXIT
	//ReleaseSemaphore(_activeAdpaterListSem, 10000000, NULL);

	// 方案2: 直接关闭句柄, epoll 返回值 POLL_ERROR --> 是不是过于粗暴 -_-!
	if(_iocpBroadcast)
	{
		CloseHandle(_iocpBroadcast);
		_iocpBroadcast = NULL;
	}

	// 删除所有iocpcontext
	for(auto itr = _ctxList.begin(); itr != _ctxList.end(); ++itr)
	{
		IOCPContext* ctx = *itr;
		assert(ctx->adapter == NULL);
		delete ctx;
	}
	_ctxList.clear();

	// 销毁锁池
	_lockPool.destroy();
}

void IOCPSelector::lock()
{
	_lock.lock();
}

void IOCPSelector::unlock()
{
	_lock.unlock();
}

Lock* IOCPSelector::allocLock()
{
	return _lockPool.allocate();
}

void IOCPSelector::freeLock(Lock* l)
{
	_lockPool.recycle(l);
}

/*
* 手动触发一个事件
*/
int IOCPSelector::raiseEvent(IOCPContext* ctx, u_int ev)
{
	ev &= ctx->epollEvent;
	if(ev != IO_EVENT_NONE)
	{
		SET_BIT(ctx->curEvent, ev);
		if(ctx->isInQueue)
		{
		}
		else
		{
			ctx->isInQueue = true;
			PostQueuedCompletionStatus(_iocpBroadcast, 0, (ULONG_PTR)ctx, NULL);
		}
	}
	return 0;
}

/*
* IOCP对象的工作线程函数,调用GetQueuedCompletionStatus从系统队列中
*/
int IOCPSelector::loop()
{
	int ret = 0;
	for(;;)
	{
		DWORD transfered = 0;
		IOCPContext* ctx = NULL;
		IOCPOVERLAPPED* iocpOlpPtr = NULL;
		if(!GetQueuedCompletionStatus(_iocpWorker, &transfered, reinterpret_cast<PULONG_PTR>(&ctx), (LPOVERLAPPED*)&iocpOlpPtr, INFINITE))
		{
			if(iocpOlpPtr)
			{
				/*
				* IO操作被标记为失败
				*/
				onIoCompleted(ctx, false, iocpOlpPtr, transfered);
			}
			else
			{
				/*
				* IOCP本身发生了一些错误,可能是超时 GetLastError returns WAIT_TIMEOUT 或者其他系统错误
				*/
				assert(0);
				ret = GetLastError();
				break;
			}
		}
		else
		{
		
			if(transfered == 0 && iocpOlpPtr == NULL && ctx == NULL)
			{
				/*
				* 约定的正常退出标志
				*/
				break;
			}
			else
			{
				/*
				* 根据MSDN的说明GetQueuedCompletionStatus()返回TRUE[只]表示从IOCP的队列中取得一个成功完成IO操作的包.
				* 这里"成功"的语义只是指操作这个动作本身成功完成,至于完成的结果是不是程序认为的"成功",不一定.
				* 
				* 1. AcceptEx 和 ConnectEx 成功的话,如果不要求一起发送/接收数据(地址的内容除外),那么 transfered == 0成立.
				* 2. Send, Recv 请求是如果传入的缓冲区长度大于0,而transfered == 0应该判断为失败.
				*
				* 实际测试发现接受客户端连接,执行一个Recv操作,然后客户端马上断开,就能运行到这里,并且 Recv transfered == 0成立.
				* 总而言之,上层应该判断如果传入的数据(不包括AcceptEx和ConnectEx接收的远程地址,而专门指数据部分)缓冲区长度大于0,
				* 而返回的结果表示 transfered = 0 说明操作失败.
				*
				* 网络模块本身无法根据 transfered是否等于0来判断操作是否成功,因为上层完全可能投递一个缓冲区长度为0的Recv请求.
				* 这在服务器开发中是常用的技巧,用来节约内存.
				*
				*/
				onIoCompleted(ctx, true, iocpOlpPtr, transfered);
			}
		}
	}
	return ret;
}

/*
* IO操作完成
*
*/
void IOCPSelector::onIoCompleted(IOCPContext* ctx, bool oppResult, IOCPOVERLAPPED* olp, size_t bytesTransfered)
{
	bool canDel = false;
	u_int ev = IO_EVENT_NONE;

	/* 用IOCPAdapter本身的锁来保证删除操作的同步,要非常小心,注意调用 IOCPAdapter的其他方法时有没有也用到了锁导致死锁. */
	ctx->lock();

	ev = ctx->onIoCompleted(oppResult, olp, bytesTransfered);

	if(TEST_BIT(ev, IO_EVENT_NONE))
	{
		/* 忽略 */
	}
	else if(TEST_BIT(ev, IO_EVENT_AUTODELETE))
	{
		/* 自动删除 */
		canDel = closeContext(ctx);
	}
	else
	{
	}
	
	/*
	* 进入活跃队列(发送到 IOCP:B)
	* 如果已经在队列中,则把事件或与,否则添加新的项.
	*/
	if(IO_EVENT_NONE != ev)
	{
		raiseEvent(ctx, ev);
	}

	ctx->unlock();

	// 自动删除
	if(canDel)
	{
		freeContext(ctx);
	}
}

IOCPContext* IOCPSelector::allocContext(IOCPAdapter* adp)
{
	IOCPContext* ctx = new IOCPContext(adp->getSock(), adp->getSockType(), IO_RECV_BUF_LEN, IO_SEND_BUF_LEN);
	ctx->setLock(allocLock());
	adp->attach(ctx);
	ctx->adapter = adp;

	lock();
	_ctxList.push_back(ctx);
	unlock();
	return ctx;
}

void IOCPSelector::freeContext(IOCPContext* ctx)
{
	ctx->detach();
	freeLock(ctx->setLock(NULL));

	lock();
	_ctxList.remove(ctx);
	unlock();

	delete ctx;
}

bool IOCPSelector::closeContext(IOCPContext* ctx)
{
	bool canDel = false;
	if(ctx->busy())
	{
		// 设置一个自动删除的标记
		ctx->isAutoDelete = true;
	}
	else
	{
		if(ctx->isInQueue)
		{
			// 已经发送到 IOCP:B,由广播端执行删除动作.
		}
		else
		{
			canDel = true;
		}
	}

	return canDel;
}

int IOCPSelector::ctl(IOAdapter* adp, int op, u_int ev)
{
	IOCPAdapter *iocpAdp = dynamic_cast<IOCPAdapter*>(adp);
	IOCPContext* ctx = iocpAdp->getContext();
	u_int curEv = IO_EVENT_NONE;

	if(IO_CTL_ADD == op)
	{
		/*
		* 分配一个新的 IOCPContext 指针给 IOCPAdapter
		*/
		if(ctx != NULL)
		{
			return ctl(adp, IO_CTL_MOD, ev);
		}

		ctx = allocContext(iocpAdp);
		if( _iocpWorker != CreateIoCompletionPort((HANDLE)iocpAdp->getSock(), _iocpWorker, (ULONG_PTR)ctx, 0))
		{
			assert(0);
		}
		else
		{
			ctx->epollEvent = ev;
		}
		curEv = ctx->updateEvent(ctx->epollEvent);
	}
	else if(IO_CTL_MOD == op)
	{
		assert(ctx);
		/*
		* 修改事件屏蔽字
		*/
		ctx->lock();

		ctx->epollEvent = ev;
		curEv = ctx->updateEvent(ctx->epollEvent);

		ctx->unlock();
	}
	else if(IO_CTL_DEL == op)
	{
		/* 
		* Windows IOCP 无法取消套接字和IOCP句柄的关联,所以, DEL 无法完全实现. 
		* 在 IOAdpater 生命周期, 只能用一次 IO_CTL_DEL. 调用后应该尽快删除. DEL 后重新 ADD 不能保证数据正确.
		*/
		/* 检查指针的有效性 */
		bool validPtr = true;
		lock();
		if(_ctxList.end() == std::find(_ctxList.begin(), _ctxList.end(), ctx))
		{
			validPtr = false;
		}
		unlock();
		if(validPtr)
		{
			/*
			* 分离 IOCPAdapter 和 IOCPContext 指针
			*/
			bool del = false;

			ctx->lock();

			iocpAdp->detach();
			ctx->detach();
			ctx->epollEvent = IO_EVENT_NONE;
			ctx->adapter = NULL;
			del = closeContext(ctx);

			ctx->unlock();

			if(del)
			{
				freeContext(ctx);
			}
		}
		else
		{
			return IO_ERROR_UNDEFINED;
		}
	}
	else
	{
		assert(0);
	}

	// 手动触发已经存在的事件
	if(curEv != IO_EVENT_NONE)
	{
		raiseEvent(ctx, curEv);
	}
	return IO_ERROR_SUCESS;
}

int IOCPSelector::wakeup(size_t count)
{
	if(_iocpBroadcast)
	{
		for(size_t i = 0; i < count; ++i)
		{
			PostQueuedCompletionStatus(_iocpBroadcast, 0, NULL, NULL);
		}
	}
	else
	{
		return IO_ERROR_UNDEFINED;
	}

	return IO_ERROR_SUCESS;
}

/*
* select 返回一个活跃IOAdapter(可读/可写/出错),状态由IOAdapter保存,或者返回NULL表示超时或者退出.
*/
int IOCPSelector::wait(IOAdapter** adp, u_int* ev, u_int timeo /* = INFINITE */)
{
	int ret = IO_WAIT_SUCESS;
	for(;;)
	{
		DWORD transfered = 0;
		IOCPContext *ctx = NULL;
		IOCPOVERLAPPED* iocpOlpPtr = NULL;
		bool canDel = false;
		bool skip = false;
		if(!GetQueuedCompletionStatus(_iocpBroadcast, &transfered, reinterpret_cast<PULONG_PTR>(&ctx), (LPOVERLAPPED*)&iocpOlpPtr, timeo))
		{
			/* 广播句柄被关闭是异常退出标记 */
			WAIT_TIMEOUT == GetLastError() ? ret = IO_WAIT_TIMEOUT : ret = IO_WAIT_ERROR;
		}
		else
		{
		
			if(transfered == 0 && iocpOlpPtr == NULL && ctx == NULL)
			{
				/*
				* 约定的正常退出标志
				*/
				ret = IO_WAIT_EXIT;
			}
			else
			{
				/*
				* 获取到广播的结果(经过过滤的,发送给用户层的结果)
				*/
				ctx->lock();

				// 出队列
				ctx->isInQueue = false;

				// 清空事件屏蔽字
				if(TEST_BIT(ctx->epollEvent, IO_EVENT_EPOLLONESHOT))
				{
					ctx->epollEvent = IO_EVENT_NONE;
				}

				// 如果已经设置了自动删除标记则执行一次删除动作
				if(ctx->isAutoDelete)
				{
					skip = true;
					canDel = closeContext(ctx);
				}
				else
				{
					*adp = ctx->adapter;
					*ev = ctx->curEvent;
					ctx->curEvent = IO_EVENT_NONE;
				}
				ctx->unlock();

				if(canDel)
				{
					freeContext(ctx);
				}
				if(skip)
				{
					continue;
				}
			}
		}
		break;
	}
	return ret;
}

IOSelector* IO_create_selector()
{
	return  new IOCPSelector();
}

int IO_destroy_slector(IOSelector* s)
{
	delete dynamic_cast<IOCPSelector*>(s);
	return 0;
}