#pragma once

/* Copyright (C) 2013 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

/*
 * "管道"是V0.3的抽象核心(2).
 * 一次HTTP请求可以理解为一系列的数据流动:HTTP请求通过 socket 管道"流"到 Server
 * Server把这个管道连接到一个 file 管道/Fcgi 管道等,然后使之反向流动以完成一次HTTP
 * 请求.
 * 在管道系统中,只能在管道头"读",通过读操作把数据从连接在一起的各个管道中"抽"出来.
 * 只能在管道尾"写",通过写操作把数据"推"过去.
 *
 * 数据流向: (data) -> |prev| -> |this| -> |next| -> (data)
 * 调用静态函数 buildChain 构建管道链
 * 管道链一旦构建,只在头尾两端读写.
 * 数据流是单向的.
 *
*/

/*
 *  一个FCGI响应的数据流: FCGI IOPipeSource -> ResponderHeader -> TmpFilePipe -> BufferPipe -> HTTP IOPipeSink (不缓存FCGI结果)
 *  或者 FCGI IOPipeSource -> TmpFilePipe -> BufferPipe -> ResponderHeader -> HTTP IOPipeSink (缓存fcgi结果)
*/

/*
* pump / push 的逻辑并不完全一样(即 a.pump(b)  != b.push(a), 调用 pump 和 push 在外部表现出来时一样的,但是管道系统内部不一样.
* pump 会把数据抽到目标管道,除了目标管道,数据并不会存储在其他管道.
* push 会把数据推送到最前端,如果最前端管道存储不了,则会压入次前端管道,依次类推.
*/

/*
* 管道系统的基类.
*/
#include <functional>
typedef std::function<unsigned int (void* buf, unsigned int len)> reader_t;
class IPipe
{
private:
	IPipe* _prev;
	IPipe* _next;
	
protected:
	IPipe();

	virtual size_t _read(void* buf, size_t len) = 0;
	virtual size_t _pump(reader_t reader, size_t maxlen) = 0;
	virtual __int64 _size() = 0;

	/* 是否允许跳过当前管道直接把数据写入下一个管道内 */
	virtual bool _skip();

public:
	virtual ~IPipe();

	/*
	* 连接到另一个管道后面
	*/
	void link(IPipe* prevPipe);
	void unlink();
	IPipe* prev();
	IPipe* next();
	
	/*
	* 管道链的数据流是单方向的 总是从 prev -> current -> next.
	* 可以在 next 端调用 pump 抽取数据, 也可以在 prev 端 调用 push 推送数据. 两种方法的数据流方向是一致的.
	* 要提供 size() 就应该提供 pump(), 同理 space() 和 push().
	*/
	__int64 size();
	size_t pump(size_t maxlen = 0, IPipe* src = NULL);
	size_t push(size_t maxlen = 0, IPipe* dest = NULL);
	size_t read(void* buf, size_t len);
	size_t write(const void* buf, size_t len);
};

/*
* 把参数中所有pipe连城链,参数要以最后一个NULL结尾
* 实例: _build_pipe_chain(p1,p2,p3,p4,p5,NULL) 将构建一个数据流为: p1 -> p2 -> p3 -> p4 -> p5 的管道链
*/
size_t _build_pipe_chain(IPipe* p, ...);
#define build_pipes_chain(p0, ...) _build_pipe_chain(p0, __VA_ARGS__, NULL)