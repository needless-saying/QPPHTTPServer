#pragma once
#include "HTTPLib.h"
#include "Pipes/BufferPipe.h"
#include "Pipes/FilePipe.h"

class FCGIConnection : public IOStateMachine
{
private:
	IRequest* _request;
	IHTTPServer* _server;

	bool beforeStep(IOAdapter* adp, int ev, stm_result_t* res) override;
	bool step0(IOAdapter* adp, int ev, stm_result_t* res) override;
	bool step1(IOAdapter* adp, int ev, stm_result_t* res) override;
	bool step2(IOAdapter* adp, int ev, stm_result_t* res) override;

public:
	FCGIConnection(IRequest* request, IHTTPServer *svr, bool cacheAll);
	~FCGIConnection();

	IPipe* getStdout();
	IPipe* getStdin();
};

