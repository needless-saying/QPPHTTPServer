/* Copyright (C) 2011 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#pragma once
#include <list>
#include "fastcgi.h"
#include "Buffer.h"
#include "Pipes/Pipe.h"

/*
* 包装FastCGI Record, FastCGI Record 是 FastCGI 协议的基本通讯单位.
* 一个 Record 的结构如下:
	typedef struct {
		unsigned char version;
		unsigned char type;
		unsigned char requestIdB1;
		unsigned char requestIdB0;
		unsigned char contentLengthB1;
		unsigned char contentLengthB0;
		unsigned char paddingLength;
		unsigned char reserved;

		unsigned char contentData[contentLength];
		unsigned char paddingData[paddingLength];
	} FCGI_Record;
* 前8个字节是 FCGI_Header 结构, 后面的数据由 FCGI_Header 指定的内容长度 contentLength 和 对齐长度 paddingLength 指定.
* 可以认为就是一段变长的内存缓冲.
* 一些特定类型的 Record contentData 有特定的预定义结构,如 FCGI_BeginRequestBody, FCGI_EndRequestBody 和 FCGI_UnknownTypeBody.
*
* class FCGIRecord 的目的是封装FastCGI record以方便编程访问. 包括 reader 和 writer 分别用于解析和生成 FastCGI record.
*/

/* 
* Name - Value pair 
*/
typedef struct 
{ 
	const unsigned char* data; 
	size_t len; 
}nv_t;
typedef std::pair<nv_t, nv_t> nvpair_t;
typedef std::list<nvpair_t> nvlist_t;

/*
* FCGI Record wrapper
*/
class FCGIRecord
{
private:
	Buffer _buffer; /* record 原始数据 */
	
public:
	FCGIRecord();
	FCGIRecord(const FCGIRecord& rh);
	FCGIRecord(const void* buf, size_t len);
	~FCGIRecord();

	FCGIRecord& operator = (const FCGIRecord& rh);
	size_t assign(const void* buf, size_t len);
	size_t assign(const Buffer* buf);
	size_t assign(IPipe* p);
	const void* buffer() const;
	size_t size() const;

	void reset();
	bool check(); /* 检测是否是一个完整的 record */

	bool setHeader(unsigned short requestId, unsigned char type);
	bool setBeginRequestBody(unsigned short role, bool keepConn); /* FCGI_BEGIN_REQUEST */
	bool setEndRequestBody(unsigned int appStatus, unsigned char protocolStatus); /* FCGI_END_REQUEST */
	bool setUnknownTypeBody(); /* FCGI_UNKNOWN_TYPE */
	bool addNameValuePair(nv_t n, nv_t v); /* FCGI_PARAMS,FCGI_GET_VALUES,FCGI_GET_VALUES_RESULT */
	bool addNameValuePair(const char* n, const char* v);
	size_t addBodyData(const void* buf, size_t len); /* 通用,FCGI_STDIN,FCGI_STDOUT,FCGI_STDERR,FCGI_DATA*/
	bool setEnd(unsigned char padding = 0); /* 打包 */

	bool getHeader(FCGI_Header &header);
	unsigned char getType();
	size_t getContentLength();
	bool getBeginRequestBody(unsigned short &role, bool &keepConn); /* FCGI_BEGIN_REQUEST */
	bool getEndRequestBody(unsigned int &appStatus, unsigned char &protocolStatus); /* FCGI_END_REQUEST */
	size_t getNameValuePairCount();
	bool getNameValuePair(int index, nv_t &n, nv_t &v); /* FCGI_PARAMS,FCGI_GET_VALUES,FCGI_GET_VALUES_RESULT */
	const void* getBodyData(); /* 通用,FCGI_STDIN,FCGI_STDOUT,FCGI_STDERR,FCGI_DATA*/
	size_t getBodyLength();
};