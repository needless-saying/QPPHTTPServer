#include "StdAfx.h"
#include "ChunkTransformer.h"


ChunkTransformer::ChunkTransformer(size_t packageSize)
	: IPipeTransformer(packageSize), _end(false)
{
}


ChunkTransformer::~ChunkTransformer()
{
}

size_t ChunkTransformer::_bound(const void* ibuf, size_t ilen)
{
	/* 分段数据格式: 长度(表示为16进制字符串) + CRLF + 数据 + CRLF */
	char chunkSize[200] = {0};
	sprintf(chunkSize, "%x", ilen);

	return strlen(chunkSize) + 2 + ilen + 2;
}

size_t ChunkTransformer::_transform(const void* ibuf, size_t ilen, void* obuf, size_t olen)
{
	/* 如果已经写入过结尾标记,则退出 */
	if(_end) return 0;
	if(ilen == 0)
	{
		_end = true;
	}

	/* 分段数据格式: 长度(表示为16进制字符串) + CRLF + 数据 + CRLF */
	char chunkSize[200] = {0};
	sprintf(chunkSize, "%x", ilen);

	assert(olen >= strlen(chunkSize) + 2 + ilen + 2);

	Buffer buf(obuf, olen);
	buf.write(chunkSize, strlen(chunkSize));
	buf.write("\r\n", 2);
	buf.write(ibuf, ilen);
	buf.write("\r\n", 2);

	return buf.size();
}