/* Copyright (C) 2011 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#include "StdAfx.h"
#include "HTTPRequest.h"

HTTPRequest::HTTPRequest(IHTTPServer *server) 
	: _httpHeader(NULL), _cachePipe(NULL), _httpPostData(NULL), _server(server), _contentLength(0), _bytesRecved(0), _bytesSent(0)
{
	_cachePipe = new BufferPipe(MAX_REQUESTHEADERSIZE, K_BYTES);
}

HTTPRequest::~HTTPRequest()
{
	if(_cachePipe) delete _cachePipe;
	if(_httpHeader) delete _httpHeader;
	if(_httpPostData) delete _httpPostData;
}

bool HTTPRequest::isValid()
{
	if(_httpHeader)
	{
		// 检查是否已经收到了完整的请求头
		const char* header = NULL;
		size_t headerLen = 0;
		getHeaderBuffer(&header, &headerLen);
		
		if(http_header_end(header, headerLen) > 0)
		{
			if(contentLength() == 0 || (_httpPostData && _httpPostData->size() == contentLength()))
			{
				return true;
			}
			else
			{
				// content length 不正确.
			}
		}
		else
		{
			// 请求头不完整.
		}
	}
	else
	{
		// 没有接收到请求头.
	}

	return false;
}

void HTTPRequest::getHeaderBuffer(const char** buf, size_t *len)
{
	if(_httpHeader)
	{
		*buf = (const char*)_httpHeader->buffer();
		*len = (size_t)_httpHeader->size() - 1;
	}
	else
	{
		*buf = NULL;
		*len = 0;
	}
}

bool HTTPRequest::keepAlive()
{
	return stricmp(field("Connection").c_str(), "keep-alive") == 0;
}

size_t HTTPRequest::contentLength()
{
	return _contentLength;
}

HTTP_METHOD HTTPRequest::method()
{
	const char* header = NULL;
	size_t headerLen = 0;
	getHeaderBuffer(&header, &headerLen);

	// 取出 HTTP 方法
	char szMethod[MAX_METHODSIZE] = {0};
	int nMethodIndex = 0;
	for(size_t i = 0; i < MAX_METHODSIZE && i < headerLen; ++i)
	{
		if(header[i] != ' ')
		{
			szMethod[nMethodIndex++] = header[i];
		}
		else
		{
			break;
		}
	}

	// 返回
	if( strcmp(szMethod, "GET") == 0 ) return METHOD_GET;
	if( strcmp(szMethod, "PUT") == 0 ) return METHOD_PUT;
	if( strcmp(szMethod, "POST") == 0 ) return METHOD_POST;
	if( strcmp(szMethod, "HEAD") == 0 ) return METHOD_HEAD;
	if( strcmp(szMethod, "DELETE") == 0 ) return METHOD_DELETE; // 删除
	if( strcmp(szMethod, "TRACE") == 0 ) return METHOD_TRACE;
	if( strcmp(szMethod, "CONNECT") == 0 ) return METHOD_CONNECT;

	return METHOD_UNKNOWN;
}

// 返回客户端请求对象, 如果返回空字符串,说明客户端请求格式错误.
std::string HTTPRequest::uri(bool decode)
{
	const char* header = NULL;
	size_t headerLen = 0;
	getHeaderBuffer(&header, &headerLen);

	std::string strObject("");
	const char* lpszRequest = header;
	const char *pStart = NULL, *pEnd = NULL;

	// 第一行的第一个空格的下一个字符开始是请求的文件名开始.
	for(size_t i = 0; i < headerLen; ++i)
	{
		if(lpszRequest[i] == ' ')
		{
			pStart = lpszRequest + i + 1; 
			break;
		}
		if(lpszRequest[i] == '\n') break;
	}
	if(pStart == NULL)
	{
		// 找不到开始位置
		return strObject;
	}

	// 从第一行的末尾方向查找第一个空格,实例: GET / HTTP/1.1
	pEnd = strstr(lpszRequest, "\r\n"); 
	if(pEnd == NULL || pEnd < pStart) 
	{
		/* 找不到结尾位置 */
		assert(0);
		return strObject;
	}

	// 把结尾的空格移除
	while(pEnd >= pStart)
	{
		if(pEnd[0] == ' ')
		{
			pEnd--;
			break;
		}
		pEnd--;
	}

	if(pEnd == NULL || pEnd < pStart)
	{
		assert(0);
	}
	else
	{
		strObject.assign(pStart, pEnd - pStart + 1);
	}

	if(decode) return decode_url(strObject);
	else return strObject;
}

std::string HTTPRequest::field(const char* pszKey)
{
	const char* header = NULL;
	size_t headerLen = 0;
	getHeaderBuffer(&header, &headerLen);

	return get_field(header, pszKey);
}

bool HTTPRequest::range(__int64 &lFrom, __int64 &lTo)
{
	__int64 nFrom = 0;
	__int64 nTo = -1; // -1 表示到最后一个字节.

	const char* header = NULL;
	size_t headerLen = 0;
	getHeaderBuffer(&header, &headerLen);

	const char* lpszRequest = header;
	const char* pRange = strstr(lpszRequest, "\r\nRange: bytes=");
	if(pRange)
	{
		/*
		The first 500 bytes (byte offsets 0-499, inclusive):
		bytes=0-499
		The second 500 bytes (byte offsets 500-999, inclusive):
		bytes=500-999
		The final 500 bytes (byte offsets 9500-9999, inclusive):
		bytes=-500
		bytes=9500-
		The first and last bytes only (bytes 0 and 9999):
		bytes=0-0,-1
		Several legal but not canonical specifications of the second 500 bytes (byte offsets 500-999, inclusive):
		bytes=500-600,601-999
		bytes=500-700,601-999
		*/

		pRange += strlen("\r\nRange: bytes=");
		const char *pMinus = strchr(pRange, '-');
		if(pMinus)
		{
			char szFrom[200], szTo[200];
			memset(szFrom, 0, 200);
			memset(szTo, 0, 200);
			memcpy(szFrom, pRange, pMinus - pRange);
			nFrom = _atoi64(szFrom);

			pMinus++;
			pRange = strstr(pMinus, "\r\n");
			if(pMinus + 1 == pRange)
			{
				nTo = -1;
			}
			else
			{
				memcpy(szTo, pMinus, pRange - pMinus);
				nTo = _atoi64(szTo);
				if(nTo <= 0) nTo = -1;
			}

			lFrom = nFrom;
			lTo = nTo;

			return true;
		}
		else
		{
		}
	}
	else
	{
	}
	return false;
}

std::string HTTPRequest::getHeader()
{
	const char* header = NULL;
	size_t headerLen = 0;
	getHeaderBuffer(&header, &headerLen);
	
	return header;
}

IPipe* HTTPRequest::getPostData()
{
	return _httpPostData;
}

void HTTPRequest::nextRequest()
{
	if(_httpHeader) delete _httpHeader;
	if(_httpPostData) delete _httpPostData;
	_httpHeader = NULL;
	_httpPostData = NULL;
	_contentLength = 0;
	_bytesRecved = 0;
	_bytesSent = 0;

	// 状态机状态重置
	backward(current());
}

bool HTTPRequest::beforeStep(IOAdapter* adp, int ev, stm_result_t* res)
{
	if(TEST_BIT(ev, IO_EVENT_EPOLLERR))
	{
		/* 套接字错误 */
		res->rc = STM_ABORT;
		res->param.errorCode = SE_RECVFAILD;
		return false;
	}
	if(TEST_BIT(ev, IO_EVENT_EPOLLHUP))
	{
		/* 对方已经关闭了连接 */
		res->rc = STM_ABORT;
		res->param.errorCode = SE_REMOTEFAILD;
		return false;
	}

	return true;
}

bool HTTPRequest::step0(IOAdapter* adp, int ev, stm_result_t* res)
{
	assert(_cachePipe);

	/* 创建一个请求头的接收代理,用来接收请求头和POSTdata */
	build_pipes_chain((IPipe*)adp, (IPipe*)_cachePipe);
	forward();

	res->rc = STM_CONTINUE;
	res->param.epollEvent = IO_EVENT_EPOLLIN;
	return false;
}

bool HTTPRequest::step1(IOAdapter* adp, int ev, stm_result_t* res)
{
	// 读取套接字数据
	size_t rd = _cachePipe->pump();
	_bytesRecved += rd;
	if(0 == rd)
	{
		// 请求头长度超过限制
		assert(0);
		forward(2);
		return true;
	}
	else
	{
		// 判断是否已经读取到完整的请求头
		int headerlen = http_header_end((const char*)_cachePipe->buffer(), (size_t)_cachePipe->size());
		if(headerlen < 0)
		{
			// 继续接收请求头
		}
		else
		{
			// 把请求头从缓存中读出单独保存.之所以设计一个缓存是为了处理HTTP1.1协议的"管道化连接"(客户端在收到第一个回应之前可能连续发送若干个请求头)
			// 服务器端总是依次处理.
			assert(_httpHeader == NULL);
			_httpHeader = new BufferPipe(MAX_REQUESTHEADERSIZE, K_BYTES);
			size_t plen = _httpHeader->pump(headerlen, _cachePipe);
			assert(plen == headerlen);

			 // 请求头是字符串,写个0结尾.
			char c = 0;
			_httpHeader->write(&c, 1);

			// 设置Content-Length的值,之后可以访问 contentLength() 直接获取,而不用再请求头文本内查找.
			_contentLength = atoi(field("Content-Length").c_str());
			if(contentLength() >= MAX_POST_DATA)
			{
				/* content-length 长度超出限制 */
				forward(2);
				return true;
			}
			else if(contentLength() > 0)
			{
				// 如果长度超过 POST_DATA_CACHE_SIZE 则用临时文件,否则直接在内存中缓冲.
				if(contentLength() > POST_DATA_CACHE_SIZE)
				{
					_httpPostData = new FilePipe(_server->tmpFileName());
				}
				else
				{
					_httpPostData = new BufferPipe(POST_DATA_CACHE_SIZE, (4 * K_BYTES));
				}
				_httpPostData->link(_cachePipe);


				/* 从 _cachePipe 读取POST DATA */
				forward();
				return true;
			}
			else
			{
				// 没有POST数据
				forward(2);
				return true;
			}
		}// continue to receive header
	}// continue to pump

	// 继续接收请求头
	res->rc = STM_CONTINUE;
	res->param.epollEvent = IO_EVENT_EPOLLIN;
	return false;
}

bool HTTPRequest::step2(IOAdapter* adp, int ev, stm_result_t* res)
{
	assert(_httpPostData);
	size_t maxlen = contentLength() - (size_t)_httpPostData->size();
	size_t rd = 0;

	// 接收post数据
	if(_cachePipe->size() > 0)
	{
		// 数据已经先读取到缓存中,不要重复统计.
		rd = _httpPostData->pump(maxlen);
	}
	else
	{
		// 直接从 IOAdapter 中读取
		rd = _httpPostData->pump(maxlen);
		_bytesRecved += rd;

		if(rd == 0)
		{
			// 400 Bad request
			forward();
			return true;
		}
	}

	if(_httpPostData->size() >= contentLength())
	{
		// POST DATA接收完毕.
		_httpPostData->unlink();
		
		forward();
		return true;
	}
	else
	{
		// 继续接收 POST 数据
		res->rc = STM_CONTINUE;
		res->param.epollEvent = IO_EVENT_EPOLLIN;
		return false;
	}
}

bool HTTPRequest::step3(IOAdapter* adp, int ev, stm_result_t* res)
{
	res->rc = STM_DONE;
	return false;
}

void HTTPRequest::statistics(__int64* bytesRecved, __int64* bytesSent)
{
	*bytesRecved = _bytesRecved;
	*bytesSent = _bytesSent;
}