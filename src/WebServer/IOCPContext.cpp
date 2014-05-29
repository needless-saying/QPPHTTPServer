#include "stdafx.h"
#include "IOCP.h"

/************************************************************************/
/*  IOCP指针的实现                                                      */
/************************************************************************/

static bool is_socket_listening(SOCKET s)
{
	BOOL listening = FALSE;
	int len = sizeof(BOOL);
	getsockopt(s, SOL_SOCKET, SO_ACCEPTCONN, (char*)&listening, &len);

	return listening == TRUE;
}

static bool is_socket_connected(SOCKET s)
{
	sockaddr addr;
	int len = sizeof(sockaddr);
	return 0 == getpeername(s, &addr, &len);
}

static int get_socket_type(SOCKET s)
{
	int sockType = SOCK_STREAM;
	int len = sizeof(sockType);
	if(SOCKET_ERROR == getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&sockType, &len))
	{
		if(WSAENOTSOCK == WSAGetLastError())
		{
			sockType = SOCK_FILE;
		}
		else
		{
			assert(0);
		}
	}
	return sockType;
}


/*
* IOCPSelector 和 IOCPAdapter 操作 IOCPContext, IOCPContext 由 IOCPSelector 维护, ctl ADD时分配,并关联一个 IOCPAdapter.
* IOCPAdapter 被关闭时收回. IOCPAdapter 只是一个操作接口.
*/
IOCPContext::IOCPContext(SOCKET s, int sockType, size_t recvBufLen, size_t sendBufLen)
{
	_sock = s;
	_sockType = sockType;
	if(SOCK_FILE != _sockType)
	{
		_listening = is_socket_listening(s);
		_connected = is_socket_connected(s);
	}
	else
	{
		_listening = false;
		_connected = true;
	}

	_lock = NULL;
	_mode = IO_MODE_LT;
	_lastErrorCode = IO_ERROR_SUCESS;
	
	_newSock = INVALID_SOCKET;
	_acceptBuf = NULL;
	_lpfnAcceptEx = NULL;
	_shutdownFlag = -1;

	memset(&_sendOlp, 0, sizeof(iocp_overlapped_t));
	memset(&_recvOlp, 0, sizeof(iocp_overlapped_t));
	_recvOlp.len = recvBufLen;
	_sendOlp.len = sendBufLen;

	adapter = NULL;
	isInQueue = false;
	epollEvent = IO_EVENT_NONE;
	curEvent = IO_EVENT_NONE;
	isAutoDelete = false;
}

IOCPContext::~IOCPContext()
{
	if(_newSock != INVALID_SOCKET) closesocket(_newSock);
	if(_acceptBuf) delete []_acceptBuf;
	if(_recvOlp.buf) delete []_recvOlp.buf;
	if(_sendOlp.buf) delete []_sendOlp.buf;
}

Lock* IOCPContext::setLock(Lock* l)
{
	Lock* oldLock = _lock;
	_lock = l;
	return oldLock;
}

void IOCPContext::lock()
{
	if(_lock) _lock->lock();
}

void IOCPContext::unlock()
{
	if(_lock) _lock->unlock();
}

bool IOCPContext::busy()
{
	return _recvOlp.oppType != IO_OPP_NONE || _sendOlp.oppType != IO_OPP_NONE;
}

int IOCPContext::setMode(int m)
{
	int oldm = _mode;
	_mode = m;
	return oldm;
}

int IOCPContext::setLastError(int err)
{
	/* 
	* 只有发生错误才记录,和 WSAGetLastError 的语义一致 
	* The WSAGetLastError function returns the error status for the last Windows Sockets operation that failed.
	*/
	int olderr = _lastErrorCode;
	if(err != IO_ERROR_SUCESS)
	{
		_lastErrorCode = err;
	}
	return olderr;
}

int IOCPContext::getLastError()
{
	return _lastErrorCode;
}

bool IOCPContext::ready(int t)
{
	if(IO_READYTYPE_EPOLLIN == t)
	{
		if(_listening)
		{
			// 侦听套接字:已经有一个有效的sock acceptIO完毕
			return _recvOlp.oppType == IO_OPP_NONE && _newSock != INVALID_SOCKET;
		}
		else
		{
			// 接收缓冲内有未读数据->认为处于就绪状态. [start ->(已读) -> upos -> (未读) -> ipos (空闲或IOCP操作中) -> end]
			return _recvOlp.upos < _recvOlp.ipos;
		}
	}
	else
	{
		if(_listening)
		{
			// 侦听套接字不可写
			return false;
		}
		else
		{
			// 已经连接并且发送缓冲有空间->认为处于就绪状态 [start -> (正在发送) -> ipos -> (已写入未发送) -> upos -> (空闲) -> end]
			return _connected && _sendOlp.upos < _sendOlp.len;
		}
	}
}

u_int IOCPContext::updateEvent(u_int ev)
{
	u_int curEv = IO_EVENT_NONE;
	
	/*
	* 根据需要发起 第一个IOCP 操作.
	*/
	if(ev != IO_EVENT_NONE)
	{
		// 设置工作模式
		setMode(TEST_BIT(ev, IO_EVENT_EPOLLET) ? IO_MODE_ET : IO_MODE_LT);
		
		// 仅处理 EPOLLIN, EPOLLOUT 不需要手动开始,调用对应的函数自然会发起IOCP操作.
		if(getLastError() == IO_ERROR_SUCESS)
		{
			if(TEST_BIT(ev, IO_EVENT_EPOLLIN))
			{
				if(_listening)
				{
					setLastError(postAccept());
				}
				else
				{
					setLastError(postRecv());
				}
			}
		}
	}

	/*
	* 根据当前状态获得需要触发的事件
	*/
	// 只要有未读数据/或者侦听套接字就绪就认为"可读"不管套接字是否出错.
	if(ready(IO_READYTYPE_EPOLLIN))
	{
		SET_BIT(curEv, IO_EVENT_EPOLLIN);
	}

	// 只有套接字状态正常才"可写"
	int err = getLastError();
	if(IO_ERROR_SUCESS == err)
	{
		if(ready(IO_READYTYPE_EPOLLOUT))
		{
			SET_BIT(curEv, IO_EVENT_EPOLLOUT);
		}
	}
	else if(IO_ERROR_HUP == err)
	{
		SET_BIT(curEv, IO_EVENT_EPOLLHUP);
	}
	else
	{
		SET_BIT(curEv, IO_EVENT_EPOLLERR);
	}
	return curEv;
}

u_int IOCPContext::onIoCompleted(bool oppResult, IOCPOVERLAPPED* olp, size_t bytesTransfered)
{
	u_int ev = IO_EVENT_NONE;

	/*
	* 清理已经完成的操作:统计字节数等等.
	*/
	if(oppResult)
	{
		olp->transfered += bytesTransfered;
	}

	/* 清空标记 */
	int oppType = olp->oppType;
	olp->oppType = IO_OPP_NONE;

	/*
	*  分类处理已经完成的操作结果
	*/
	if(_sock == INVALID_SOCKET)
	{
		ev = IO_EVENT_AUTODELETE;
	}
	else
	{
		switch(oppType)
		{
		case IO_OPP_ACCEPT:
			{
				if(oppResult)
				{
					/* 更新套接字地址 */
					if( 0 != setsockopt( _newSock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&_sock, sizeof(_sock)) )
					{
						assert(0);
					}
					/* 触发可读事件(进入IOSelector的可读队列) */
					ev = IO_EVENT_EPOLLIN;
				}
				else
				{
					/*
					* WSAECONNRESET: 远端连接后又被关闭.
					*/
					closesocket(_newSock);
					_newSock = INVALID_SOCKET;
					ev = IO_EVENT_EPOLLHUP;
				}
			}
			break;
		case IO_OPP_CONNECT:
			{
				/* 连接成功则触发可写事件,否则触发出错事件(调用者应该能识别出此时的出错事件指连接失败) */
				if(oppResult)
				{
					ev = IO_EVENT_EPOLLOUT;
					_connected = true;
				}
				else
				{
					ev = IO_EVENT_EPOLLHUP;
				}
			}
			break;
		case IO_OPP_RECV:
			{
				if(oppResult && bytesTransfered > 0)
				{
					_recvOlp.ipos += bytesTransfered;
					if(_mode == IO_MODE_ET)
					{
						if(_recvOlp.et)
						{
							ev = IO_EVENT_EPOLLIN;
							_recvOlp.et = false;
						}
						else
						{
							ev = IO_EVENT_NONE;
						}
					}
					else
					{
						ev = IO_EVENT_EPOLLIN;
					}

					/* 继续接收数据,如果调用失败(一个本地错误)则同时触发ERR事件 */
					int drr = postRecv();
					if(drr != IO_ERROR_SUCESS)
					{
						setLastError(drr);
						SET_BIT(ev, IO_EVENT_EPOLLERR);
					}
				}
				else
				{
					/* 触发error事件 */
					ev = IO_EVENT_EPOLLHUP;
					setLastError(IO_ERROR_HUP);
				}
			}
			break;
		case IO_OPP_SEND:
			{
				if(oppResult && bytesTransfered > 0)
				{
					_sendOlp.ipos = bytesTransfered;

					if(_mode == IO_MODE_ET)
					{
						if(_sendOlp.et)
						{
							ev = IO_EVENT_EPOLLOUT;
							_sendOlp.et = false;
						}
						else
						{
							ev = IO_EVENT_NONE;
						}
					}
					else
					{
						ev = IO_EVENT_EPOLLOUT;
					}

					/* 继续发送 */
					int dsr = postSend();
					if(dsr != IO_ERROR_SUCESS)
					{
						/* 继续发送时如果发生本地错误,触发ERR事件, ERR事件和EPOLLOUT互斥 */
						/* 已经设置了shutdown标记,并且缓冲区内的数据已经全部发完,则触发ERROR事件使用户可以得到通知 */
						setLastError(dsr);
						ev = IO_EVENT_EPOLLERR;
					}
				}
				else
				{
					/* 触发error事件 */
					setLastError(IO_ERROR_HUP);
					ev = IO_EVENT_EPOLLHUP;
				}
			}
			break;
		default: break;
		}
	}

	return ev;
}

/*
To assure that all data is sent and received on a connected socket before it is closed, an application should use shutdown to close connection before calling closesocket. 
For example, to initiate a graceful disconnect:

	Call WSAAsyncSelect to register for FD_CLOSE notification. 
	Call shutdown with how=SD_SEND. 
	When FD_CLOSE received, call recv until zero returned, or SOCKET_ERROR. 
	Call closesocket. 
	Note  The shutdown function does not block regardless of the SO_LINGER setting on the socket.

	An application should not rely on being able to reuse a socket after it has been shut down. In particular, a Windows Sockets provider is not required to support the 
	use of connect on a socket that has been shut down.

* shutdown 语义: 如果发送缓冲内还有数据则等发送缓冲清空后触发一个 ERR 事件; 如果发送缓冲内没有数据则手动触发.
*/
int IOCPContext::shutdown(int how)
{
	/*
	* 首先设置一个shutdown标记,禁止后续的recv/send调用,然后
	* 1. 如果发送缓冲内还有数据,则等待发送缓冲发送完毕后触发一个IO_EVENT_ERROR事件
	* 2. 如果发送缓冲是空的,则返回IO_ERROR_SUCESS表示用户可以直接调用close()安全关闭而不用等待事件通知.
	* -- 有没有RECV IO在进行无关紧要,用户如果关心接收缓冲内的数据可在shutdown返回后调用recv得到.
	*/
	_shutdownFlag = how;

	// 判断发送缓冲是否为空,并且没有IO操作正在进行.
	if(_sendOlp.ipos == 0 && _sendOlp.upos == 0)
	{
		assert(_sendOlp.oppType == IO_OPP_NONE);

		/* 记录一个事件,下次添加到selector时返回 */
		setLastError(IO_ERROR_SHUTDOWN);
	}
	else
	{
		/* 发送缓冲发送完毕后自动触发一个 ERR 事件 */
		assert(_sendOlp.oppType == IO_OPP_SEND);
	}
	return IO_ERROR_SUCESS;
}

SOCKET IOCPContext::attach(SOCKET s)
{
	SOCKET oldSock = _sock;
	_sock = s;
	return oldSock;
}

SOCKET IOCPContext::detach()
{
	return attach(INVALID_SOCKET);
}

int IOCPContext::postAccept()
{
	assert(_sockType != SOCK_FILE);
	/* 有IO操作正在进行或者已经完成了一次连接但新连接的套接字没有被取走 */
	if(_recvOlp.oppType != IO_OPP_NONE || _newSock != INVALID_SOCKET)
	{
		return IO_ERROR_SUCESS;
	}

	if(!_acceptBuf)
	{
		_acceptBuf = new char[MIN_SOCKADDR_BUF_SIZE * 2];
	}

	if(!_lpfnAcceptEx)
	{
		/*
		* 获得AcceptEx()函数指针
		*/
		GUID GuidAcceptEx = WSAID_ACCEPTEX;
		DWORD dwBytes = 0;
		if( 0 != WSAIoctl(_sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx, sizeof(GuidAcceptEx), &_lpfnAcceptEx, sizeof(_lpfnAcceptEx), &dwBytes, NULL, NULL) )
		{
			assert(0);
			return IO_ERROR_ACCEPTFAILED;
		}
	}

	/*
	* 创建一个新的套接字(accept 调用的只能是TCP流套接字)
	*/
	_newSock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

	// 发起一个 IOCP AcceptEx 调用
	DWORD dwBytesReceived = 0;
	_recvOlp.oppType = IO_OPP_ACCEPT;
	if(!_lpfnAcceptEx(_sock, _newSock, _acceptBuf, 0, MIN_SOCKADDR_BUF_SIZE, MIN_SOCKADDR_BUF_SIZE, &dwBytesReceived, (OVERLAPPED*)&_recvOlp))
	{
		if(WSA_IO_PENDING != WSAGetLastError() && WSAECONNRESET != WSAGetLastError())
		{
			return IO_ERROR_ACCEPTFAILED;
		}
	}
	return IO_ERROR_SUCESS;
}

int IOCPContext::postConnect(const char *ipAddr, u_short port)
{
	assert(_sockType != SOCK_FILE);
	/*
	* 取得ConnectEx函数指针
	*/
	DWORD dwBytesReceived = 0;
	GUID GuidConnectEx = WSAID_CONNECTEX;
	LPFN_CONNECTEX lpfnConnectEx = NULL;
	DWORD dwBytes = 0;
	if( 0 != WSAIoctl(_sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidConnectEx, sizeof(GuidConnectEx), &lpfnConnectEx, sizeof(lpfnConnectEx), &dwBytes, NULL, NULL) )
	{
		assert(0);
		return IO_ERROR_CONNECTFAILED;
	}

	/*
	* 执行ConnectEx
	*/
	_sendOlp.oppType = IO_OPP_CONNECT;
	
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.S_un.S_addr = inet_addr(ipAddr);
	if( !lpfnConnectEx(_sock, (const sockaddr*)&addr, sizeof(sockaddr), NULL, 0, NULL, (LPOVERLAPPED)&_sendOlp) )
	{
		if(WSA_IO_PENDING != WSAGetLastError())
		{
			return IO_ERROR_CONNECTFAILED;
		}
	}

	return IO_ERROR_SUCESS;
}
/*
* recv io 提交的缓冲区总是从 [ipos -> end), 缓冲区内有效数据的范围是 [upos -> ipos) 无效数据(已经被user读取)范围是 [start, upos)
*/
int IOCPContext::postRecv()
{
	/* 设置了 shutdown 标记,直接返回成功 */
	if(_shutdownFlag == SD_RECEIVE || _shutdownFlag == SD_BOTH)
	{
		return IO_ERROR_SUCESS;
	}

	/* 有IO操作正在进行 */
	if(_recvOlp.oppType != IO_OPP_NONE)
	{
		assert(_recvOlp.oppType == IO_OPP_RECV);
		return IO_ERROR_SUCESS;
	}

	/*
	* 分配接收缓冲区
	*/
	if(!_recvOlp.buf)
	{
		_recvOlp.buf = new byte[_recvOlp.len];
		_recvOlp.ipos = 0;
		_recvOlp.upos = 0;
	}

	/*
	* 设置缓冲区,丢弃已经读取的部分
	*/
	memmove(_recvOlp.buf, _recvOlp.buf + _recvOlp.upos, _recvOlp.ipos - _recvOlp.upos);
	_recvOlp.ipos -= _recvOlp.upos;
	_recvOlp.upos = 0;
	
	if(_recvOlp.ipos >= _recvOlp.len)
	{
	}
	else
	{
		/*
		* 投递一个IO RECV 操作
		*/
		_recvOlp.oppType = IO_OPP_RECV;
		WSABUF wsaBuf = { _recvOlp.len - _recvOlp.ipos, (char*)_recvOlp.buf + _recvOlp.ipos };
		DWORD dwFlags = 0;
		DWORD dwTransfered = 0;

		if(_sockType != SOCK_FILE)
		{
			if(SOCKET_ERROR == WSARecv(_sock, &wsaBuf, 1, &dwTransfered, &dwFlags, (LPOVERLAPPED)&_recvOlp, NULL))
			{
				if(WSAGetLastError() != WSA_IO_PENDING)
				{
					return IO_ERROR_RECVFAILED;
				}
			}
		}
		else
		{
			if(!ReadFile((HANDLE)_sock, wsaBuf.buf, wsaBuf.len, &dwTransfered, (LPOVERLAPPED)&_recvOlp))
			{
				if(GetLastError() != ERROR_IO_PENDING)
				{
					return IO_ERROR_RECVFAILED;
				}
			}
		}
	}
	return IO_ERROR_SUCESS;
}

/*
* send io 缓冲区分布, 提交部分 [start -> ipos), 已写未提交部分 [ipos -> upos), 空余部分 [upos -> end)
*/
int IOCPContext::postSend()
{
	/* 如果设置了关闭标志也要继续发送已经拷贝到缓冲区内的数据 */
	/* 有IO操作正在进行 */
	if(_sendOlp.oppType != IO_OPP_NONE)
	{
		assert(IO_OPP_SEND == _sendOlp.oppType);
		return IO_ERROR_SUCESS;
	}

	/* 设置发送缓冲区,把已经发送成功的数据丢弃 */
	assert(_sendOlp.buf);
	memmove(_sendOlp.buf, _sendOlp.buf + _sendOlp.ipos, _sendOlp.upos - _sendOlp.ipos);
	_sendOlp.upos -= _sendOlp.ipos;
	_sendOlp.ipos = _sendOlp.upos;

	if(_sendOlp.ipos <= 0)
	{
		/* 如果设置了shutdown标记,当发送缓冲区全部发送完毕后返回 SHUTDOWN */
		if(_shutdownFlag == SD_BOTH || _shutdownFlag == SD_SEND)
		{
			return IO_ERROR_SHUTDOWN;
		}
		else
		{
			return IO_ERROR_SUCESS;
		}
	}
	else
	{
		/*
		* 投递一个IO SEND 操作
		*/	
		_sendOlp.oppType = IO_OPP_SEND;
		DWORD dwTransfered = 0;
		DWORD dwLastError = 0;
		WSABUF wsaBuf = { _sendOlp.ipos, (char*)_sendOlp.buf };

		if(_sockType != SOCK_FILE)
		{
			if(SOCKET_ERROR == WSASend(_sock, &wsaBuf, 1, &dwTransfered, 0, (LPOVERLAPPED)&_sendOlp, NULL))
			{
				if(WSAGetLastError() != WSA_IO_PENDING)
				{
					return IO_ERROR_SENDFAILED;
				}
			}
		}
		else
		{
			if(!WriteFile((HANDLE)_sock, wsaBuf.buf, wsaBuf.len, &dwTransfered, (LPOVERLAPPED)&_sendOlp))
			{
				if(GetLastError() != ERROR_IO_PENDING)
				{
					return IO_ERROR_SENDFAILED;
				}
			}
		}

		return IO_ERROR_SUCESS;
	}
}

SOCKET IOCPContext::accept()
{
	assert(_sockType != SOCK_FILE);
	SOCKET newSock = INVALID_SOCKET;

	lock();
	if(_recvOlp.oppType == IO_OPP_NONE)
	{
		// AccpetEx调用已经完成,把新连接返回
		newSock = _newSock;
		_newSock = INVALID_SOCKET;

		// 发起下一个IO
		setLastError(postAccept());
	}
	else
	{
	}
	unlock();

	return newSock;
}

int IOCPContext::connect(const char* ipAddr, u_short port)
{
	assert(_sockType != SOCK_FILE);

	/* 加锁,防止用户重复调用 */
	int ret = IO_ERROR_UNDEFINED;
	lock();
	ret = postConnect(ipAddr, port);
	unlock();
	return ret;
}

int IOCPContext::recv(void* buf, size_t len)
{
	// 复制缓冲区内的数据
	lock();
	if(len > _recvOlp.ipos - _recvOlp.upos) len = _recvOlp.ipos - _recvOlp.upos;
	if(len > 0)
	{
		if(buf)
		{
			memcpy(buf, _recvOlp.buf + _recvOlp.upos, len);
		}
		_recvOlp.upos += len;
	}
	else
	{
		/* 边缘触发标记 */
		_recvOlp.et = true;
	}

	// 发起一个RECV IO
	setLastError(postRecv());
	unlock();

	return len;
}

int IOCPContext::send(const void* buf, size_t len)
{
	/* 如果已经设置了shutdown标记,则不允许再写缓冲 */
	if(SD_BOTH == _shutdownFlag || SD_SEND == _shutdownFlag)
	{
		return IO_ERROR_SHUTDOWN;
	}

	lock();

	/* 分配发送缓冲  */
	if(!_sendOlp.buf)
	{
		_sendOlp.buf = new byte[_sendOlp.len];
		_sendOlp.ipos = 0;
		_sendOlp.upos = 0;
	}

	// 写入缓冲区
	if(len > _sendOlp.len - _sendOlp.upos) len = _sendOlp.len - _sendOlp.upos;
	memcpy(_sendOlp.buf + _sendOlp.upos, buf, len);
	_sendOlp.upos += len;

	if(_sendOlp.upos >= _sendOlp.len)
	{
		_sendOlp.et = true;
	}

	// 发起一个 send IO
	setLastError(postSend());

	unlock();
	return len;
}

// 返回读缓冲或者写缓冲的长度
size_t IOCPContext::size(bool r)
{
	size_t s = 0;
	lock();
	if(r)
	{
		s =  _recvOlp.ipos - _recvOlp.upos;
	}
	else
	{
		s = _sendOlp.len - _sendOlp.upos;
	}
	unlock();
	return s;
}

size_t IOCPContext::_pump(reader_t reader, size_t maxlen)
{
	/* 如果已经设置了shutdown标记,则不允许再写缓冲 */
	if(SD_BOTH == _shutdownFlag || SD_SEND == _shutdownFlag)
	{
		return 0;
	}

	lock();

	/* 分配发送缓冲  */
	if(!_sendOlp.buf)
	{
		_sendOlp.buf = new byte[_sendOlp.len];
		_sendOlp.ipos = 0;
		_sendOlp.upos = 0;
	}

	// 写入缓冲区
	size_t len = maxlen;
	if(len > _sendOlp.len - _sendOlp.upos) len = _sendOlp.len - _sendOlp.upos;
	len = reader(_sendOlp.buf + _sendOlp.upos, len);
	_sendOlp.upos += len;

	if(_sendOlp.upos >= _sendOlp.len)
	{
		_sendOlp.et = true;
	}

	// 发起一个 send IO
	setLastError(postSend());

	unlock();
	return len;
}