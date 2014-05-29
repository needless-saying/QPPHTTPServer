#include "StdAfx.h"
#include "HTTPConnection.h"
#include "HTTPRequest.h"

HTTPConnection::HTTPConnection(IHTTPServer* svr, IOAdapter* adp)
	: _hrt(true), _svr(svr), _request(NULL), _responder(NULL), _factory(NULL), _adp(adp)
{
	adp->getpeername(_remoteIPAddr, _remotePort);
}

HTTPConnection::~HTTPConnection()
{
	if(_request) delete _request;
	if(_responder) _factory->releaseResponder(_responder);
}

const char* HTTPConnection::getRemoteIPAddr()
{
	return _remoteIPAddr;
}

u_short HTTPConnection::getRemotePort()
{
	return _remotePort;
}

IRequest* HTTPConnection::getRequest()
{
	return _request;
}

IResponder* HTTPConnection::getResponder()
{
	return _responder;
}

void HTTPConnection::requestBegin()
{
	_reqCount++;
	_reqStartTime = _hrt.now();
	_reqEndTime = _reqStartTime;

	_reqRecvBytes = 0;
	_reqSentBytes = 0;
}

void HTTPConnection::requestEnd()
{
	_reqEndTime = _hrt.now();

	__int64 recved = 0, sent = 0;
	_request->statistics(&recved, &sent);
	_reqRecvBytes += recved;
	_reqSentBytes += sent;

	if(_responder)
	{
		_responder->statistics(&recved, &sent);
		_reqRecvBytes += recved;
		_reqSentBytes += sent;
	}

	_connRecvBytes += _reqRecvBytes;
	_connSentBytes += _reqSentBytes;
}

void HTTPConnection::connectionBegin()
{
	_reqCount = 0;
	_connStartTime = _hrt.now();
	_connEndTime = _connStartTime;

	_connRecvBytes = 0;
	_connSentBytes = 0;
}

void HTTPConnection::connectionEnd()
{
	_connEndTime = _hrt.now();
}

bool HTTPConnection::beforeStep(IOAdapter* adp, int ev, stm_result_t* res)
{
	return true;
}


bool HTTPConnection::step0(IOAdapter* adp, int ev, stm_result_t* res)
{
	// 记录连接开始的时间
	connectionBegin();

	// 创建一个 HTTPRequest,准备运行
	_request = new HTTPRequest(_svr);

	// 开始处理一次HTTP请求,记录请求开始的时间
	requestBegin();

	forward();
	res->rc = STM_CONTINUE;
	res->param.epollEvent = IO_EVENT_EPOLLIN;
	return false;
}

bool HTTPConnection::step1(IOAdapter* adp, int ev, stm_result_t* res)
{
	// HTTPRequest 运行
	assert(_request);
	_request->run(adp, ev, res);

	if(STM_DONE == res->rc)
	{
		// HTTP请求接收完毕,调用 IHTTPServer 的方法生成一个 HTTP响应报文.
		assert(_responder == NULL);
		_svr->catchRequest(_request, &_responder, &_factory);
		assert(_responder);

		// 返回 STEPDONE 给SERVER提供一个处理请求开始的时机
		res->st = 1;
		res->rc = STM_STEPDONE;
		res->param.epollEvent = IO_EVENT_EPOLLOUT;
		forward();
	}
	else if(STM_CONTINUE == res->rc)
	{
	}
	else if(STM_SLEEP == res->rc)
	{
	}
	else if(STM_ABORT == res->rc)
	{
		requestEnd();
		connectionEnd();

		// 终止,连接关闭
	}
	else if(STM_STEPDONE ==res->rc)
	{
		assert(0);
	}
	else
	{
		assert(0);
	}
	return false;
}

bool HTTPConnection::step2(IOAdapter* adp, int ev, stm_result_t* res)
{
	// IResponder 运行
	assert(_responder);
	_responder->run(adp, ev, res);

	if(STM_DONE == res->rc)
	{
		requestEnd();

		// 返回STEPDONE,SERVER可以处理一个请求处理完毕的时机.
		res->rc = STM_STEPDONE;
		res->param.epollEvent = IO_EVENT_EPOLLOUT;
		res->st = 2;
		forward();
	}
	else if(STM_CONTINUE == res->rc)
	{
	}
	else if(STM_SLEEP == res->rc)
	{
	}
	else if(STM_ABORT == res->rc)
	{
		requestEnd();
		connectionEnd();

		// 终止,连接关闭
	}
	else if(STM_STEPDONE ==res->rc)
	{
	}
	else
	{
		assert(0);
	}
	return false;
}

bool HTTPConnection::step3(IOAdapter* adp, int ev, stm_result_t* res)
{
	// 一次请求处理完毕
	if(_request->keepAlive())
	{
		_factory->releaseResponder(_responder);
		_factory = NULL;
		_responder = NULL;

		// 继续接收下一个请求头(状态机回退2步到step1)
		_request->nextRequest();

		// 开始处理一次HTTP请求,记录请求开始的时间
		requestBegin();

		res->rc = STM_CONTINUE;
		res->param.epollEvent = IO_EVENT_EPOLLIN;
		
		backward(2);
	}
	else
	{
		// 关闭套接字,等待shutdown事件确认发送缓冲区内的数据已经全部发送完毕.
		adp->shutdown(SD_BOTH);

		res->rc = STM_CONTINUE;
		res->param.epollEvent = IO_EVENT_EPOLLERR;

		forward();
	}
	return false;
}

bool HTTPConnection::step4(IOAdapter* adp, int ev, stm_result_t* res)
{
	// 关闭连接
	connectionEnd();

	if(TEST_BIT(ev, IO_EVENT_EPOLLERR) && adp->getLastError() == IO_ERROR_SHUTDOWN)
	{
		res->rc = STM_DONE;
	}
	else
	{
		res->rc = STM_ABORT;
		res->param.errorCode = TEST_BIT(ev, IO_EVENT_EPOLLERR) ? SE_NETWORKFAILD : SE_REMOTEFAILD;
	}
	return false;
}

void HTTPConnection::queryLastRequest(request_statis_t* info)
{
	assert(_request);
	info->uri = _request ? _request->uri(true) : "{null}";
	info->method = _request ? _request->method() : METHOD_UNDEFINE;
	info->sc = _responder ? _responder->getServerCode() : SC_UNKNOWN;
	info->usedTime = (size_t)_hrt.getMs(_reqEndTime - _reqStartTime);
	info->bytesSent = _reqSentBytes;
	info->bytesRecved = _reqRecvBytes;
}

void HTTPConnection::query(connection_statis_t* info)
{
	info->requestCount = _reqCount;
	info->usedTime = (size_t)_hrt.getMs(_connEndTime - _connStartTime);
	info->bytesRecved = _connRecvBytes;
	info->bytesSent = _connSentBytes;
}