#include "stdafx.h"
#include "IOCP.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////
IOCPAdapter::IOCPAdapter(int t)
	: _ctx(NULL), _sockType(t), _newAdp(NULL)
{
	/*
	* 创建一个新的套接字.
	*/
	_s = WSASocket(AF_INET, t, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if( INVALID_SOCKET == _s )
	{
		assert(0);
	}
}

IOCPAdapter::IOCPAdapter(SOCKET s)
	: _ctx(NULL), _newAdp(NULL), _s(s)
{
	/* 获取套接字类型 */
	int len = sizeof(_sockType);
	getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&_sockType, &len);
}

IOCPAdapter::IOCPAdapter(const char* filename)
	: _ctx(NULL), _sockType(SOCK_FILE), _newAdp(NULL)
{
	/*
	* 创建一个文件句柄连接命名管道
	*/
	HANDLE h = CreateFileA(filename,
		GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		NULL);
	if( INVALID_HANDLE_VALUE == h )
	{
		assert(0);
		_s = INVALID_SOCKET;
	}
	else
	{
		_s = (SOCKET)h;
	}
}

IOCPAdapter::~IOCPAdapter()
{
	if(_s != INVALID_SOCKET)
	{
		if(SOCK_FILE != _sockType)
		{
			closesocket(_s);
		}
		else
		{
			CloseHandle((HANDLE)_s);
		}
	}
	assert(_ctx == NULL);

	if(_newAdp)
	{
		delete _newAdp;
	}
}

IOCPContext* IOCPAdapter::attach(IOCPContext* ctx)
{
	IOCPContext* oldCtx = _ctx;
	_ctx = ctx;
	return oldCtx;
}

IOCPContext* IOCPAdapter::detach()
{
	return attach(NULL);
}

IOCPContext* IOCPAdapter::getContext()
{
	return _ctx;
}

int IOCPAdapter::getLastError()
{
	if(_ctx) return _ctx->getLastError();
	return IO_ERROR_UNDEFINED;
}

int IOCPAdapter::bind(const char* ipAddr, u_short port)
{
	assert(_sockType != SOCK_FILE);
	assert(_s != INVALID_SOCKET);
	sockaddr_in addr;
	addr.sin_family	= AF_INET;
	addr.sin_port = htons(port);
	if(NULL == ipAddr || strlen(ipAddr) == 0 )
	{
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		addr.sin_addr.s_addr = inet_addr(ipAddr);
	}

	if(0 == ::bind(getSock(), (sockaddr *)&addr, sizeof(sockaddr_in)))
	{
		// 如果是UDP套接字,bind成功即认为已经"连接"
		return IO_ERROR_SUCESS;
	}
	else
	{
		//  不记录 lastError 因为可以换一个参数重试
		return IO_ERROR_BINDFAILED;
	}
}

int IOCPAdapter::shutdown(int how)
{
	if(_ctx) return _ctx->shutdown(how);
	return ::shutdown(getSock(), how);
}

int IOCPAdapter::setopt(int optname, const char* optval, int optlen)
{
	assert(_sockType != SOCK_FILE);
	return IO_ERROR_SUCESS;
}

int IOCPAdapter::getopt(int optname, char* optval, int* optlen)
{
	assert(_sockType != SOCK_FILE);
	return IO_ERROR_SUCESS;
}

/* 状态查询接口 */
int IOCPAdapter::query(__int64* bytesReceived, __int64* bytesSent)
{
	
	return IO_ERROR_SUCESS;
}

int IOCPAdapter::getsockname(char *ipAddr, u_short &port)
{
	assert(_sockType != SOCK_FILE);
	sockaddr_in a;
	int len = sizeof(a);
	int ret = ::getsockname(getSock(), (sockaddr*)&a, &len);

	strcpy(ipAddr, inet_ntoa(a.sin_addr));
	port = ntohs(a.sin_port);

	return ret;
}

int IOCPAdapter::getpeername(char* ipAddr, u_short &port)
{
	assert(_sockType != SOCK_FILE);
	sockaddr_in a;
	int len = sizeof(a);
	int ret = ::getpeername(getSock(), (sockaddr*)&a, &len);

	strcpy(ipAddr, inet_ntoa(a.sin_addr));
	port = ntohs(a.sin_port);

	return ret;
}

int IOCPAdapter::connect(const char *ipAddr, u_short port)
{
	assert(_sockType != SOCK_FILE);
	if(_ctx) return _ctx->connect(ipAddr, port);
	return IO_ERROR_UNDEFINED;
}

int IOCPAdapter::listen(int backlog /* = SOMAXCONN */)
{
	assert(_sockType != SOCK_FILE);
	assert(_s != INVALID_SOCKET);
	int ret = IO_ERROR_SUCESS;
	if(0 == ::listen(_s, backlog))
	{
	}
	else
	{
		ret = IO_ERROR_LISTENFAILED;
	}
	return ret;
}

/*
* 接受一个新连接
* 返回值表示正在进行的AcceptEx的状态,已经完成的调用由 newAdp 参数传达.
*/
IOAdapter* IOCPAdapter::accept()
{
	assert(_sockType != SOCK_FILE);
	IOCPAdapter* newAdp = NULL;
	if(_ctx)
	{
		SOCKET newsock = _ctx->accept();
		if(newsock != INVALID_SOCKET)
		{
			newAdp = new IOCPAdapter(newsock);
		}
	}
	return newAdp;
}

int IOCPAdapter::recv(void *buf, size_t len)
{
	if(_ctx) return _ctx->recv(buf, len);
	return 0;
}

int IOCPAdapter::send(const void *buf, size_t len)
{
	if(_ctx) return _ctx->send(buf, len);
	return 0;
}

#ifdef DERIVE_FROM_PIPE

size_t IOCPAdapter::_read(void* buf, size_t len)
{
	if(_ctx) return _ctx->recv(buf, len);
	return 0;
}

size_t IOCPAdapter::_pump(reader_t reader, size_t maxlen)
{
	if(_ctx) return _ctx->_pump(reader, maxlen);
	return 0;
}

__int64 IOCPAdapter::_size()
{
	if(_ctx) return _ctx->size(true);
	return 0;
}

#endif

IOAdapter* IO_create_adapter()
{
	return new IOCPAdapter(SOCK_STREAM);
}

IOAdapter* IO_create_adapter(const char* filename)
{
	return new IOCPAdapter(filename);
}

int IO_destroy_adapter(IOAdapter* adp)
{
	IOCPAdapter *iocpAdp = dynamic_cast<IOCPAdapter*>(adp);
	delete iocpAdp;

	return IO_ERROR_SUCESS;
}