#include "StdAfx.h"
#include "BufferPipe.h"

BufferPipe::BufferPipe(size_t maxBufSize, size_t stepSize)
	: _buffer(stepSize, maxBufSize), _rdPos(0), _wrPos(0), _stepSize(stepSize), _maxSize(maxBufSize)
{
}

BufferPipe::BufferPipe(void* buf, size_t dataLen, size_t bufLen)
	: _buffer(buf, bufLen), _rdPos(0), _wrPos(dataLen), _stepSize(bufLen), _maxSize(bufLen)
{
}

BufferPipe::BufferPipe(const void* buf, size_t dataLen, size_t bufLen)
	: _buffer((void*)buf, bufLen), _rdPos(0), _wrPos(dataLen), _stepSize(bufLen), _maxSize(bufLen)
{
}

BufferPipe::~BufferPipe()
{
}

void BufferPipe::trim()
{
	void* buf = NULL;

	// 先把内部缓冲区整理一下(未读数据移到开头位置,丢弃已读部分)
	if(_rdPos > 0)
	{
		_buffer.seek(0, SEEK_SET);
		if(_buffer.lock(&buf, _wrPos) > 0)
		{
			memmove(buf, (char*)buf + _rdPos, _wrPos - _rdPos);
			_wrPos -= _rdPos;
			_rdPos = 0;
			_buffer.unlock(_wrPos);
		}
	}
}

__int64 BufferPipe::_size()
{
	return _wrPos - _rdPos;
}

size_t BufferPipe::_read(void* buf, size_t len)
{
	size_t maxlen = (size_t)_size();
	if(len > maxlen) len = maxlen;

	memcpy(buf, buffer(), len);
	skip(len);
	return len;
}

// 调用 rfunc 读取最多 maxlen 的数据
size_t BufferPipe::_pump(reader_t reader, size_t maxlen)
{
	size_t plen = 0;
	/* 丢弃已经读取的部分 */
	// 写操作时才清空,COPY ON WRITE 类似
	trim();

	while(plen < maxlen)
	{
		size_t len = maxlen - plen;
		if(len > _stepSize) len = _stepSize;
		void* destBuf = NULL;

		len = lock(&destBuf, len);
		if(len > 0)
		{
			size_t rd = reader(destBuf, len);
			unlock(rd);

			plen += rd;

			if(rd <= 0) break;
		}
		else
		{
			break;
		}
	}

	return plen;
}

void BufferPipe::trunc()
{
	_buffer.trunc(false);
	_rdPos = 0;
	_wrPos = 0;
}


/*
* BufferPipe 的内部缓冲区指针
*/
const void* BufferPipe::buffer()
{
	return (const char*)_buffer.buffer() + _rdPos;
}

size_t BufferPipe::skip(size_t len)
{
	if(len > _wrPos - _rdPos) len = _wrPos - _rdPos;
	_rdPos += len;
	return len;
}

size_t BufferPipe::lock(void **buf, size_t len)
{
	_buffer.seek(_wrPos, SEEK_SET);
	return _buffer.lock(buf, len);
}

size_t BufferPipe::unlock(size_t len)
{
	size_t wr = _buffer.unlock(len);
	_wrPos += wr;
	return wr;
}