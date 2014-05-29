#pragma once
#include "pipe.h"
#include "..\Buffer.h"

class BufferPipe : public IPipe
{
private:
	// 私有的,派生类不要改这些变量
	// 个人比较喜欢包含(代理)接口实现而不是派生,派生的耦合性太强了,必须完全了解基类.
	// 如果是纯虚接口,那问题不大,接口总是容易理解的;如果不是虚接口,基类实现又很复杂,没有完全理解,敢随便修改一个基类的变量吗?
	Buffer _buffer;
	size_t _rdPos;
	size_t _wrPos;
	size_t _maxSize;
	size_t _stepSize;

	void trim();
protected:
	// IPipe
	size_t _read(void* buf, size_t len);
	size_t _pump(reader_t reader, size_t maxlen);
	__int64 _size();

public:
	BufferPipe(size_t maxBufSize, size_t stepSize);
	BufferPipe(void* buf, size_t dataLen, size_t bufLen);
	BufferPipe(const void* buf, size_t dataLen, size_t bufLen);
	~BufferPipe();
	
	void trunc();

	/*
	*  提供直接访问内部缓冲区的方法,减少内存拷贝.
	*/
	// 锁定缓冲区 for write, 相当于 _write, 直接访问内部区缓冲区
	size_t lock(void **buf, size_t len);
	size_t unlock(size_t len);

	// 获得直接访问内部缓冲区的读指针
	const void* buffer();
	// size()
	size_t skip(size_t len);
};
