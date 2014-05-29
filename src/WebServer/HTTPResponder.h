/* Copyright (C) 2012 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#pragma once
#include "HTTPLib.h"
#include "HTTPResponseHeader.h"
#include "HTTPRequest.h"

/*
* HTTPResponder 类是接口 IResponder 的实现,封装了 HTTP 协议的响应报文.
* HTTPResponder 接受 IRequest HTTP 请求报文为参数,生成对应的响应报文,并把数据发送到 HTTP 客户端.
* 
*/

class HTTPContent;
class HTTPResponder : public IResponder
{
protected:
	HTTPResponseHeader _header;
	IRequest *_request;
	IHTTPServer *_server;
	HTTPContent* _content;
	__int64 _bytesRecved;
	__int64 _bytesSent;
	int _svrCode;

	bool makeResponseHeader(int svrCode);	
	void getHeaderBuffer(const char** buf, size_t *len);
	bool beforeStep(IOAdapter* adp, int ev, stm_result_t* res);
	bool step0(IOAdapter* adp, int ev, stm_result_t* res);
	bool step1(IOAdapter* adp, int ev, stm_result_t* res);
	bool step2(IOAdapter* adp, int ev, stm_result_t* res);

public:
	HTTPResponder(IHTTPServer *server, IRequest *req);
	HTTPResponder(IHTTPServer *server, IRequest *req, int sc, const char* msg);
	virtual ~HTTPResponder();

	IRequest* getRequest();
	std::string getHeader();
	int getServerCode();
	void statistics(__int64* bytesRecved, __int64* bytesSent);
};