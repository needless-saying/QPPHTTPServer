#include "StdAfx.h"
#include "IPipeTransformer.h"


IPipeTransformer::IPipeTransformer(size_t packageSize)
	: _bp(SIZE_MAX, 4096), _packageSize(packageSize), _packageBuf(NULL)
{
}


IPipeTransformer::~IPipeTransformer(void)
{
	if(_packageBuf) delete []_packageBuf;
}

bool IPipeTransformer::_skip()
{
	return false;
}

__int64 IPipeTransformer::_size()
{
	return _bp.size();
}

size_t IPipeTransformer::doTransform(const void* ibuf, size_t ilen)
{
	assert(ilen <= _packageSize);

	void* obuf = NULL;
	size_t olen = _bound(ibuf, ilen);

	if(olen > 0)
	{
		size_t lockedLen = _bp.lock(&obuf, olen);
		assert(lockedLen >= olen);

		// 变换
		olen = _transform(ibuf, ilen, obuf, olen);

		// 解锁缓存
		_bp.unlock(olen);
	}
	else
	{
	}

	return olen;
}

size_t IPipeTransformer::_read(void* buf, size_t len)
{
	size_t rd = 0;

	while(len > 0)
	{
		if(_bp.size() > 0)
		{
			// 先把缓存内的数据读出
			size_t r = _bp.read(buf, len);
			buf = (char*)buf + r;
			len -= r;
		}
		else
		{
			// 从 prev 中读取一个 package,变换,写入缓存
			if(prev())
			{
				if(_packageBuf == NULL)
				{
					_packageBuf = new byte[_packageSize];
					assert(_packageBuf);
				}

				byte* ibuf = _packageBuf;
				size_t ilen = prev()->read(ibuf, _packageSize);
				if(doTransform(ibuf, ilen) > 0)
				{
				}
				else
				{
					assert(0);
					break;
				}
			}
			else
			{
				break;
			}
		}
	}

	return rd;
}

size_t IPipeTransformer::_pump(reader_t reader, size_t maxlen)
{
	// 变换器不需要把数据读入到自身缓存中
	return 0;
	//size_t plen = 0;

	//if(_bp.size() <= 0)
	//{
	//	// 从 prev 中读取一个 package,变换,写入缓存
	//	if(prev())
	//	{
	//		if(_packageBuf == NULL)
	//		{
	//			_packageBuf = new byte[_packageSize];
	//			assert(_packageBuf);
	//		}

	//		byte* ibuf = _packageBuf;
	//		size_t ilen = maxlen;
	//		if(ilen > _packageSize) ilen = _packageSize;

	//		ilen = reader(ibuf, _packageSize);
	//		size_t tlen = doTransform(ibuf, ilen);
	//		assert(tlen > 0);

	//		plen = ilen;
	//	}
	//}

	//return plen;
}
