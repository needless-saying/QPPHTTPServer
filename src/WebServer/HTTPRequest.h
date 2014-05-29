/* Copyright (C) 2011 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#pragma once

#include "HTTPLib.h"
#include "Pipes/BufferPipe.h"
#include "Pipes/FilePipe.h"
/*
* HTTP 协议中的"请求"报文的封装
*
* HTTPRequest 对象实现了 IRequest 接口,开始运行后从客户端读取整个 HTTP请求报文,包括请求头和 POST 内容
* 
*/

class HTTPRequest : public IRequest
{
protected:
	BufferPipe* _cachePipe;
	BufferPipe* _httpHeader;
	IPipe* _httpPostData;
	IHTTPServer *_server;
	size_t _contentLength;
	__int64 _bytesRecved;
	__int64 _bytesSent;

	void getHeaderBuffer(const char** buf, size_t *len);
	bool beforeStep(IOAdapter* adp, int ev, stm_result_t* res);
	bool step0(IOAdapter* adp, int ev, stm_result_t* res);
	bool step1(IOAdapter* adp, int ev, stm_result_t* res);
	bool step2(IOAdapter* adp, int ev, stm_result_t* res);
	bool step3(IOAdapter* adp, int ev, stm_result_t* res);

public:
	HTTPRequest(IHTTPServer *server);
	virtual ~HTTPRequest();

	/* IRequest */
	HTTP_METHOD method(); // 返回HTTP 方法
	std::string uri(bool decode); // 返回客户端请求的对象(已经经过UTF8解码,所以返回宽字符串)
	std::string field(const char* key); // 返回请求头中的一个字段(HTTP头中只有ANSI字符,所以返回string).
	bool range(__int64 &from, __int64 &to);
	bool keepAlive();
	size_t contentLength(); /* 请求头中的 Content-Length 字段的值 */
	std::string getHeader();
	bool isValid();
	IPipe* getPostData();
	void nextRequest();
	void statistics(__int64* bytesRecved, __int64* bytesSent);
};
