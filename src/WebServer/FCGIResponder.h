/* Copyright (C) 2012 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#pragma once
#include <list>
#include "HTTPLib.h"
#include "HTTPResponseHeader.h"
#include "Pipes/BufferPipe.h"
#include "Pipes/FilePipe.h"
#include "FCGITransformer.h"
#include "ChunkTransformer.h"

/* 
* FCGI 协议的 Responder, FCGIResponder 接收 HTTPRequest 为参数,控制和FCGI服务器间的通信,并把响应流转发到 HTTP 客户端
*
* 2014-3-19
* V0.2版FCGIResponder绝对是我写过的最恶心的代码,逻辑混乱结构不清,蹩脚的设计造就了难以理解的编码和各种暗藏的陷阱,事实证明这样的代码果然是有问题的代码.
* V0.3版要推倒重来,简单的,优雅的代码才是我的追求.
*/
class FCGIRecord;
class FCGIResponder : public IResponder
{
protected:
	int _svrCode;
	IRequest *_request;
	IHTTPServer *_server;
	IOAdapter* _fcgiAdp;
	u_short _fcgiId;
	IOAdapter* _httpAdp;

	// 统计信息
	__int64 _bytesHTTPSent;
	__int64 _bytesHTTPRecv;
	__int64 _bytesFCGISent;
	__int64 _bytesFCGIRecv;

	// 是否缓存结束后再发送到HTTP客户端
	bool _cacheAll;
	HTTPResponseHeader _header;
	bool _headerFormatted;
	bool _fcgiRequestEnd;

	/*
	* 缓冲管道
	*/
	// 1. 发送到FCGI服务器: POST data -> FCGITransformer -> BufferPipe -> FCGI Adapter
	// 2. 从FCGI服务器接收: FCGI Adapter -> BufferPipe
	// _recordPipe 重复使用(输入时作为输入缓冲区,输出时作为输出缓冲区
	BufferPipe* _recordPipe;
	FCGITransformer* _recordTransformer;

	// 3发送到HTTP客户端 FCGIResponder -> FilePipe -> BufferPipe -> ChunkFilter -> HTTPResponderHeader -> HTTP Adapter
	bool _httpClientRuning;
	BufferPipe* _cacheBufferPipe;
	FilePipe* _cacheFilePipe;
	ChunkTransformer* _chunkTransformer;

	// 同步锁,如果不缓存,数据流FCGI服务器 -> FCGIResponder 和 FCGIResponder -> HTTP Client 之间需要做同步
	Lock* _lock;
	void lock();
	void unlock();

	// 准备 HTTP -> FCGI 
	void prepareFCGIInput();

	// 解析 FCGI 服务器发送来的数据
	void disposeRecord(FCGIRecord* record);

	// 尝试启动 HTTP client pump
	void tryHttpClientPump();

	bool beforeStep(IOAdapter* adp, int ev, stm_result_t* res);
	bool step0(IOAdapter* adp, int ev, stm_result_t* res);
	bool step1(IOAdapter* adp, int ev, stm_result_t* res);
	bool step2(IOAdapter* adp, int ev, stm_result_t* res);
	bool step3(IOAdapter* adp, int ev, stm_result_t* res);
	bool step4(IOAdapter* adp, int ev, stm_result_t* res);
	
public:
	FCGIResponder(IHTTPServer *server, IRequest* request, bool cacheAll);
	~FCGIResponder();

	// 专有函数,需要知道哪个是FCGI服务器连接
	IOAdapter* setFCGIConnection(u_short fcgiId, IOAdapter* adp);

	// IResponder
	IRequest* getRequest();
	std::string getHeader();
	int getServerCode();
	void statistics(__int64* bytesRecved, __int64* bytesSent);
};

