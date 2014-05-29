/* Copyright (C) 2011 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#pragma once
#include "HTTPLib.h"
#include "FCGIRecord.h"

/*
* 关于Fast CGI 的运行模式
* 1. 本地模式,在Windows下使用NT的 命名管道.
* 2. 远程模式,使用套接字.

* FastCGI on NT will set the listener pipe HANDLE in the stdin of
* the new process.  The fact that there is a stdin and NULL handles
* for stdout and stderr tells the FastCGI process that this is a
* FastCGI process and not a CGI process.
*/

/*
* Fast CGI 连接工厂,用来管理 FCGI 连接,并创建和管理本地模式下的 FCGI 服务器进程.
* 
* 说明: 为什么要使用独立的管道名.
* 使用独立的管道名的好处是每个连接对应一个 FCGI 进程,如此就可以安全的关闭 FCGI 进程而不用担心影响到其他进程.
*/

/*
* 本地FCGI进程定义
*/

class FCGIResponder;
class FCGIFactory : public IResponderFactory, public IOStateMachine
{
public:
	typedef struct
	{
		PROCESS_INFORMATION processInfo; /* 进程句柄 */
		char pipeName[MAX_PATH]; /* 该进程对应的管道名 */
		uintptr_t thread; /* 创建进程并连接管道的孵化线程 */
		IOAdapter* adp;
		FCGIFactory* instPtr;
	}fcgi_process_context_t;
	typedef std::list<fcgi_process_context_t*> fcgi_process_list_t;
private:
	fcgi_server_ctx_t _fcgiServerCtx;
	u_short _fcgiId;
	IHTTPServer* _svr;
	IOStateMachineScheduler* _scheduler;
	Lock _lock;

	fcgi_process_list_t _processList;		// 本地FCGI进程列表
	std::list<IOAdapter*> _busyAdpList;		// 忙碌连接列表
	std::list<IOAdapter*> _idleAdpList;		// 空闲列表
	std::list<FCGIResponder*> _waitingList;

	void lock();
	void unlock();

	u_short getFCGIId();

	// 分派FCGI连接
	void dispatch();

	// IOStateMachine
	bool step0(IOAdapter* adp, int ev, stm_result_t* res);

	// stm handler
	void handleResponder(FCGIResponder* responder, IOAdapter* adp, IOStateMachine* sm, stm_result_t* res);

public:
	FCGIFactory(IHTTPServer *svr, IOStateMachineScheduler* scheduler);
	~FCGIFactory();

	// 创建本地FCGI进程的线程函数
	int spawn(fcgi_process_context_t *context);

	void init(fcgi_server_ctx_t* ctx);
	void release();

	// IResponderFactory
	IResponder* catchRequest(IRequest* request);
	void releaseResponder(IResponder* responder);
};

