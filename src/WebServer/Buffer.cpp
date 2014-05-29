#include "StdAfx.h"
#include "Buffer.h"

Buffer::Buffer(size_t memInc, size_t maxSize)
	: _memInc(memInc), _maxSize(maxSize), _buffer(NULL), _bufLen(0), _pos(0), _len(0), _innerBuf(true), _readOnly(false)
{
	ASSERT(_memInc <= _maxSize);
	if( _memInc > _maxSize ) _memInc = _maxSize;
}

Buffer::Buffer(void* buf, size_t len)
	: _memInc(0), _maxSize(len), _buffer((char*)buf), _bufLen(len), _pos(0), _len(len), _innerBuf(false), _readOnly(false)
{
}

Buffer::Buffer(const void* buf, size_t len)
	: _memInc(0), _maxSize(len), _buffer((char*)buf), _bufLen(len), _pos(0), _len(len), _innerBuf(false), _readOnly(true)
{
}

Buffer::~Buffer()
{
	trunc(true);
}

/*
* 复制时总是在新对象内重新分配内存
* 因为重新分配了内存,似乎也不是完全意义上的"复制"
*/
Buffer::Buffer(const Buffer &rh)
	: _buffer(NULL), _bufLen(0), _innerBuf(true)
{
	assign(rh);
}

Buffer& Buffer::operator = (const Buffer& rh)
{
	return assign(rh);
}

Buffer& Buffer::assign(const Buffer& rh)
{
	// 清空
	trunc(true);

	_innerBuf = true;
	_bufLen = rh._bufLen;
	_buffer = new char[_bufLen];
	_pos = rh._pos;
	_len = rh._len;
	_maxSize = rh._maxSize;
	_memInc = rh._memInc;
	_readOnly = false;

	assert(_buffer);
	memcpy(_buffer, rh._buffer, _bufLen);
	return *this;
}

void Buffer::trunc(bool freeBuf /* = true */)
{
	if( freeBuf && _innerBuf )
	{
		if(_buffer != NULL) delete[]_buffer;
		_buffer = NULL;
		_bufLen = 0;
	}

	_pos = 0;
	_len = 0;
}

size_t Buffer::size() const
{
	return _len;
}

size_t Buffer::capacity() const
{
	return _bufLen;
}

size_t Buffer::space()
{
	return _bufLen - _pos;
}

size_t Buffer::reserve(size_t s)
{
	// 已经拥有足够的空间
	if( space() >= s ) return s;

	// 1. 一次最少分配 _memInc 大小的内存.
	size_t incSize = s - space();
	if( incSize < _memInc ) incSize = _memInc;

	// 2. 最多只能有 _maxSize 大小的内存.
	size_t newSize = _bufLen + incSize;
	if( newSize > _maxSize ) newSize = _maxSize;

	if( newSize <= _bufLen )
	{
	}
	else
	{
		char *tmp = new char[newSize];
		if(tmp)
		{
			memset(tmp, 0, newSize);
			if(_buffer)
			{
				memcpy(tmp, _buffer, _bufLen);
				delete []_buffer;
			}
			_buffer = tmp;
			_bufLen = newSize;
		}
	}
	return space() >= s ? s : space();
}

size_t Buffer::write(const void *buf, size_t len)
{
	if(_readOnly) return 0;

	assert(buf);
	len = reserve(len);
	if(len > 0)
	{
		memcpy(_buffer + _pos, buf, len);	
		_pos += len;
		if(_pos > _len) _len = _pos;
	}
	return len;
}

size_t Buffer::read(void *buf, size_t len)
{
	assert(buf);
	if(_len - _pos < len) len = _len - _pos;

	if(len > 0)
	{
		memcpy(buf, _buffer + _pos, len);
		_pos += len;
	}

	return len;
}

size_t Buffer::tell() const
{
	return _pos;
}

int Buffer::seek(long offset, int origin)
{
	if(origin == SEEK_SET)
	{
		if(offset >= 0 && offset <= (long)_len)
		{
			_pos = offset;
			return 0;
		}
	}
	else if(origin == SEEK_CUR)
	{
		if(offset >= 0)
		{
			if(offset <= (long)(_len - _pos))
			{
				_pos += offset;
				return 0;
			}
		}
		else
		{
			offset *= -1;
			if(offset <= (long)_pos)
			{
				_pos -= offset;
				return 0;
			}
		}
	}
	else
	{
		if(offset >= 0 && offset <= (long)_len)
		{
			_pos = _len - offset;
			return 0;
		}
	}

	return -1;
}

bool Buffer::eof() const
{
	return _pos >= _len;
}

size_t Buffer::lock(void **buf, size_t len)
{
	if(_readOnly)
	{
		buf = NULL;
		return 0;
	}

	len = reserve(len);
	*buf = _buffer + _pos;
	return len;
}

size_t Buffer::unlock(size_t len)
{
	if(_readOnly)
	{
		return 0;
	}

	if(len > _bufLen - _pos) len = _bufLen - _pos;
	_pos += len;
	if(_pos > _len) _len = _pos;

	return len;
}

const void* Buffer::buffer() const
{
	return _buffer;
}