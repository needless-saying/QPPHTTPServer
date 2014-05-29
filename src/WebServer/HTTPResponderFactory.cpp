#include "StdAfx.h"

#include "HTTPLib.h"
#include "HTTPResponder.h"
#include "HTTPResponderFactory.h"

DefaultResponderFactory::DefaultResponderFactory(IHTTPServer* svr)
	: _svr(svr)
{
}

DefaultResponderFactory::~DefaultResponderFactory()
{
}

IResponder* DefaultResponderFactory::catchRequest(IRequest* req)
{
	return new HTTPResponder(_svr, req);
}

void DefaultResponderFactory::releaseResponder(IResponder* res)
{
	delete res;
}