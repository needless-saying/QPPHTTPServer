/* Copyright (C) 2011 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#include "StdAfx.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>

#include "HTTPLib.h"
#include "HTTPContent.h"
#include "Pipes/FilePipe.h"
#include "Pipes/BufferPipe.h"

static void get_content_type_from_filename(std::string& strType, const char* pszFileName)
{
	strType = "application/octet-stream";

	const char *pExt = strrchr(pszFileName, '.');
	if(pExt && strlen(pExt) < 1024)
	{
		char szExt[1025];
		strcpy(szExt, pExt + 1);

		if(stricmp(szExt, "jpg") == 0)
		{
			strType =  "image/jpeg";
		}
		else if(stricmp(szExt, "txt") == 0)
		{
			strType = "text/plain";
		}
		else if(stricmp(szExt, "htm") == 0)
		{
			strType = "text/html";
		}
		else if(stricmp(szExt, "html") == 0)
		{
			strType = "text/html";
		}
		else if(stricmp(szExt, "gif") == 0)
		{
			strType = "image/gif";
		}
		else if(stricmp(szExt, "png") == 0)
		{
			strType = "image/png";
		}
		else if(stricmp(szExt, "bmp") == 0)
		{
			strType = "image/x-xbitmap";
		}
		else
		{
		}
	}
}

static void map_content_type_string(std::string& strType, int t)
{
	strType = "application/octet-stream";

	if(t == CONTENT_TYPE_BINARY)
	{
	}
	else if(CONTENT_TYPE_TEXT == t)
	{
		strType = "text/plain";
	}
	else if(CONTENT_TYPE_HTML == t)
	{
		strType = "text/html";
	}
	else
	{
	}
}

static void format_range(std::string& str, __int64 fr, __int64 to, __int64 sz)
{
	char szRanges[300] = {0};
	if(fr < 0) fr = 0;
	if(fr >= sz) fr = sz - 1;
	if(to < 0) to = sz - 1;
	if(to >= sz) to = sz - 1;
	sprintf(szRanges, "bytes %lld-%lld/%lld", fr, to, sz);
	str = szRanges;
}

static void build_etag_string(std::string& str, const void* buf, size_t len)
{
	char szLength[201] = {0};
	ltoa(len, szLength, 10);

	// 如果是内存数据, 根据大小和取若干个字节的16进制字符创建.
	unsigned char szValue[ETAG_BYTE_SIZE + 1];
	for(int i = 0; i < ETAG_BYTE_SIZE; ++i)
	{
		szValue[i] = ((const char*)buf)[(len / ETAG_BYTE_SIZE) * i];
	}

	str = to_hex(szValue, ETAG_BYTE_SIZE);
	str += ":";
	str += szLength;
}

static void build_etag_string(std::string& str, struct _stat64 *fileInf)
{
	char szLength[201] = {0};
	_i64toa(fileInf->st_size, szLength, 10);

	// 根据文件大小和修改日期创建. [ETag: ec5ee54c00000000:754998030] 修改时间的HEX值:文件长度
	// 确保同一个资源的 ETag 是同一个值.
	// 即使客户端请求的只是这个资源的一部分.
	// 断点续传客户端根据 ETag 的值确定下载的几个部分是不是同一个文件.
	str = to_hex((const unsigned char*)&(fileInf->st_mtime), sizeof(fileInf->st_mtime));
	str += ":";
	str += szLength;
}

HTTPContent::HTTPContent()
	: _content(NULL), _contentLength(0), _acceptRanges(true)
{
}

HTTPContent::~HTTPContent()
{
	close();
}

bool HTTPContent::open(const std::string &fileName, __int64 from, __int64 to)
{
	assert(_content == NULL);
	struct _stat64 fileInf;
	if( 0 != _stat64(fileName.c_str(), &fileInf))
	{
		return false;
	}
	else
	{
		_content = new FilePipe(fileName, from, to);

		get_content_type_from_filename(_contentType, fileName.c_str());
		format_range(_contentRange, from, to, fileInf.st_size);
		build_etag_string(_etag, &fileInf);
		_contentLength = fileInf.st_size;
		_lastModify = format_http_date(&fileInf.st_mtime);
		_acceptRanges = true;
		
		return true;
	}
}

bool HTTPContent::open(const char* buf, size_t len, int type)
{
	assert(_content == NULL);
	__int64 ltime;
	_time64(&ltime);

	_content = new BufferPipe(len + 1, len);
	_content->write(buf, len);
	
	map_content_type_string(_contentType, type);
	format_range(_contentRange, 0, len - 1, len);
	build_etag_string(_etag, buf, len);
	_contentLength = len;
	_lastModify = format_http_date(&ltime);
	_acceptRanges = false;
	return true;
}

/*
* 读取目录列表输出为一个HTML文本流.
*/
bool HTTPContent::open(const std::string &urlStr, const std::string &filePath)
{
	assert(_content == NULL);
	char buffer[_MAX_PATH + 100] = {0};
	char sizeBuf[_MAX_PATH + 100] = {0};

	// 创建缓存,生成一个UTF8 HTML文本流,包含了文件列表.
	BufferPipe* bp = new BufferPipe(MAX_DIRECTORY_LIST_SIZE, 2 * K_BYTES);
	_content = bp;
	
	// 1. 输出HTML头,并指定UTF-8编码格式
	puts("<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"/></head>");
	puts("<body>");

	// 2. 输出路径
	//(1). 输出第一项 根目录
	puts("<A href=\"/\">/</A>");

	//(2). 其它目录
	std::string::size_type st = 1;
	std::string::size_type stNext = 1;
	while( (stNext = urlStr.find('/', st)) != std::string::npos)
	{
		std::string strDirName =  urlStr.substr(st, stNext - st + 1);
		std::string strSubUrl = urlStr.substr(0, stNext + 1);

		puts("&nbsp;|&nbsp;");

		puts("<A href=\"");
		puts(AtoUTF8(strSubUrl).c_str());
		puts("\">");
		puts(AtoUTF8(strDirName).c_str());
		puts("</A>");
		
		// 下一个目录
		st = stNext + 1;
	}
	puts("<br /><hr />");

	// 3. 列出当前目录下的所有文件
	std::string strFullName;
	strFullName = filePath;
	if(strFullName.back() != '\\') strFullName += '\\'; // 如果不是以'\\'结尾的文件路径,则补齐. 注意一个原则:URL以正斜杠分隔;文件名以反斜杠分隔
	strFullName += "*";

	std::string strFiles(""); // 普通文件写在这个字符串中.

	__finddata64_t fileinfo;
	intptr_t findRet = _findfirst64(strFullName.c_str(), &fileinfo);
	if( -1 != findRet )
	{
		do 
		{
			// 跳过 . 文件
			if( stricmp(fileinfo.name, ".") == 0 || 0 == stricmp(fileinfo.name, "..") )
			{
				continue;
			}

			// 跳过系统文件和隐藏文件
			if( fileinfo.attrib & _A_SYSTEM || fileinfo.attrib & _A_HIDDEN )
			{
				continue;
			}

			// 输出子目录或者
			if( fileinfo.attrib & _A_SUBDIR )
			{
				// 如果是子目录,直接写入

				// 最后修改时间
				_ctime64_s( buffer, _countof(buffer), &fileinfo.time_write );
				puts(AtoUTF8(buffer).c_str());

				// 目录名需要转换为UTF8编码
				sprintf(buffer, "%s/", fileinfo.name);
				std::string fileurl = AtoUTF8(urlStr.c_str());
				std::string filename = AtoUTF8(buffer);

				puts("&nbsp;&nbsp;");
				puts("<A href=\"");
				puts(fileurl.c_str());
				puts(filename.c_str());
				puts("\">");
				puts(filename.c_str());
				puts("</A>");

				// 写入目录标志
				puts("&nbsp;&nbsp;[DIR]");

				// 换行
				puts("<br />");
			}
			else
			{
				// 普通文件,写入到一个缓冲的字符串string变量内,循环外再合并.这样,所有的目录都在前面,文件在后面
				_ctime64_s( buffer, _countof(buffer), &fileinfo.time_write );
				strFiles += AtoUTF8(buffer);

				// 文件名转换为UTF8编码再写入
				std::string filename = AtoUTF8(fileinfo.name);
				std::string fileurl = AtoUTF8(urlStr.c_str());

				strFiles += "&nbsp;&nbsp;";
				strFiles += "<A href=\"";
				strFiles += fileurl;
				strFiles += filename;
				strFiles += "\">";
				strFiles += filename;
				strFiles += "</A>";

				// 文件大小
				// 注: 由于Windows下 wsprintf 不支持 %f 参数,所以只好用 sprintf 了
				double filesize = 0;
				if( fileinfo.size >= G_BYTES)
				{
					filesize = (fileinfo.size * 1.0) / G_BYTES;
					sprintf(sizeBuf, "%.2f&nbsp;GB", filesize);
				}
				else if( fileinfo.size >= M_BYTES ) // MB
				{
					filesize = (fileinfo.size * 1.0) / M_BYTES;
					sprintf(sizeBuf, "%.2f&nbsp;MB", filesize);
				}
				else if( fileinfo.size >= K_BYTES ) //KB
				{
					filesize = (fileinfo.size * 1.0) / K_BYTES;
					sprintf(sizeBuf, "%.2f&nbsp;KB", filesize);
				}
				else // Bytes
				{
					sprintf(sizeBuf, "%lld&nbsp;Bytes", fileinfo.size);
				}
			
				strFiles += "&nbsp;&nbsp;";
				strFiles += sizeBuf;

				// 换行
				strFiles += "<br />";
			}
		} while ( -1 != _findnext64(findRet, &fileinfo));
		
		_findclose(findRet);
	}

	// 把文件字符串写入到 Content 中.
	if(strFiles.size() > 0)
	{
		puts(strFiles.c_str());
	}

	// 4. 输出结束标志.
	puts("</body></html>");

	/*
	* 生成响应头信息
	*/
	__int64 ltime;
	_time64(&ltime);

	map_content_type_string(_contentType, CONTENT_TYPE_HTML);
	format_range(_contentRange, 0, bp->size() - 1, bp->size());
	build_etag_string(_etag, bp->buffer(), (size_t)bp->size());
	_contentLength = bp->size();
	_lastModify = format_http_date(&ltime);
	_acceptRanges = false;
	return true;
}

size_t HTTPContent::puts(const char* str)
{
	assert(_content);
	return _content->write(str, strlen(str));
}

void HTTPContent::close()
{
	if(_content)
	{
		delete _content;
		_content = NULL;
	}
}

std::string& HTTPContent::contentType()
{
	return _contentType;
}

std::string& HTTPContent::contentRange()
{
	return _contentRange;
}

__int64 HTTPContent::contentLength()
{
	return _contentLength;
}

std::string& HTTPContent::lastModified()
{
	return _lastModify;
}

std::string& HTTPContent::etag()
{
	return _etag;
}

bool HTTPContent::acceptRanges()
{
	return _acceptRanges;
}


__int64 HTTPContent::_size()
{
	if(_content) return _content->size();
	return 0;
}

size_t HTTPContent::_pump(reader_t reader, size_t maxlen)
{
	return 0;
}

size_t HTTPContent::_read(void* buf, size_t len)
{
	if(_content)
	{
		return _content->read(buf, len);
	}
	return 0;
}