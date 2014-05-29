#include "StdAfx.h"
//#include "Uri.h"
#include "Lock.h"
#include "FCGIRecord.h"
#include "FCGIResponder.h"


FCGIResponder::FCGIResponder(IHTTPServer *server, IRequest* request, bool cacheAll)
	: _server(server), _request(request), _cacheAll(cacheAll), _svrCode(SC_UNKNOWN), _fcgiAdp(NULL), _httpAdp(NULL),
	_bytesHTTPSent(0), _bytesHTTPRecv(0), _bytesFCGISent(0), _bytesFCGIRecv(0),
	_fcgiBufferPipe(NULL), _cacheFilePipe(NULL)
{
	_lock = new Lock();
}

FCGIResponder::~FCGIResponder()
{
	if(_lock) delete _lock;
	if(_fcgiBufferPipe) delete _fcgiBufferPipe;
	if(_cacheFilePipe) delete _cacheFilePipe;
}

void FCGIResponder::lock()
{
	if(_lock) _lock->lock();
}

void FCGIResponder::unlock()
{
	if(_lock) _lock->unlock();
}

IOAdapter* FCGIResponder::setFCGIConnection(u_short fcgiId, IOAdapter* adp)
{
	IOAdapter* oldAdp = _fcgiAdp;
	_fcgiAdp = adp;
	_fcgiId = fcgiId;
	return oldAdp;
}

IRequest* FCGIResponder::getRequest()
{
	return _request;
}

std::string FCGIResponder::getHeader()
{
	return _header.getHeader();
}

int FCGIResponder::getServerCode()
{
	return _svrCode;
}

void FCGIResponder::statistics(__int64* bytesRecved, __int64* bytesSent)
{
	*bytesRecved = _bytesHTTPRecv;
	*bytesSent = _bytesHTTPSent;
}


void FCGIResponder::initFCGIEnv()
{
	assert(_fcgiBufferPipe);
	
	/* 
	* 准备参数 
	*/
	std::string tmp;
	char numberBuf[50] = {0};

	/* 分析uri信息 */
	std::string uri = _request->uri(true);
	std::string uriPath(""), uriQueryString("");
	std::string uriServerPath;
	std::string::size_type pos = uri.find('?');
	if(pos == std::string::npos)
	{
		uriPath = uri;
	}
	else
	{
		uriPath = uri.substr(0, pos);
		uriQueryString = uri.substr(pos + 1, uri.size() - pos - 1);
	}

	if(!_server->mapServerFilePath(uriPath, uriServerPath))
	{
		assert(0);
	}

	/* 获取连接地址 */
	char serverIP[MAX_IP_LENGTH] = {0}, clientIP[MAX_IP_LENGTH] = {0};
	u_short serverPort = 0, clientPort = 0;
	_httpAdp->getsockname(serverIP, serverPort);
	_httpAdp->getpeername(clientIP, clientPort);

	/* HTTP方法 */
	char method[50] = {0};
	map_method(_request->method(), method);

	/* SERVER_NAME - HOST */
	std::string hostName = _request->field("Host");
	if( hostName.size() <= 0)
	{
		hostName = _server->ip();
	}
	else
	{
		std::string::size_type pos = hostName.find(':');
		if(pos != std::string::npos)
		{
			hostName = hostName.substr(0, pos);
		}
	}

	/* 
	* 准备缓冲区 
	*/
	FCGIRecord record;

	/* 发送 FCGI_BEGIN_REQUEST */
	record.setHeader(_fcgiId, FCGI_BEGIN_REQUEST);
	record.setBeginRequestBody(FCGI_RESPONDER, true);
	record.setEnd();
	_fcgiBufferPipe->write(record.buffer(), record.size());
	record.reset();

	/* 发送参数 */
	record.setHeader(_fcgiId, FCGI_PARAMS);
	record.addNameValuePair("HTTPS", "off");
	record.addNameValuePair("REDIRECT_STATUS", "200");
	record.addNameValuePair("SERVER_PROTOCOL", "HTTP/1.1");
	record.addNameValuePair("GATEWAY_INTERFACE", "CGI/1.1");
	record.addNameValuePair("SERVER_SOFTWARE", SERVER_SOFTWARE);
	record.addNameValuePair("SERVER_NAME", hostName.c_str());
	record.addNameValuePair("SERVER_ADDR", serverIP);
	record.addNameValuePair("SERVER_PORT", itoa(serverPort, numberBuf, 10));
	record.addNameValuePair("REMOTE_ADDR", clientIP);
	record.addNameValuePair("REMOTE_PORT", itoa(clientPort, numberBuf, 10));
	record.addNameValuePair("REQUEST_METHOD", method);
	record.addNameValuePair("REQUEST_URI", uri.c_str());
	if(uriQueryString.size() > 0)
	{
		record.addNameValuePair("QUERY_STRING",uriQueryString.c_str());
	}

	record.addNameValuePair("DOCUMENT_ROOT", _server->docRoot().c_str());
	record.addNameValuePair("SCRIPT_NAME", uriPath.c_str());
	record.addNameValuePair("SCRIPT_FILENAME", uriServerPath.c_str());
	record.addNameValuePair("HTTP_HOST", _request->field("Host").c_str());
	record.addNameValuePair("HTTP_USER_AGENT", _request->field("User-Agent").c_str());
	record.addNameValuePair("HTTP_ACCEPT", _request->field("Accept").c_str());
	record.addNameValuePair("HTTP_ACCEPT_LANGUAGE", _request->field("Accept-Language").c_str());
	record.addNameValuePair("HTTP_ACCEPT_ENCODING", _request->field("Accept-Encoding").c_str());

	tmp = _request->field("Cookie");
	if(tmp.size() > 0)
	{
		record.addNameValuePair("HTTP_COOKIE", tmp.c_str());
	}
	
	tmp = _request->field("Referer");
	if(tmp.size() > 0)
	{
		record.addNameValuePair("HTTP_REFERER", tmp.c_str());
	}

	tmp = _request->field("Content-Type");
	if(tmp.size() > 0)
	{
		record.addNameValuePair("CONTENT_TYPE", tmp.c_str());
	}

	tmp = _request->field("Content-Length");
	if(tmp.size() > 0)
	{
		record.addNameValuePair("CONTENT_LENGTH", tmp.c_str());
	}
	record.setEnd();
	_fcgiBufferPipe->write(record.buffer(), record.size());
	record.reset();

	/* 空记录表示结束 */
	record.setHeader(_fcgiId, FCGI_PARAMS);
	record.setEnd();
	_fcgiBufferPipe->write(record.buffer(), record.size());
	record.reset();
}

// 初始化 FCGI 环境变量
bool FCGIResponder::step0(IOAdapter* adp, int ev, stm_result_t* res)
{
	lock();
	if(adp != _fcgiAdp)
	{
		assert(_httpAdp == NULL);
		// HTTP连接必须等到 FCGI 连接以及准备就绪之后才可用
		_httpAdp = adp;
		res->rc = STM_PAUSE;
	}
	else
	{
		/*
		* 初始化 FCGI 连接
		*/
		// 一个 FCGIRecord 最大长度是2个字节,即 65535
		_fcgiBufferPipe = new BufferPipe(65535, 4 * K_BYTES);
		initFCGIEnv();

		// 创建一个 FCGIRecord 变换器,把上一个管道的数据输出成 一个个 FCGIRecord 格式的数据包流.
		_fcgiTransformer = new FCGITransformer(FCGI_STDIN, _fcgiId, 4096);

		// 连接管道
		if(_request->getPostData())
		{
			build_pipes_chain(_request->getPostData(), (IPipe*)_fcgiTransformer, (IPipe*)_fcgiBufferPipe, (IPipe*)_fcgiAdp);
		}
		else
		{
			build_pipes_chain((IPipe*)_fcgiTransformer, (IPipe*)_fcgiBufferPipe, (IPipe*)_fcgiAdp);
		}

		// 开始发送到 FCGI 服务器
		res->rc = STM_CONTINUE;
		res->param.epollEvent = IO_EVENT_EPOLLOUT;
		forward();
	}
	unlock();
	return false;
}

// FCGIResponder -> FCGI Server: 发送 FCGI 环境变量, HTTP POST
bool FCGIResponder::step1(IOAdapter* adp, int ev, stm_result_t* res)
{
	// 发送完环境变量后,如果有 HTTP POST 数据,继续发送之.
	lock();
	
	if(adp != _fcgiAdp)
	{
		assert(_httpAdp == NULL);
		_httpAdp = adp;
		res->rc = STM_PAUSE;
	}
	else
	{
		if(_fcgiAdp->pump() == 0)
		{
			// 开始接收 FCGI server 的响应数据
			forward();
			res->rc = STM_CONTINUE;
			res->param.epollEvent = IO_EVENT_EPOLLIN;
		}
		else
		{
			// 继续发送到 FCGI Server
			res->rc = STM_CONTINUE;
			res->param.epollEvent = IO_EVENT_EPOLLOUT;
		}
	}

	unlock();
	return false;
}

// FCGI Server -> FCGIResponder:正式接收内容之前,准备发送到HTTP客户端的响应头
bool FCGIResponder::step2(IOAdapter* adp, int ev, stm_result_t* res)
{
	lock();
	unlock();
	return false;
}

// FCGI Server -> FCGIResponder: 正式接收内容
// FCGI Responder -> HTTP Client
bool FCGIResponder::step3(IOAdapter* adp, int ev, stm_result_t* res)
{
	lock();
	unlock();
	return false;
}

// 结束
bool FCGIResponder::step4(IOAdapter* adp, int ev, stm_result_t* res)
{
	lock();
	unlock();
	return false;
}