#pragma once
#include "HTTPLib.h"
/*
 * HTTP响应生成
 * 为不同的 HTTP请求 生成对应的 HTTP响应.
 * DefaultResponderFactory 和 FCGIResponderFactory
 *
 */

/* 默认的响应生成器,用来处理一般的 HTTP请求 */
class DefaultResponderFactory : public IResponderFactory
{
private:
	IHTTPServer *_svr;
public:
	DefaultResponderFactory(IHTTPServer* svr);
	~DefaultResponderFactory();

	IResponder* catchRequest(IRequest* req);
	void releaseResponder(IResponder* res);
};