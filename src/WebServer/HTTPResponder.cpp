/* Copyright (C) 2011 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#include "StdAfx.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "HTTPResponder.h"
#include "HTTPContent.h"

HTTPResponder::HTTPResponder(IHTTPServer *server, IRequest *req)
	: _server(server), _request(req), _content(NULL), _svrCode(SC_OK), _bytesSent(0), _bytesRecved(0)
{
	
}

HTTPResponder::HTTPResponder(IHTTPServer *server, IRequest *req, int sc, const char* msg)
	:_svrCode(sc), _server(server), _request(req)
{
	_content = new HTTPContent();
	_content->open(msg, strlen(msg), CONTENT_TYPE_TEXT);
}

HTTPResponder::~HTTPResponder()
{
	if(_content) delete _content;
}

bool HTTPResponder::makeResponseHeader(int svrCode)
{
	/* 写响应头 */
	_header.setResponseCode(svrCode);
	_header.addDefaultFields();
	if(svrCode == SC_BADMETHOD)
	{
		_header.add("Allow", "GET, HEAD, POST, PUT");
	}
	if(_content)
	{
		// Last-Modified
		_header.add("Last-Modified", _content->lastModified());
		
		// ETag
		_header.add("ETag", _content->etag());
		
		// Content-Type
		_header.add("Content-Type", _content->contentType());
		
		// Content-Length
		char szLen[200] = {0};
		__int64 lLen = _content->contentLength();
		_header.add("Content-Length", _i64toa(lLen, szLen, 10));
		
		// Content-Range: bytes %d-%d/%d\r\n"
		if(SC_PARTIAL == svrCode)
		{
			_header.add("Content-Range", _content->contentRange());
		}

		// "Accept-Ranges: bytes" 支持断点续传(只有静态文件支持断点续传).
		if(_content->acceptRanges())
		{
			_header.add("Accept-Ranges", "bytes");
		}
	}
	else
	{
		// Content-Length 
		_header.add("Content-Length", "0");
	}
	if(_request->keepAlive())
	{
		_header.add("Connection", "keep-alive");
	}
	else
	{
		_header.add("Connection", "close");
	}

	// 格式化响应头准备输出
	_header.format();

	return true;
}

std::string HTTPResponder::getHeader()
{
	return _header.getHeader();
}

int HTTPResponder::getServerCode()
{
	return _header.getResponseCode();
}

IRequest* HTTPResponder::getRequest()
{
	return _request;
}

void HTTPResponder::statistics(__int64* bytesRecved, __int64* bytesSent)
{
	*bytesRecved = _bytesRecved;
	*bytesSent = _bytesSent;
}

bool HTTPResponder::beforeStep(IOAdapter* adp, int ev, stm_result_t* res)
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

bool HTTPResponder::step0(IOAdapter* adp, int ev, stm_result_t* res)
{
	/* 初始化 */
	do
	{
		if(_content != NULL)
		{
			// 已经初始化过了
			break;
		}

		// 根据 request 生成一个响应
		_content = new HTTPContent();

		/* 非法请求头或者请求头太大 */
		if(!_request->isValid())
		{
			_svrCode = SC_BADREQUEST;
			_content->open(g_HTTP_Bad_Request, strlen(g_HTTP_Bad_Request), CONTENT_TYPE_TEXT);
			break;
		}

		/* 静态文件只支持 GET 和 HEAD 方法 */
		if( METHOD_GET != _request->method() && METHOD_HEAD != _request->method())
		{
			_svrCode = SC_BADMETHOD;
			_content->open(g_HTTP_Bad_Method, strlen(g_HTTP_Bad_Method), CONTENT_TYPE_TEXT);
			break;
		}

		/* 打开文件 */
		std::string uri = _request->uri(true);
		std::string serverFilePath("");

		// 映射为服务器文件名.
		if(!_server->mapServerFilePath(uri, serverFilePath))
		{
			// 无法映射服务器文件名,提示禁止
			_svrCode = SC_FORBIDDEN;
			_content->open(g_HTTP_Forbidden, strlen(g_HTTP_Forbidden), CONTENT_TYPE_TEXT);
		}
		else if(serverFilePath.back() == '\\')
		{
			if(_content->open(uri, serverFilePath))
			{
				_svrCode = SC_OK;
			}
			else
			{
				_svrCode = SC_SERVERERROR;
				_content->open(g_HTTP_Server_Error, strlen(g_HTTP_Server_Error), CONTENT_TYPE_TEXT);
			}
		}
		else
		{
			// 客户端是否请求了断点续传的内容
			// 创建文件内容对象并关联给Response对象
			__int64 lFrom = 0;
			__int64 lTo = -1;
			if(_request->range(lFrom, lTo))
			{
				_svrCode = SC_PARTIAL;
			}

			if(!_content->open(serverFilePath, lFrom, lTo))
			{
				_svrCode = SC_NOTFOUND;
				_content->open(g_HTTP_Content_NotFound, strlen(g_HTTP_Content_NotFound), CONTENT_TYPE_TEXT);
			}
		}
	}
	while(false);
	
	/*
	* 创建管道链
	*/
	makeResponseHeader(_svrCode);
	if(_request->method() == METHOD_HEAD)
	{
		_content->close();
		delete _content;
		_content = NULL;

		build_pipes_chain((IPipe*)&_header, (IPipe*)adp);
	}
	else
	{
		// 必须添加 (IPipe*) 否则会导致可变参数获取失败.
		build_pipes_chain((IPipe*)_content, (IPipe*)&_header, (IPipe*)adp);
	}

	/* 
	* 发送第一个数据包 
	*/
	res->rc = STM_CONTINUE;
	res->param.epollEvent = IO_EVENT_EPOLLOUT;

	forward();
	return false;
}

bool HTTPResponder::step1(IOAdapter* adp, int ev, stm_result_t* res)
{
	/* 发送响应流 */
	size_t wr = adp->pump(0);
	_bytesSent += wr;
	if( 0 == wr )
	{
		/* 发送完毕 */
		forward();
		return true;
	}
	else
	{
		/* 继续发送 */
		res->rc = STM_CONTINUE;
		res->param.epollEvent = IO_EVENT_EPOLLOUT;
	}
	return false;
}

bool HTTPResponder::step2(IOAdapter* adp, int ev, stm_result_t* res)
{
	/* 结束 */
	res->rc = STM_DONE;
	return false;
}