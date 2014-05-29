#pragma once
#include "HTTPLib.h"

typedef struct request_statis_t
{
	std::string uri;
	HTTP_METHOD method;
	int sc;
	size_t usedTime; // 消耗时间-毫秒
	__int64 bytesRecved;
	__int64 bytesSent;
}REQUEST_STATIS;
typedef struct connection_statis_t
{
	size_t usedTime;
	size_t requestCount; // 总计处理的请求数
	__int64 bytesRecved;
	__int64 bytesSent;
}CONNECTION_STATIS;

class HTTPConnection : public IOStateMachine
{
protected:
	IRequest* _request;
	IResponder* _responder;
	IHTTPServer* _svr;
	IResponderFactory* _factory;
	IOAdapter* _adp;
	char _remoteIPAddr[MAX_IP_LENGTH + 1];
	u_short _remotePort;

	// 统计信息
	HighResolutionTimer _hrt;
	__int64 _connStartTime;
	__int64 _connEndTime;
	__int64 _connRecvBytes;
	__int64 _connSentBytes;
	__int64 _reqStartTime;
	__int64 _reqEndTime;
	__int64 _reqRecvBytes;
	__int64 _reqSentBytes;
	size_t _reqCount;

	// 收集
	void requestBegin();
	void requestEnd();
	void connectionBegin();
	void connectionEnd();

	bool beforeStep(IOAdapter* adp, int ev, stm_result_t* res);
	bool step0(IOAdapter* adp, int ev, stm_result_t* res);
	bool step1(IOAdapter* adp, int ev, stm_result_t* res);
	bool step2(IOAdapter* adp, int ev, stm_result_t* res);
	bool step3(IOAdapter* adp, int ev, stm_result_t* res);
	bool step4(IOAdapter* adp, int ev, stm_result_t* res);

public:
	HTTPConnection(IHTTPServer* svr, IOAdapter* adp);
	virtual ~HTTPConnection();

	const char* getRemoteIPAddr();
	u_short getRemotePort();
	
	IRequest* getRequest();
	IResponder* getResponder();
	void queryLastRequest(request_statis_t* info);
	void query(connection_statis_t* info);
};

