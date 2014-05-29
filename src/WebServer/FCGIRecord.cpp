#include "StdAfx.h"
#include "FCGIRecord.h"

/*
* 按照FCGI协议编码数值
*/
static void write_number(unsigned char* dest, unsigned int number, int bytes)
{
	/*
	* 高位在前
	*/
	if( 1 == bytes )
	{
		assert( number <= 127);
		*dest = static_cast<unsigned char>(number & 0x7F);

		/* 单字节数值的最高位必须为0 */
	}
	else if( 2 == bytes )
	{
		assert( number <= 65535);
		*dest = static_cast<unsigned char>((number >> 8) & 0xFF);
		*(dest + 1) = static_cast<unsigned char>(number & 0xFF);

		/* 双字节编码的数值最高位没要求 */
	}
	else if( 4 == bytes )
	{
		*dest = static_cast<unsigned char>((number >> 24) & 0xFF);
		*(dest + 1) = static_cast<unsigned char>((number >> 16) & 0xFF);
		*(dest + 2) = static_cast<unsigned char>((number >> 8) & 0xFF);
		*(dest + 3) = static_cast<unsigned char>(number & 0xFF);

		/* 最高位设置为1表示是一个4字节数值 */
		*dest |= 0x80;
	}
	else
	{
		assert(0);
	}
}

static unsigned int read_number14(const unsigned char* src, size_t *bytes)
{
	if( (*src >> 7) == 0 )
	{
		/* 单字节 0 ~ 127 */
		if(bytes) *bytes = 1;
		return *src;
	}
	else
	{
		/* 4字节 */
		// ((B3 & 0x7f) << 24) + (B2 << 16) + (B1 << 8) + B0];
		if(bytes) *bytes = 4;
		return ((*src & 0x7f) << 24) + (*(src + 1) << 16) + (*(src + 2) << 8) + *(src + 3);
	}
}

static unsigned int read_number2(const unsigned char* src)
{
	/* 2字节 */
	// ((B3 & 0x7f) << 24) + (B2 << 16) + (B1 << 8) + B0];
	return (*src << 8) + (*(src + 1));
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FCGIRecord::FCGIRecord()
	: _buffer(1024, 65535 + FCGI_HEADER_LEN) /* 这是一个 Fast CGI record 的最大长度 */
{
}

FCGIRecord::FCGIRecord(const void* buf, size_t len)
	: _buffer(1024, 65535 + FCGI_HEADER_LEN)
{
	assign(buf, len);
}

FCGIRecord::FCGIRecord(const FCGIRecord& rh)
	: _buffer(1024, 65535 + FCGI_HEADER_LEN)
{
	assign(rh.buffer(), rh.size());
}

FCGIRecord::~FCGIRecord()
{
}

size_t FCGIRecord::assign(const void* buf, size_t len)
{
	if(len < FCGI_HEADER_LEN)
	{
		return 0;
	}
	else
	{
		const FCGI_Header* header = reinterpret_cast<const FCGI_Header*>(buf);
		size_t recordLen = getContentLength() + FCGI_HEADER_LEN + header->paddingLength;
		if(len < recordLen)
		{
			// 长度不足
			return 0;
		}
		else
		{
			// 拷贝到内部缓冲区
			_buffer.trunc();
			_buffer.write(buf, recordLen);

			return recordLen;
		}
	}
}

size_t FCGIRecord::assign(IPipe* p)
{
	//if(p->size() < FCGI_HEADER_LEN)
	//{
	//	// 长度不足
	//	return 0;
	//}
	//else
	//{
	//	FCGI_Header header;
	//	p->peek(&header, FCGI_HEADER_LEN);

	//	size_t recordLen = getContentLength(header) + header.paddingLength + FCGI_HEADER_LEN;
	//	if(p->size() < recordLen)
	//	{
	//		// 长度不足
	//		return 0;
	//	}
	//	else
	//	{
	//		void* buf = NULL;
	//		size_t lockedLen = _buffer.lock(&buf, recordLen);
	//		if(lockedLen < recordLen)
	//		{
	//			// 空间不足
	//			_buffer.unlock(0);
	//			assert(0);
	//			return 0;
	//		}
	//		else
	//		{
	//			p->read(buf, recordLen);
	//			_buffer.unlock(recordLen);
	//			return recordLen;
	//		}
	//	}
	//}
	return 0;
}

size_t FCGIRecord::assign(const Buffer* buf)
{
	return assign(buf->buffer(), buf->size());
}

FCGIRecord& FCGIRecord::operator = (const FCGIRecord& rh)
{
	assign(rh.buffer(), rh.size());
	return *this;
}

const void* FCGIRecord::buffer() const
{
	return _buffer.buffer();
}

size_t FCGIRecord::size() const
{
	return _buffer.size();
}

void FCGIRecord::reset()
{
	_buffer.trunc(false);
}

bool FCGIRecord::check()
{
	FCGI_Header header;
	if(getHeader(header))
	{
		return getContentLength() == (_buffer.size() - FCGI_HEADER_LEN - header.paddingLength);
	}
	return false;
}

bool FCGIRecord::setHeader(unsigned short requestId, unsigned char type)
{
	FCGI_Header header;
	memset(&header, 0, FCGI_HEADER_LEN);

	header.version = FCGI_VERSION_1;
	header.type = type;
	write_number(&header.requestIdB1, requestId, 2);
	

	_buffer.trunc();
	_buffer.write(&header, FCGI_HEADER_LEN);
	return true;
}

bool FCGIRecord::getHeader(FCGI_Header &header)
{
	if( _buffer.size() >= FCGI_HEADER_LEN )
	{
		const FCGI_Header* headerPtr = reinterpret_cast<const FCGI_Header*>(_buffer.buffer());
		memcpy(&header, headerPtr, FCGI_HEADER_LEN);
		return true;
	}
	else
	{
		return false;
	}
}

unsigned char FCGIRecord::getType()
{
	FCGI_Header header;
	if(getHeader(header))
	{
		return header.type;
	}
	return 0;
}

size_t FCGIRecord::getContentLength()
{
	FCGI_Header header;
	if(getHeader(header))
	{
		return read_number2(&header.contentLengthB1);
	}
	return 0;
}

bool FCGIRecord::setBeginRequestBody(unsigned short role, bool keepConn)
{
	if(getType() != FCGI_BEGIN_REQUEST) return false;

	FCGI_BeginRequestBody body;
	memset(&body, 0, sizeof(FCGI_BeginRequestBody));

	write_number(&body.roleB1, role, 2);
	if(keepConn) body.flags |= FCGI_KEEP_CONN;

	_buffer.write(&body, sizeof(FCGI_BeginRequestBody));
	return true;
}

bool FCGIRecord::setEndRequestBody(unsigned int appStatus, unsigned char protocolStatus)
{
	if(getType() != FCGI_END_REQUEST) return false;

	FCGI_EndRequestBody body;
	memset(&body, 0, sizeof(FCGI_EndRequestBody));

	write_number(&body.appStatusB3, appStatus, 4);
	body.protocolStatus = protocolStatus;

	_buffer.write(&body, sizeof(FCGI_EndRequestBody));
	return true;
}

bool FCGIRecord::setUnknownTypeBody()
{
	if(getType() != FCGI_UNKNOWN_TYPE) return false;

	FCGI_UnknownTypeBody body;
	memset(&body, 0, sizeof(FCGI_UnknownTypeBody));

	body.type = static_cast<unsigned char>(getType());
	
	_buffer.write(&body, sizeof(FCGI_UnknownTypeBody));
	return true;
}

bool FCGIRecord::addNameValuePair(const char* nstr, const char* vstr)
{
	nv_t n, v;
	n.len = strlen(nstr);
	n.data = reinterpret_cast<const byte*>(nstr);

	v.len = strlen(vstr);
	v.data = reinterpret_cast<const byte*>(vstr);

	return addNameValuePair(n, v);
}

bool FCGIRecord::addNameValuePair(nv_t n, nv_t v)
{
	if(getType() != FCGI_PARAMS) return false;
	unsigned char number[4];
	if(n.len <= 127)
	{
		write_number(number, n.len, 1);
		_buffer.write(number, 1);
	}
	else
	{
		write_number(number, n.len, 4);
		_buffer.write(number, 4);
	}
	if(v.len <= 127)
	{
		write_number(number, v.len, 1);
		_buffer.write(number, 1);
	}
	else
	{
		write_number(number, v.len, 4);
		_buffer.write(number, 4);
	}
	
	if(n.data != NULL)
	{
		_buffer.write(n.data, n.len);
	}
	if(v.data != NULL)
	{
		_buffer.write(v.data, v.len);
	}

	return true;
}

size_t FCGIRecord::addBodyData(const void* buf, size_t len)
{
	return _buffer.write(buf, len);
}

bool FCGIRecord::setEnd(unsigned char padding /* = 0 */)
{
	if(_buffer.size() < FCGI_HEADER_LEN) return false;

	/* 把contentlength 写到 FCGI_Header 相应的字段中 */
	size_t contentLength = _buffer.size() - FCGI_HEADER_LEN;
	
	FCGI_Header* header = NULL;
	_buffer.seek(0, SEEK_SET);
	_buffer.lock((void**)&header, FCGI_HEADER_LEN + contentLength);

	write_number(&header->contentLengthB1, contentLength, 2);
	header->paddingLength = padding;

	_buffer.unlock(FCGI_HEADER_LEN + contentLength);
	
	/* 填充对齐 */
	if(padding > 0)
	{
		char c = 0;
		for(unsigned char i = 0; i < padding; ++i)
		{
			_buffer.write(&c, 1);
		}
	}
	return true;
}

size_t FCGIRecord::getBodyLength()
{
	if( _buffer.size() > FCGI_HEADER_LEN )
	{
		return _buffer.size() - FCGI_HEADER_LEN;
	}
	else
	{
		return 0;
	}
}

const void* FCGIRecord::getBodyData()
{
	if( getBodyLength() > 0 )
	{
		return reinterpret_cast<const char*>(_buffer.buffer()) + FCGI_HEADER_LEN;
	}
	else
	{
		return NULL;
	}
}

bool FCGIRecord::getBeginRequestBody(unsigned short &role, bool &keepConn)
{
	if( !check() || getType() != FCGI_BEGIN_REQUEST ) return false;

	const FCGI_BeginRequestRecord *recordPtr = reinterpret_cast<const FCGI_BeginRequestRecord*>(getBodyData());
	role = read_number2(&recordPtr->body.roleB1);
	keepConn = (recordPtr->body.flags & FCGI_KEEP_CONN) != 0;

	return true;
}

bool FCGIRecord::getEndRequestBody(unsigned int &appStatus, unsigned char &protocolStatus)
{
	if( !check() || getType() != FCGI_END_REQUEST ) return false;

	size_t bytes = 0;
	const FCGI_EndRequestRecord *recordPtr = reinterpret_cast<const FCGI_EndRequestRecord*>(getBodyData());
	appStatus = read_number14(&recordPtr->body.appStatusB3, &bytes);
	protocolStatus = recordPtr->body.protocolStatus;

	return true;
}

size_t FCGIRecord::getNameValuePairCount()
{
	if( !check() || FCGI_PARAMS != getType() ) return 0;

	size_t count = 0;
	size_t nameBytes = 0;
	size_t valueBytes = 0;
	size_t nameLen = 0;
	size_t valueLen = 0;
	size_t pos = FCGI_HEADER_LEN;

	const unsigned char* content = reinterpret_cast<const unsigned char*>(getBodyData());
	while( pos < _buffer.size())
	{
		nameLen = read_number14(content, &nameBytes);
		valueLen = read_number14(content + nameBytes, &valueBytes);

		pos += nameBytes + nameLen + valueBytes + valueLen;
		content += nameBytes + nameLen + valueBytes + valueLen;
		++count;
	}

	return count;
}

bool FCGIRecord::getNameValuePair(int index, nv_t &n, nv_t &v)
{
	if( !check() || FCGI_PARAMS != getType() ) return false;

	size_t count = 0;
	size_t nameBytes = 0;
	size_t valueBytes = 0;
	size_t nameLen = 0;
	size_t valueLen = 0;
	size_t pos = FCGI_HEADER_LEN;

	const unsigned char* content = reinterpret_cast<const unsigned char*>(getBodyData());
	while( pos < _buffer.size())
	{
		nameLen = read_number14(content, &nameBytes);
		valueLen = read_number14(content + nameBytes, &valueBytes);

		pos += nameBytes + nameLen + valueBytes + valueLen;
		content += nameBytes + nameLen + valueBytes + valueLen;
		if(++count == index)
		{
			const unsigned char* nameData = content + nameBytes + valueBytes;
			const unsigned char* valueData = nameData + nameLen;

			if(n.data == NULL)
			{
				n.len = nameLen;
			}
			else if(n.len >= nameLen)
			{
				n.data = nameData;
				n.len = nameLen;
			}
			else
			{
				break;
			}

			if(v.data == NULL)
			{
				v.len = valueLen;
			}
			else if(v.len >= valueLen)
			{
				v.data = valueData;
				v.len = valueLen;
			}
			else
			{
				break;
			}
			return true;
		}
	}

	return false;
}
