/* Copyright (C) 2012 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#pragma once
#include <string>
#include <utility>
#include <vector>

#ifdef UNICODE
typedef std::wstring tstring;
typedef wchar_t tchar;
#define URI_TEXT(x) L ## x
#else
typedef std::string tstring;
typedef char tchar;
#define URI_TEXT(x) x
#endif

class Uri
{
public:

	// 公开的类型定义
	typedef std::pair<tstring, tstring> param_t;
	typedef std::vector<param_t> param_list_t;

	// url 协议类型
	typedef enum _protocol_t
	{
		file = 1,
		gopher,
		http,
		https,
		ftp,
		mailto,
		mms,
		news,
		nntp,
		telnet,
		wais,
		prospero,

		unknown = 100
	}protocol_t;

private:
	// 原始的url字符串
	protocol_t _defaultProtoType;
	protocol_t _protType;
	unsigned int _port;
	tstring _urlStr;
	tstring _hostname;
	tstring _path;
	tstring _parameters;
	tstring _content; // 除了协议名之外的全部
	tstring _username;
	tstring _password;
	
	param_list_t _httpParamList;// http 参数列表
	
private:
	static protocol_t _mapProtocolType(const tstring& protoName);
	static unsigned int _getDefaultPort(protocol_t protoType);
	static tstring _getSubStr(const tstring& str, tstring::const_iterator& first, tstring::const_iterator& second);
	bool _parseUrl(const tstring& urlStr);
	void _reset();

public:
	// 构造函数
	Uri(protocol_t defaultProtoType = http);
	Uri(const tstring& urlStr, protocol_t defaultProtoType = http);
	Uri(const Uri& rh);
	~Uri(void);

	// 静态函数,URL编码和解码 (UTF8)
	static tstring encode(const tstring& inputStr);
	static tstring decode(const tstring& inputStr);

	// 是否是一个有效的url
	bool isValid();

	// 所有协议通用的,返回url的各个部分
	protocol_t protocol();
	unsigned int port();
	const tstring& uri();
	const tstring& content();
	const tstring& username();
	const tstring& password();
	const tstring& hostname();
	const tstring& path();
	const tstring& parameters();

	// 各协议专用函数
	const param_list_t& httpParamList();
	bool httpQuery(const tstring& paramKey, tstring& paramValue);

	// 设置url
	Uri& operator = (const Uri& rh);
	Uri& operator = (const tstring& urlStr);
	bool operator == (const Uri& rh);

	// 使这样的语法可以运行 if (url) 或者 if(!url)
	bool operator !();
	operator void * ();
};

