#include "StdAfx.h"
#include "FilePipe.h"
#include "..\ATW.h"

FilePipe::FilePipe(const std::string& fileName, __int64 from, __int64 to)
	: _readOnly(true), _rPos(from), _wPos(to + 1), _fileName(fileName)
{
	if(_file.open(AtoT(fileName).c_str(), OSFile::r, false))
	{
		__int64 fz = _file.size();

		if(fz > 0)
		{
			if(_rPos > fz)
			{
				_rPos = fz;
			}

			if(0 == _wPos || _wPos > fz)
			{
				_wPos = fz;
			}
		}
		else
		{
			_rPos = 0;
			_wPos = 0;
		}
	}
}

FilePipe::FilePipe(const std::string& tmpFileName)
	: _wPos(0), _rPos(0), _readOnly(false), _fileName(tmpFileName)
{
	/* 开始写的时候再创建临时文件 */
}


FilePipe::~FilePipe()
{
	_file.close();
}

size_t FilePipe::_read(void* buf, size_t len)
{
	size_t rd = 0;
	if(len > _wPos - _rPos) len = (size_t)(_wPos - _rPos);

	if(_file.isopen() && len > 0)
	{
		_file.seek(_rPos, OSFile::s_set);
		rd = _file.read(buf, len);
		_rPos += rd;

		/* 对于非只读模式,当读指针追上写指针时,可以清空文件 */
		if(!_readOnly && _rPos == _wPos && _file.trunc())
		{
			_rPos = 0;
			_wPos = 0;
		}
	}
	return rd;
}

#define SINGLE_PUMP_SIZE 4096
size_t FilePipe::_pump(reader_t reader, size_t maxlen)
{
	if(_readOnly) return 0;

	if(!_file.isopen())
	{
		if(!_file.open(AtoT(_fileName).c_str(), OSFile::rw, true))
		{
			assert(0);
			return 0;
		}
	}

	_file.seek(_wPos, OSFile::s_set);

	size_t plen = 0;
	byte buf[SINGLE_PUMP_SIZE];
	
	while(plen < maxlen)
	{
		size_t len = maxlen - plen;
		if(len > SINGLE_PUMP_SIZE) len = SINGLE_PUMP_SIZE;

		size_t rd = reader(buf, len);
		if(rd > 0)
		{
			size_t fwr = _file.write(buf , len);
			assert(fwr == rd);
			plen += rd;
			_wPos += rd;
		}
		else
		{
			break;
		}
	}

	return plen;
}

__int64 FilePipe::_size()
{
	return _wPos - _rPos;
}