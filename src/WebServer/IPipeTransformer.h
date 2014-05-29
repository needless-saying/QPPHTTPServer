#pragma once
#include "Pipes/BufferPipe.h"

/*
* 变换器
* read / write 的逻辑和 IPipe 不一样
* IPipe: 如果当前管道内无数据,可以直接从上一个管道读取到客户缓冲区内.(数据流跳过了当前管道)
* IPipeFilter: 所有的数据必须被当前管道处理.(数据流必须进过当前管道)
*/
class IPipeTransformer : IPipe
{
private:
	/*
	* _packageSize 表示一次可以变换的最大长度.
	* 内部缓冲区会缓存变换后的数据,应该确保 _packageSize 不会过大导致内部缓冲区无法缓存,数据会丢失.
	*/
	BufferPipe _bp;
	size_t _packageSize;
	byte* _packageBuf;

	size_t doTransform(const void* ibuf, size_t ilen);

protected:
	// 变换
	virtual size_t _transform(const void* ibuf, size_t ilen, void* obuf, size_t olen) = 0;
	virtual size_t _bound(const void* ibuf, size_t ilen) = 0;

	size_t _read(void* buf, size_t len);
	size_t _pump(reader_t reader, size_t maxlen);
	__int64 _size();
	bool _skip();

public:
	IPipeTransformer(size_t packageSize);
	virtual ~IPipeTransformer();
};