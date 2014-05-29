/* Copyright (C) 2011 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#pragma once
#include "HTTPLib.h"

/*
* HTTPContent
* 用作包装 HTTP 响应报文中的内容部分,有可能包含以下几种数据类型:
* 1. 一段文本,如 HTTP 500 等错误提示.
* 2. 一个目录文本流,浏览目录时得到的文件列表.
* 3. 一个只读文件的部分或者全部.
*
*/

#define CONTENT_TYPE_FILE 1
#define CONTENT_TYPE_TEXT 2
#define CONTENT_TYPE_BINARY 3
#define CONTENT_TYPE_HTML 4

class HTTPContent : public IPipe
{
private:
	IPipe *_content;
	std::string _contentType;
	std::string _contentRange;
	std::string _etag;
	std::string _lastModify;
	bool _acceptRanges;
	__int64 _contentLength;

	size_t puts(const char* str);

protected:
	// from pipe
	size_t _read(void* buf, size_t len);
	size_t _pump(reader_t reader, size_t maxlen);
	__int64 _size();

public:
	HTTPContent();
	virtual ~HTTPContent();
	bool open(const std::string &fileName, __int64 from = 0, __int64 to = 0); /* 打开一个只读文件 */
	bool open(const std::string &urlStr, const std::string &filePath); /* 打开一个目录 */
	bool open(const char* buf, size_t len, int type); /* 打开一段 mem buffer */
	void close();

	std::string& contentType();
	std::string& contentRange();
	__int64 contentLength();
	std::string& lastModified();
	std::string& etag();
	bool acceptRanges();
};
