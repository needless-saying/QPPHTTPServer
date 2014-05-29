#include "StdAfx.h"
#include "HTTPLib.h"
#include "FCGIRecord.h"
#include "FCGITransformer.h"

FCGITransformer::FCGITransformer(u_char fcgiType, u_short fcgiId, size_t packageSize)
	: IPipeTransformer(packageSize), _fcgiId(fcgiId), _fcgiType(fcgiType), _end(false)
{
}

FCGITransformer::~FCGITransformer()
{
}

// 计算输出数据的长度
size_t FCGITransformer::_bound(const void* ibuf, size_t ilen)
{
	return ilen + FCGI_HEADER_LEN;
}

// 变换数据 input buffer -> output buffer
size_t FCGITransformer::_transform(const void* ibuf, size_t ilen, void* obuf, size_t olen)
{
	if(olen < ilen + FCGI_HEADER_LEN) return 0;

	size_t translen = 0;
	if(ilen == 0)
	{
		if(_end)
		{
			// 已经结束,返回0
		}
		else
		{
			// 写入结束
			_end = true;

			FCGIRecord record;
			record.setHeader(_fcgiId, _fcgiType);
			record.setEnd();
			memcpy(obuf, record.buffer(), record.size());
			translen = record.size();
		}
	}
	else
	{
		FCGIRecord record;
		record.setHeader(_fcgiId, _fcgiType);
		record.addBodyData(ibuf, ilen);
		record.setEnd();
		memcpy(obuf, record.buffer(), record.size());
		translen = record.size();
	}
	return translen;
}