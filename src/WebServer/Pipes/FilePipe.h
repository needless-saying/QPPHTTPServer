#pragma once
#include "pipe.h"
#include "..\OSFile.h"

class FilePipe : public IPipe
{
private:
	OSFile _file;
	__int64 _rPos;
	__int64 _wPos;
	bool _readOnly;
	std::string _fileName;

protected:	
	size_t _read(void* buf, size_t len);
	size_t _pump(reader_t reader, size_t maxlen);
	__int64 _size();

public:
	/* Ö»¶Á */
	FilePipe(const std::string& fileName, __int64 from, __int64 to);

	/* ¶ÁÐ´ */
	FilePipe(const std::string& tmpFileName);

	~FilePipe();
};

