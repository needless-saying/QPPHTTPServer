#include "StdAfx.h"
//#include "Uri.h"
#include "Lock.h"
#include "FCGIRecord.h"
#include "FCGIResponder.h"


FCGIResponder::FCGIResponder(IHTTPServer *server, IRequest* request, bool cacheAll)
	: _server(server), _request(request), _cacheAll(cacheAll), _svrCode(SC_UNKNOWN), _fcgiAdp(NULL), _httpAdp(NULL),
	_bytesHTTPSent(0), _bytesHTTPRecv(0), _bytesFCGISent(0), _bytesFCGIRecv(0),
	_recordPipe(NULL), _cacheFilePipe(NULL)
{
	_lock = new Lock();
}

FCGIResponder::~FCGIResponder()
{
	if(_lock) delete _lock;
	if(_recordPipe) delete _recordPipe;
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


void FCGIResponder::prepareFCGIInput()
{
	assert(_recordPipe == NULL);
	_recordPipe = new BufferPipe(65535, 4 * K_BYTES);

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
	_recordPipe->write(record.buffer(), record.size());
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
	_recordPipe->write(record.buffer(), record.size());
	record.reset();

	/* 空记录表示结束 */
	record.setHeader(_fcgiId, FCGI_PARAMS);
	record.setEnd();
	_recordPipe->write(record.buffer(), record.size());
	record.reset();

	// 连接管道
	if(_request->getPostData())
	{
		// 创建一个 FCGIRecord 变换器,把上一个管道的数据输出成 一个个 FCGIRecord 格式的数据包流.
		_recordTransformer = new FCGITransformer(FCGI_STDIN, _fcgiId, 4096);
		build_pipes_chain(_request->getPostData(), (IPipe*)_recordTransformer, (IPipe*)_recordPipe, (IPipe*)_fcgiAdp);
	}
	else
	{
		// 添加一个空 STDIN 
		record.setHeader(_fcgiId, FCGI_STDIN);
		record.setEnd();
		_recordPipe->write(record.buffer(), record.size());
		record.reset();
		build_pipes_chain((IPipe*)_recordPipe, (IPipe*)_fcgiAdp);
	}
}

// 处理 FCGI 服务器发送来的数据
void FCGIResponder::disposeRecord(FCGIRecord* record)
{
	u_char t = record->getType();

	// 分类处理
	if(FCGI_STDOUT == t)
	{
		// 数据写入缓存(_cacheFilePipe,_recordPipe 已经连接,所以只对 _cacheFilePipe 写入即可)
		size_t wr = _cacheFilePipe->write(record->getBodyData(), record->getBodyLength());
		assert(wr == record->getBodyLength());

		/* 启动 HTTP Client Adp 发送数据到 HTTP 客户端 */
		if(!_cacheAll)
		{
			tryHttpClientPump();
		}
	}
	else if(FCGI_END_REQUEST == t)
	{
		/* 请求结束 */
		unsigned int appStatus = 0;
		byte protocolStatus = 0;
		if(record->getEndRequestBody(appStatus, protocolStatus))
		{
			assert(protocolStatus == FCGI_REQUEST_COMPLETE);
		}
		else
		{
			assert(0);
		}

		_fcgiRequestEnd = true;

		/// 发送数据到 HTTP 客户端
		tryHttpClientPump();
	}
	else if(FCGI_STDERR == t)
	{
		/* 写日志 */
		std::string err;
		err.assign((const char*)record->getBodyData(), record->getBodyLength());
		LOGGER_CWARNING(theLogger, _T("%s\r\n"), AtoT(err).c_str());
	}
	else
	{
		/* 忽略 */
		assert(0);
	}
}

void FCGIResponder::tryHttpClientPump()
{
	// 准备 HTTP 响应头(根据需要判断是否要覆盖 FCGI 服务器填写的响应头)
	if(!_headerFormatted)
	{
		/* FCGI 服务器一定会发送响应头吗? */
		/* 分析由 FCGI 进程产生的响应头的几个特殊域: Status, Content-Length, Transfer-Encoding */
		int headerlen = http_header_end((const char*)_cacheBufferPipe->buffer(), (size_t)_cacheBufferPipe->size());
		if(headerlen > 0)
		{
			_cacheBufferPipe->skip(headerlen);

			/* 生成一个 HTTP 响应头 */
			_svrCode = SC_OK;
			_header.setResponseCode(_svrCode);
			_header.addDefaultFields();
			_header.add(std::string((const char*)_recordPipe->buffer(), headerlen));

			/* 分析由 FCGI 进程产生的响应头的几个特殊域: Status, Content-Length, Transfer-Encoding */
			std::string tmp;
			if(_header.getField("Status", tmp))
			{
				// FCGI Status 域指明新的响应码
				_svrCode = atoi(tmp.c_str());
				if( _svrCode == 0) _svrCode = SC_OK;
				_header.setResponseCode(_svrCode);
				_header.remove("Status");
			}

			int contentLen = 0;
			if(_header.getField("Content-Length", tmp))
			{
				// FCGI 进程指明了内容的长度
				contentLen = atoi(tmp.c_str());
			}
			else
			{
				if(_fcgiRequestEnd)
				{
					char clstr[100] = {0};
					contentLen = (size_t)_cacheFilePipe->size() + (size_t)_cacheBufferPipe->size();
					sprintf(clstr, "%d", contentLen);
					_header.add("Content-Length", clstr);
				}
			}

			if(!_header.getField("Transfer-Encoding", tmp) && contentLen == 0)
			{
				// FCGI 服务器没有指定长度,并且没有指定 Transfer-Encoding 则使用 chunked 编码.
				_header.add("Transfer-Encoding", "chunked");
				_chunkTransformer = new ChunkTransformer(4 * K_BYTES);
			}

			/* 是否保持连接 */
			if(_request->keepAlive())
			{
				_header.add("Connection", "keep-alive");
			}
			else
			{
				_header.add("Connection", "close");
			}

			/* 格式化响应头准备输出 */
			_header.format();
			_headerFormatted = true;

			// 连接管道
			if(_chunkTransformer)
			{
				build_pipes_chain((IPipe*)_cacheFilePipe, (IPipe*)_cacheBufferPipe, (IPipe*)_chunkTransformer, (IPipe*)&_header);
			}
			else
			{
				build_pipes_chain((IPipe*)_cacheFilePipe, (IPipe*)_cacheBufferPipe, (IPipe*)&_header);
			}
		}
	}

	// 从 _cacheFilePipe -> _cacheBufferPipe -> _header 的管道链中抽取数据
	_httpAdp->pump(0, &_header);
}

bool FCGIResponder::beforeStep(IOAdapter* adp, int ev, stm_result_t* res)
{
	if(TEST_BIT(ev, IO_EVENT_EPOLLERR))
	{
		res->rc = STM_ABORT;
		res->param.errorCode = SE_NETWORKFAILD;
		return false;
	}

	if(TEST_BIT(ev, IO_EVENT_EPOLLHUP))
	{
		res->rc = STM_ABORT;
		res->param.errorCode = SE_REMOTEFAILD;
		return false;
	}
	return true;
}

// 初始化 FCGI 环境变量
bool FCGIResponder::step0(IOAdapter* adp, int ev, stm_result_t* res)
{
	lock();
	if(adp != _fcgiAdp)
	{
		// HTTP连接必须等到 FCGI 连接以及准备就绪之后才可用
		_httpAdp = adp;
		res->rc = STM_PAUSE;
	}
	else
	{
		/*
		* 填写 FCGI 参数,创建输入管道
		*/
		prepareFCGIInput();

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
			assert(_recordPipe->size() == 0);

			// 构建输入管道
			build_pipes_chain((IPipe*)_fcgiAdp, (IPipe*)_recordPipe);

			// 构建输出缓存
			// 不能直接把输出缓存连接到输入管道的原因是 
			/// 1. FCGI 服务器发送来的数据是以 FCGI Record 格式,需要解析(可以通过添加一个变换器解决这个问题); 
			/// 2. 更重要的是不是所有数据都发送到客户端,从逻辑上来说不直接连接比较好一些.
			build_pipes_chain((IPipe*)_cacheFilePipe, (IPipe*)_cacheBufferPipe);

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

	if(adp != _fcgiAdp)
	{
		// 尝试从缓存内读取数据发送到 HTTP 客户端
		tryHttpClientPump();
	}
	else
	{
		if(0 == _recordPipe->pump())
		{
			assert(0);
			
			/// 无法从 fcgiAdp 中读取数据,通常是一种异常状况.
			forward();
			res->rc = STM_ABORT;
		}
		else
		{
			FCGIRecord record;
			for(;;)
			{
				size_t recrodLen = record.assign(_recordPipe->buffer(), (size_t)_recordPipe->size());
				if(recrodLen <= 0)
				{
					// 缓存内的数据不够构建一个完整的 FCIG Record
					break;
				}
				else
				{
					// 成功读取一个 FCGI Record
					_recordPipe->skip(recrodLen);

					// 处理之
					disposeRecord(&record);
					record.reset();
				}
			}

			if(_fcgiRequestEnd)
			{
				assert(_recordPipe->size() == 0);

				/// 对应于 fcgiAdp 的状态机运行结束
				forward();
				res->rc = STM_DONE;
			}
			else
			{
				/// 继续接收 FCGI 服务器输出
				res->rc = STM_CONTINUE;
				res->param.epollEvent = IO_EVENT_EPOLLIN;
			}
		}
	}

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