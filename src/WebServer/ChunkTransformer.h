#pragma once
#include "IPipeTransformer.h"

class ChunkTransformer : public IPipeTransformer
{
private:
	bool _end;
	size_t _transform(const void* ibuf, size_t ilen, void* obuf, size_t olen);
	size_t _bound(const void* ibuf, size_t ilen);

public:
	ChunkTransformer(size_t packageSize);
	~ChunkTransformer();
};

