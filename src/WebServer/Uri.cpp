
#include "stdafx.h"
#include "Uri.h"
#include "ATW.h"
#include <string.h>
#include <algorithm>

#if defined(WIN32) || defined(_WIN32)
#include <WINDOWS.H>
#include <TCHAR.H>
#else
#endif

// 这里的值应该和 typedef enum _protocol_t 一一对应
tchar* protocol_name_table[] = {
	URI_TEXT(""), // idx:0 无效
	URI_TEXT("file"), // idx:1 file
	URI_TEXT("gopher"),
	URI_TEXT("http"),
	URI_TEXT("https"),
	URI_TEXT("ftp"),
	URI_TEXT("mailto"),
	URI_TEXT("mms"),
	URI_TEXT("news"),
	URI_TEXT("nntp"),
	URI_TEXT("telnet"),
	URI_TEXT("wais"),
	URI_TEXT("prospero")
};

Uri::Uri(const tstring& urlStr, protocol_t defaultProtoType) : _defaultProtoType(defaultProtoType), _protType(unknown), _port(0)
{
	_parseUrl(urlStr);
}

Uri::Uri(protocol_t defaultProtoType) : _defaultProtoType(defaultProtoType), _protType(unknown), _port(0)
{
}

Uri::Uri(const Uri& rh) : _protType(rh._protType), _port(rh._port), _urlStr(rh._urlStr),
	_hostname(rh._hostname), _path(rh._path), _parameters(rh._parameters), _content(rh._content),
	_username(rh._username), _password(rh._password), _httpParamList(rh._httpParamList)
{
}

Uri::~Uri(void)
{
}

void Uri::_reset()
{
	_protType = unknown;
	_port = 0;
	
	_urlStr.clear();
	_hostname.clear();
	_path.clear();
	_parameters.clear();
	_content.clear();
	_username.clear();
	_password.clear();
	_httpParamList.clear();
}

Uri::protocol_t Uri::_mapProtocolType(const tstring& protoName)
{
	if(_tcsicmp(protoName.c_str(), protocol_name_table[file]) == 0)
	{
		return file;
	}
	else if(_tcsicmp(protoName.c_str(), protocol_name_table[gopher]) == 0)
	{
		return gopher;
	}
	else if(_tcsicmp(protoName.c_str(), protocol_name_table[http]) == 0)
	{
		return http;
	}
	else if(_tcsicmp(protoName.c_str(), protocol_name_table[https]) == 0)
	{
		return https;
	}
	else if(_tcsicmp(protoName.c_str(), protocol_name_table[ftp]) == 0)
	{
		return ftp;
	}
	else if(_tcsicmp(protoName.c_str(), protocol_name_table[mailto]) == 0)
	{
		return mailto;
	}
	else if(_tcsicmp(protoName.c_str(), protocol_name_table[mms]) == 0)
	{
		return mms;
	}
	else if(_tcsicmp(protoName.c_str(), protocol_name_table[news]) == 0)
	{
		return news;
	}
	else if(_tcsicmp(protoName.c_str(), protocol_name_table[nntp]) == 0)
	{
		return nntp;
	}
	else if(_tcsicmp(protoName.c_str(), protocol_name_table[telnet]) == 0)
	{
		return telnet;
	}
	else if(_tcsicmp(protoName.c_str(), protocol_name_table[wais]) == 0)
	{
		return wais;
	}
	else if(_tcsicmp(protoName.c_str(), protocol_name_table[prospero]) == 0)
	{
		return prospero;
	}
	else
	{
		return unknown;
	}
}

unsigned int Uri::_getDefaultPort(protocol_t protoType)
{
	unsigned int port = 0;
	switch(protoType)
	{
	case file: port = 0; break;
	case gopher: port = 70; break;
	case http: port = 80; break;
	case https: port = 443; break;
	case ftp: port = 21; break;
	case mailto: port = 0; break;
	case mms: port = 1755; break;
	case news: port = 0; break;
	case nntp: port = 119; break;
	case telnet: port = 23; break;
	case wais: port = 0; break;
	case prospero: port = 409; break;
	default: break;
	}

	return port;
}


bool Uri::isValid()
{
	return _protType != unknown;
}

bool Uri::operator !()
{
	return !isValid();
}

Uri::operator void *()
{
	if(isValid()) return this;
	else return NULL;
}

Uri& Uri::operator = (const Uri& rh)
{
	_defaultProtoType = rh._defaultProtoType;
	_protType = rh._protType;
	_port = rh._port;
	_urlStr = rh._urlStr;
	_hostname = rh._hostname;
	_path = rh._path;
	_parameters = rh._parameters;
	_content = rh._content;
	_username = rh._username;
	_password = rh._password;

	_httpParamList = rh._httpParamList;
	return *this;
}

Uri& Uri::operator = (const tstring& urlStr)
{
	_reset();
	_parseUrl(urlStr);
	return *this;
}

bool Uri::operator == (const Uri& rh)
{
	if( this->_protType == rh._protType &&
		this->_username == rh._username && this->_password == rh._password &&
		this->_hostname == rh._hostname && this->_port == rh._port && 
		this->_path == rh._path &&
		this->_httpParamList.size() == rh._httpParamList.size()
		)
	{

		for(param_list_t::const_iterator it = _httpParamList.begin(), itrh = rh._httpParamList.begin(); 
			it != _httpParamList.end(); ++it, ++itrh)
		{
			if( _tcsicmp(it->first.c_str(), itrh->first.c_str()) != 0 || _tcscmp(it->second.c_str(), itrh->second.c_str()) != 0)
			{
				return false;
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}

// url格式 <协议名>://<用户名>:<密码>@<主机>:<端口>/<url路径>?<参数名>=<参数值>&<参数名>=<参数值>...
// 思路:
// 设置变量 [beginPos, endPos) 为每一段要分析内容的范围,依次分析:
// (1) <协议名>
// (2) <用户名>:<密码>@<主机>:<端口>
//		a) <用户名>:<密码>
//			1)<用户名>
//			2)<密码>
//		b) <主机>:<端口>
// (3) <url路径>
// (4) <参数名>=<参数值>&<参数名>=<参数值>...
// 
// 每一步分析结束的时候, [beginPos, endPos) 肯定表示了刚刚分析结束的内容.

bool Uri::_parseUrl(const tstring& urlStr)
{
	// 去除开头和结尾的空格
	tstring tmpUrlStr(urlStr);
	while(tmpUrlStr.size() > 0 && tmpUrlStr.front() == URI_TEXT(' ')) tmpUrlStr.erase(tmpUrlStr.begin());
	while(tmpUrlStr.size() > 0 && tmpUrlStr.back() == URI_TEXT(' ')) tmpUrlStr.erase(tmpUrlStr.end() - 1);
	if(tmpUrlStr.size() <= 0) return false;

	// 保存url字符串,从此时开始 _urlStr 的值不再改变,所以它的 iterator都一直有效.
	_urlStr = tmpUrlStr;
	tstring::const_iterator beginPos = _urlStr.begin(); // 要分析的内容总是在 [beginPos, endPos) 内.
	tstring::const_iterator endPos = _urlStr.end();
	
	// 解析协议名( [字符串开头, 第一个'/'之前的第一个':')
	tstring::const_iterator protonameEndPos = std::find(beginPos, endPos, URI_TEXT(':'));
	if(protonameEndPos == endPos)
	{
		// 未找到 ':' 说明省略了协议名
		_protType = _defaultProtoType;
		endPos = beginPos;
	}
	else
	{
		endPos = protonameEndPos;
		tstring protoName = _urlStr.substr( beginPos - _urlStr.begin(), endPos - beginPos);
		_protType = _mapProtocolType(protoName);

		if( unknown == _protType )
		{
			// 未知的协议名同样说明省略了协议名
			_protType = _defaultProtoType;
			endPos = beginPos;
		}
		else
		{
			// 只要协议名后面有 "://" 则跳过去
			++endPos; // 跳过 ':'
			if(endPos != _urlStr.end() && *endPos == URI_TEXT('/') && endPos + 1 != _urlStr.end() && *(endPos + 1) == URI_TEXT('/') ) endPos += 2; 
			// 跳过紧跟着':'的两个连续'/'
		}
	}

	// move next
	beginPos = endPos;
	endPos = _urlStr.end();

	// 设置内容(除协议名外的部分)
	_content = _getSubStr(_urlStr, beginPos, endPos);

	// 根据各个协议分类处理
	switch(_protType)
	{
	case ftp:
	case gopher:
	case http:
	case https:
	case mms:
	case nntp:
	case prospero:
		{
			// 解析各个字段, 通用格式: //<用户名>:<密码>@<主机>:<端口>/<url路径>
			tstring::const_iterator uphpEndPos = std::find(beginPos, endPos, URI_TEXT('/'));
			tstring::const_iterator upEndPos = std::find(beginPos, uphpEndPos, URI_TEXT('@'));
			if( upEndPos == uphpEndPos)
			{
				// url 不包含 用户名和密码
				_username = URI_TEXT("");
				_password = URI_TEXT("");
			}
			else
			{
				// url 包含了 用户名和密码
				tstring::const_iterator uEndPos = std::find(beginPos, upEndPos, URI_TEXT(':'));
				_username = _getSubStr(_urlStr, beginPos, uEndPos);

				if( uEndPos == upEndPos )
				{
					// 没有密码
					_password = URI_TEXT("");
				}
				else
				{
					// url 中有密码
					_password = _getSubStr(_urlStr, uEndPos + 1, upEndPos); // 跳过:
				}

				// 设置主机名端口号字段的起始位置,跳过 '@'
				beginPos = upEndPos + 1;
			}
			endPos = uphpEndPos;

			// 主机名 端口号
			tstring::const_iterator hostnameEndPos = std::find(beginPos, endPos, URI_TEXT(':'));
			if(hostnameEndPos == endPos)
			{
				// 主机名 和 端口
				_hostname = _getSubStr(_urlStr, beginPos, endPos);
				_port = _getDefaultPort(_protType);
			}
			else
			{
				// 主机名 和 端口
				_hostname = _getSubStr(_urlStr, beginPos, hostnameEndPos);

				hostnameEndPos++; // 跳过 ':'
				tstring portStr = _getSubStr(_urlStr, hostnameEndPos, endPos);
				_port = static_cast<unsigned int>( _tstoi(portStr.c_str()) );
			}

			// move next
			beginPos = endPos;
			endPos = _urlStr.end();

			// 路径
			tstring::const_iterator pathEndPos = std::find(beginPos, endPos, URI_TEXT('?'));
			_path = _getSubStr(_urlStr, beginPos, pathEndPos);
			if(pathEndPos == endPos) endPos = pathEndPos;
			else endPos = pathEndPos + 1; // 后面还有参数,则跳过 '?'

			// move next
			beginPos = endPos;
			endPos = _urlStr.end();

			// 全部参数
			_parameters = _getSubStr(_urlStr, beginPos, endPos);

			//// 协议专用参数
			if( http == _protType || https == _protType)
			{
				// 清空记录
				_httpParamList.clear();

				// 格式//<用户名>:<密码>@<主机>:<端口>/<url路径>?<参数名>=<参数值>&<参数名>=<参数值>...
				tstring::const_iterator paramEndPos = std::find(beginPos, endPos, URI_TEXT('&'));
				tstring::const_iterator paramBeginPos = beginPos;
				while( paramEndPos != paramBeginPos )
				{
					// [paramBeginPos, paramEndPos) 标识了正在扫描的参数名/值对.
					tstring::const_iterator paramNameEndPos = std::find(paramBeginPos, paramEndPos, URI_TEXT('='));
					if( paramNameEndPos == paramEndPos )
					{
						// 没有 '=', 说明只有参数名
						tstring paramKey = _urlStr.substr(paramBeginPos - _urlStr.begin(), paramNameEndPos - paramBeginPos);
						tstring paramValue(URI_TEXT(""));
						_httpParamList.push_back(std::make_pair(paramKey, paramValue));
					}
					else
					{
						tstring paramKey = _urlStr.substr(paramBeginPos - _urlStr.begin(), paramNameEndPos - paramBeginPos);
						++paramNameEndPos; // 跳过 '='
						tstring paramValue = _urlStr.substr(paramNameEndPos - _urlStr.begin(), paramEndPos - paramNameEndPos);
						_httpParamList.push_back(std::make_pair(paramKey, paramValue));
					}

					// next parameter
					if( paramEndPos == endPos )
					{
						// 结束
						break;
					}
					else
					{
						paramBeginPos = paramEndPos + 1; // 跳过 '&'
						paramEndPos = std::find(paramBeginPos, endPos, URI_TEXT('&'));
					}
				}
			}
		}
		break;
	case news:
	case mailto: {} break;
	default: break;
	}

	// 返回
	return true;
}

tstring Uri::_getSubStr(const tstring& str, tstring::const_iterator& first, tstring::const_iterator& second)
{
	return str.substr(first - str.begin(), second - first);
}

const tstring& Uri::content()
{
	return _content;
}

const tstring& Uri::uri()
{
	return _urlStr;
}

const tstring& Uri::hostname()
{
	return _hostname;
}

const tstring& Uri::path()
{
	return _path;
}

const tstring& Uri::parameters()
{
	return _parameters;
}

const tstring& Uri::username()
{
	return _username;
}
const tstring& Uri::password()
{
	return _password;
}

Uri::protocol_t Uri::protocol()
{
	return _protType;
}

unsigned int Uri::port()
{
	return _port;
}

const Uri::param_list_t& Uri::httpParamList()
{
	return _httpParamList;
}

bool Uri::httpQuery(const tstring& paramKey, tstring& paramValue)
{
	for(param_list_t::size_type idx = 0; idx < _httpParamList.size(); ++idx)
	{
		if( 0 == _tcsicmp(paramKey.c_str(), _httpParamList[idx].first.c_str()) )
		{ 
			paramValue = _httpParamList[idx].second;
			return true;
		}
	}

	return false;
}

char _toHex(int n)
{
	if( n < 0 ) return 0;
	if( n >= 0 && n <= 9 ) return static_cast<char>('0' + n);
	if( n >= 10 && n <= 15) return static_cast<char>('a' + n - 10);
	return 0;
}

tstring Uri::encode(const tstring& inputStr)
{
	// 转换为UTF8内码
	std::string utf8Str = WtoUTF8(TtoW(inputStr));

	// 扫描,如果内有大于 127 的字节,则用HEX表示.
	std::string destStr("");
	bool isEncoded = false;
	for(std::string::size_type idx = 0; idx < utf8Str.size(); ++idx)
	{
		unsigned char ch = static_cast<unsigned char>(utf8Str[idx]);
		if( ch > 127 )
		{
			isEncoded = true;
			destStr.push_back('%');
			destStr.push_back( _toHex(ch >> 4));
			destStr.push_back( _toHex( ch & 0x0f));
		}
		else
		{
			destStr.push_back(utf8Str[idx]);
		}
	}

	if(isEncoded) return AtoT(destStr) ;
	else return inputStr;
}

tstring Uri::decode(const tstring& inputStr)
{
	// 转换为ANSI编码
	std::string astr = TtoA(inputStr);
	std::string destStr("");

	// 扫描,得到一个UTF8字符串
	bool isEncoded = false;
	for(std::string::size_type idx = 0; idx < astr.size(); ++idx)
	{
		char ch = astr[idx];
		if(ch == '%')
		{
			isEncoded = true;
			if( idx + 1 < astr.size() && idx + 2 < astr.size() )
			{
				char orgValue[5] = {0}, *stopPos = NULL;
				orgValue[0] = '0';
				orgValue[1] = 'x';
				orgValue[2] = astr[idx + 1];
				orgValue[3] = astr[idx + 2];
				orgValue[4] = 0;
				ch = static_cast<char> (strtol(orgValue, &stopPos, 16));

				idx += 2;
			}
			else
			{
				// 格式错误
				break;
			}
		}

		destStr.push_back(ch);
	}

	if(isEncoded) return WtoT(UTF8toW(destStr));
	else return inputStr;
}

/*
Network Working Group                                     T. Berners-Lee
	Request for Comments: 1738                                          CERN
Category: Standards Track                                    L. Masinter
		  Xerox Corporation
		  M. McCahill
		  University of Minnesota
		  Editors
		  December 1994

		  统一资源定位器（URL）
		  (RFC1738——Uniform Resource Locators (URL)）
		  这份备忘录的情况
		  本备忘录详细说明了一种为因特网团体提供的因特网标准追踪协议（track protocol），
		  恳请大家讨论并提出宝贵意见。如果你想了解这个协议的情况及标准化状态，请参考《因
		  特网正式协议标准（Internet Official Protocol Standards）》（STD 1）的最新版本。
		  本备忘录可以自由发布发布，不受任何限制。
		  摘要
		  该文档详细说明了统一资源定位器、定位的语法和语义以及如何通过因特网来访问资源。


		  目录
		  1．绪论	2
		  2．常规URL语法	2
		  2．1  URL的主要部分	2
		  2．2  URL字符编码问题	3
		  2．3 分层方案和关系链接	4
		  3．特殊方案	4
		  3．1通用因特网方案语法	4
		  3．2 FTP	5
		  3．3 HTTP	7
		  3．4 GOPHER	7
		  3．5 MAILTO	9
		  3．6 NEWS（新闻）	10
		  3．7 NNTP（Network News Transfer Protocol,网络新闻传输协议）	10
		  3．8 TELNET	10
		  3．9 WAIS（Wide Area Information Servers,广域信息服务系统）	11
		  3．10 FILES(文件)	11
		  3．11 PROSPERO	12
		  4． 新方案的注册	13
		  5．特定URL方案的BNF（巴柯斯范式）	13
		  6．安全事项	16
		  7．感谢	16
		  附录：上下文URL的推荐标准	17
		  参考文献：	17
		  编者地址：	19

		  1．绪论

		  因特网上的可用资源可以用简单字符串来表示，该文档就是描述了这种字符串的语法和语
		  义。而这些字符串则被称为：“统一资源定位器”（URL）。

		  这篇说明源于万维网全球信息主动组织（World Wide Web global information 
		  initiative）介绍的概念。RFC1630《通用资源标志符》描述了一些对象数据，他们自1990
		  年起就开始使用这些对象数据。这篇URL说明符合《因特网资源定位符的功能需求
		  （Functional Requirements for Internet Resource Locators）》[12]中说明的需求。

		  这篇文档是由工程任务组织（IETF）的URI工作小组写的。如果你有什么建议和意见的
		  话，你可以给编辑或者URI工作小组< uri@bunyip.com>写信.这个小组的讨论档案存放
在URL:http://www.acl.lanl.gov/URI/archive/uri-archive.index.html。

2．常规URL语法
	正如访问资源的方法有很多种一样，对资源进行定位的方案也有好几种。

	URL的一般语法只是为使用协议来建立新方案提供了一个框架，当然除了已经在这篇文档
	中定义过的。

	URL通过提供资源位置的一种抽象标志符来对资源进行定位。系统定位了一个资源后，可
	能会对它进行各种各样的操作，这些操作可以抽象为下面的几个词：访问，更新，替换，
	发现属性。一般来说，只有访问方法这一项在任何URL方案中都需要进行描述。
	2．1  URL的主要部分
	第五部分给出了URL语法的完整BNF描述。
	URL通常被写成如下形式：
	<方案>:<方案描述部分>
	一个URL包含了它使用的方案名称（<方案>）, 其后紧跟一个冒号，然后是一个字符串
	（<方案描述部分>），这部分的解释由所使用的方案来决定。
	方案名称由一串字符组成。小写字母“a”——“z”，数字，字符加号（“+”），句点（“.”）
	和连字号（“-”）都可以。为了方便起见，程序在解释URL的时候应该视方案名称中的大
	写字母和小写字母一样。（例如：视“HTTP”和“http”一样）。
	2．2  URL字符编码问题
	URL是由一串字符组成，这些字符可以是字母，数字和特殊符号。一个URL可以用多种方
	法来表现,例如：纸上的字迹，或者是用字符集编码的八位字节序列。URL的解释仅取决
	于所用字符的特性。
	在大多数URL方案中，都是使用URL不同部分的字符序列来代表因特网协议中所使用的
	八位字节序列。例如，在ftp方案中主机名，目录名和文件名就是这样的八位字节序列，
	它们用URL的不同部分代表。在这些部分里，一个八位字节数可以用这样的字符来表示：
	该字符在US—ASCII[20]编码字符集中的编码是这个八位字节数。
	另外，八位字节数可以被编成如下形式的代码：“%”后加两个十六进制数字（来自于
	“0123456789ABCDEF”），这两个十六进制数字代表了这八位字节数的值。(字符“abcdef”
	也可以用于十六进制编码)。
	如果存在下面的情况：八位字节数在US-ASCII字符集中没有相应的可显示字符，或者使
	用相应字符会产生不安全因素，或者相应的字符被保留用于特定的URL方案的解释，那
	么它们必须被编成代码。
	没有相应的可显示字符：
	URL只能用US-ASCII字符编码集中的可显示字符表示。US-ASCII中没有用到十六进制的
	八位字节80-FF，并且00－1F和7F代表了控制字符，这些字符必须进行编码。
	不安全：
	字符不安全的原因很多。空格字符就是不安全的，因为URL在被转录或者被排版或者被
	字处理程序处理后其中重要的空格可能被忽略，而可忽略的空格却有可能被解释了。“<”
	和“>”字符也是不安全的，因为它们被用来作为URL在文本中的分隔符；而在有些系统
	中用引号“"”来界定URL。“#”字符也是不安全的，因为它在万维网和其他一些系统中
	被用来从“片段/锚点”标志符中界定URL，所以它通常都要被编码。字符“%”被用来对
	其他字符进行编码，它也是不安全的。其他一些字符，如："{", "}", "|", "\", "^", 
	"~","[", "]",和"`"，由于网关和其他传输代理有时会对这些字符进行修改，所以它们
	也是不安全的。
	必须对URL中所有不安全的字符进行编码。例如，URL中的字符“#”即使是在通常不处
	理片断或者锚点标志符的系统也需要进行编码，这样如果这个URL被拷贝到使用这些标
	志符的系统中，也不必改变URL编码了。
	保留：
	许多URL方案保留了一些字符并赋予特定的含义：它们出现在URL的特定部位并表示特
	定的含义。如果一个字符对应的八位字节在方案中被保留了，那么这个八位字节必须进行
	编码。字符";","/", "?", ":", "@", "=" 和 "&"可能被某个方案所保留，除此之外没
	有其他的保留字符。
	通常情况下一个八位字节被用一个字符表示后或者被编码之后，URL的解释都是一样的。
	但这对于保留字符来说就不适用了：对某一特定方案的保留字符进行编码可能会改变URL
	的语义。
	这样，在URL中只有字母与数字，以及特殊字符“$-_.+!*'(),”和用作保留目的的保留
	字符可以不进行编码。
	另一方面，不必进行编码的字符（包括字母与数字）如果出现在URL的特定部位，只要
	它们不用作保留目的，则可进行编码。
	2．3 分层方案和关系链接
	URL有时候被用来定位那些包含指示器的资源，而这些指示器又指向其他资源。有时候这
	些指示器用关系链接表示，在关系链接中第二资源的位置表示符原则上“和那些除了带有
	次相关路径的表示符相同”。在这篇文档中没有对关系链接进行描述。但是，关系链接的
	使用依赖于包含分层结构的原始URL，它是关系链接的基础。
	有些URL方案（例如ftp，http，和文件方案）包含的名字可以被认为是分层次的；这些
	层次之间用“/”分隔。
	3．特殊方案
	一些已经存在的标准协议和正处于试验中的协议之间的映射关系的轮廓用BNF语法定义
	进行描述。下面对一些协议进行了注释：
	ftp                     File Transfer protocol（文件传输协议）
	http                    Hypertext Transfer Protocol（超文本传输协议）
	gopher                  The Gopher protocol（Gopher协议）
	mailto                  Electronic mail address（电子邮件地址）
	news                    USENET news（USENET新闻）
	nntp                    USENET news using NNTP access
	（使用NNTP访问的USENET新闻）
	telnet                  Reference to interactive sessions
	（交互式会话访问）
	wais                    Wide Area Information Servers（广域信息服务系统）
	file                    Host-specific file names（特殊主机文件名）
	prospero                Prospero Directory Service(prospero目录服务)
	在以后的说明书中可能会对其他一些方案加以描述。这篇文档的第四部分介绍了如何注册
	新的方案，并且列出了一些正在研究中的方案名。
	3．1通用因特网方案语法
	虽然URL其他部分的语法因方案的不同而不同，但那些直接使用基于IP的协议来定位因
	特网上的主机的URL方案都使用了如下形式的通用语法来表示特定的方案数据：
	//<用户名>:<密码>@<主机>:<端口>/<url路径>
	可能会省略“<用户名>:<密码>@”，“ :<密码>”，“ :<端口>”，和“/<url路径>”这些部
	分的某些或者全部。这些方案的特定数据以双斜线“//”开头来表明它遵从通用因特网方
	案语法。各个部分分别遵守如下规则：
	用户名
	任意的用户名称。有些方案（例如：ftp）允许使用用户名称的描述。
	密码
	任意的密码。如果存在的话，它紧跟在用户名后面并用一个冒号隔开。
	用户名（和密码）如果存在的话，其后紧跟一个商用符号“@”。在用户名和密码字段中出
现的任何“:”，“@”或者“/”都要进行编码。
	  注意空的用户名或者密码不同于没有用户名和密码；决不能在没有指定用户名的情况下指
	  定密码。例如：<URL:ftp://@host.com/>的用户名为空并且没有密码，< 
URL:ftp://host.com/>没有用户名，而<URL:ftp://foo:@host.com/>的用户名是“foo”
并且密码为空。
	主机
	网络主机的域名，或者它的以“.”分隔的四组十进制数字集合形式的IP地址。域名的
	形式在RFC1034[13]的3.5节和RFC1123[5]的2.1节中进行了描述，即用“.”分隔的域
	标志串，域标志以字母或者数字开头和结束，也可能包含“-”字符。最右边的域标志不
	能以数字开头，这样就在语法结构上将域名和IP地址区分开来了。
	端口
	指明链接的端口。大部分方案都给协议指定一个默认的端口。也可以随意指定一个十进制
	形式的端口，并用冒号与主机隔开。如果忽略端口，那么这个冒号也要忽略。
	url路径
	定位符的其他部分由方案的特殊数据组成，这些特殊数据被称为“url－路径”。它提供
	了如何对特定资源进行访问的详细信息。注意主机（或端口）与url－路径间的“/”不
	是url－路径的一部分。
	url－路径的语法依赖于所使用的方案。也依赖于它在方案中的解释方法。
	3．2 FTP
	FTP URL方案可以用来指定因特网上使用FTP协议（RFC959）的可达主机上的文件和目录。
	FTP URL遵从3.1节所描述的语法。如果:<端口>被省略的话，则使用缺省端口21。
	3．2．1 FTP 用户名和密码
	在连接上FTP服务器后，可以用“USER”和“PASS”命令来指定用户名和密码。如果没
	有提供用户名或者密码并且FTP服务器只要求一项，那么将使用到“匿名”服务器的转
	换，如下所示：
	用户名“anonymous”被发送。
	访问资源的终端用户的因特网电子邮件地址被作为密码发送。
	如果URL提供用户名但不提供密码，那么远程服务器将要求提供密码，而解释FTP URL
	的程序则要求用户输入密码。
	3．2．2 FTP URL-路径
	FTP URL的URL-路径语法如下：
	<cwd1>/<cwd2>/.../<cwdN>/<name>;type=<typecode>
	这里的<cwd1>到<cwdN>和<name>（可能被编码）都是字符串，<typecode>是字符“a”，
	“i”和“d”之一。“;type=<typecode>”这一部分可以被省略。<cwdx>和<name>部分可
	以为空。整个url－路径，包括它和包含用户名，密码，主机及端口的前缀间的分界符“/”
	都可以被省略。
	url－路径可以被解释成如下的一串FTP命令：
	每个<cwd>元素被作为CWD（改变工作目录）命令的参数发送。
	如果类型编码是“d”，则执行一个以<name>作为参数的NTLS（名字列表）命令，并把结
	果解释为一个文件目录列表。
	否则，执行一个用<typecode>作为参数的TYPE命令，然后访问文件名为<name>的文件（例
	如，使用RETR命令）。
	name或者CWD部分的字符“/”和“;”都是保留字符，必须进行编码。在FTP协议中，
	这些部分在使用前被解码。特别的是，如果访问一个特定文件的适当FTP命令序列需要
	发送一个包含“/”的字符串作为CWD或者RETR命令的参数，那么必须对每个“/”都进
	行编码。
	例如，URL<URL:ftp://myname@host.dom/%2Fetc/motd>被FTP解释为“host.dom”，并以
用户名“myname”登录（如果需要，则提示输入密码），然后执行“CWD /etc”，再接着
	执行“RETR motd”。这和<URL:ftp://myname@host.dom/etc/motd>的含义不一样，它先
执行“CWD etc”然后执行“RETR motd”；开始的“CWD”可能被执行，进入用户“myname”
	的缺省目录。另一方面，<URL:ftp://myname@host.dom//etc/motd>将执行一个不带参数
的“CWD”命令，然后执行“CWD etc”，接着执行“RETR moth”。
	FTP URL也可以用于其他操作；例如，可以更新远程文件服务器上的文件，或者根据它的
	目录列表来推断它的一些信息。完成这些功能的机制在这儿没有仔细介绍。
	3．2．3 FTP 类型编码是可选择的
	FTP URL的整个;type=<typecode>部分都是可选择的。如果这一部分被省略，那么解释
	URL的客户程序必须猜测适当模式来使用。一般来说，文件数据内容的类型只能从文件名
	来猜测，例如根据文件名后缀猜测；用来传输文件的合适的类型编码于是可以从文件的数
	据内容推断出来。
	3．2．4层次
	在有些文件系统中，用来表示URL的层次结构的“/”与用来构建文件系统层次的分隔符
	相同，这样一来，文件名和URL路径看起来就很像。但这并不意味着URL是一个Unix文
	件名。
	3．2．5优化
	客户端通过FTP对资源进行访问时可能会使用一些额外的搜索方法来优化交互过程。例
	如，对一些FTP服务器来说，当访问同一个服务器的多个URL的时候，则保持控制连接
	一直打开是比较合理的。但FTP协议没有通用的层次模式，因此当一个改变目录的命令
	发出后，如果是一个不同的路径，那么一般不可能推断出下一次将要给另一个目录发送什
	么样的序列。唯一可靠的算法是断开然后重新建立控制连接。
	3．3 HTTP
	HTTP URL 方案是用来标志因特网上使用HTTP(HyperText Transfer Protocol，超文本
	传输协议)的可达资源。
	HTTP协议在其他的地方进行了详细说明。本文只介绍了HTTP URL的语法。
	HTTP URL的形式如下：
http://<host>:<port>/<path>?<searchpart>
其中<host>和<port>已经在3.1节说明过了。如果:<port>部分省略，那么就使用缺省的
	端口80。不需要用户名和密码。<path>是一个HTTP选择器，<searchpart>是查询字符串。
	<path>,<searchpart>和它前面的“?”都是可选择的。如果<path>和<searchpart>部分
	都没有，则“/”也可以省略。
	<path>和<searchpart>部分中的“/”，“;”和“？”都是保留字符。“/”字符可以在HTTP
	中用来表示层次结构。
	3．4 GOPHER
	Gopher URL方案用来标志因特网上使用Gopher协议的可达资源。
	基本Gopher协议是在RFC1436中介绍的，它支持项和项（目录）集合。Gopher+ 协议则
	在基本Gopher协议的基础上进行了扩展，并且向上兼容。[2]中对它进行了介绍。Gopher+
	支持联合属性的任意集合和使用Gopher项的替换数据表示。Gopher URL提供了Gopher
	与Gopher+的项和项属性。
	3．4．1 Gopher URL 语法
	Gopher URL的形式如下：
gopher://<host>:<port>/<gopher-path>
这里的<gopher-path>是
	<gophertype><selector>
	<gophertype><selector>%09<search>
	<gophertype><selector>%09<search>%09<gopher+_string>
	之一。
如果:<port>被省略，那么使用缺省端口70。<gophertype>是一个单字符域，它表示URL
   引用的资源的Gopher类型。<gopher-path>部分也可以整个为空。在这种情况下，分隔
   符“/”也是可选择的，并且<gophertype>的缺省值是“1”。
   <selector>是Gopher选择器字符串。在Gopher协议中，Gopher 选择器字符串一个八位
   字节串，它包括除了十六进制的09（US-ASCII HT 或tab），0A(US-ASCII 字符 LF)和
   0D(US-ASCII 字符CR)外的所有八位字节。
   Gopher客户通过向Gopher服务器发送Gopher选择器字符串来指定要获得的项。
   <gopher-path>中没有保留字符。
   需要注意的是：有些Gopher<selector>字符串是以<gophertype>字符的一个拷贝来开头，
   在这种情况下，这个字符将会连续出现两次。Gopher选择器可能是空字符串；Gopher客
   户端就是这样来查询Gopher服务器的高层目录的。
   3．4．2为Gopher搜索引擎指定URL
   如果URL被提交到Gopher搜索引擎进行查询，那么选择器后将紧跟一个已编码的tab
   （%09）和一个搜索字符串。Gopher客户为了向Gopher搜索服务器提交一个搜索必须向
   Gopher服务器发送<selector>字符串（编码后），一个tab字符，和一个搜索字符串。
   3．4．3Gopher+项的URL语法
   Gopher+项的URL有一个已编码的tab字符（%09）和一个Gopher+字符串。注意尽管
   <search>元素可以是空字符串，但在这种情况下必须提供%09<search>字符串。
   <gopher+_string>被用来表示取得Gopher+项所需要的信息。Gopher+项可以拥有交替视
   图，任意的属性系，也可以有与它们相关联的电子表格。
   客户为了获得与Gopher+URL相关联的数据，必须连接到服务器并且发送Gopher选择器，
   这个选择器的后面紧跟一个tab字符和搜索字符串（可以为空）然后是一个tab字符和
   Gopher+命令。
   3．4．4 缺省的Gopher+数据表示
   当一个Gopher服务器向客户返回目录列表时，Gopher+项后面跟着一个“+”（表示
   Gopher+项）或者一个“?”（表示具有与它们相关联的+ASK形式的Gopher+项）。Gopher+
   字符串只有一个字符“+”的Gopher URL采用项的缺省的视图（数据表示），而Gopher+
   字符串只有一个字符“?”的Gopher URL则采用具有相关联的Gopher电子表格的项。
   3．4．5 具有电子表格的Gopher+项
   具有与之相关联的+ASK的Gopher+项（也就是跟着一个“?”的Gopher+项）要求客户端
   取得该项的+ASK属性来获得表格定义，然后让用户填写这个表格并将用户应答和获得项
   的选择器字符串一起返回。Gopher+客户端知道如何完成这些工作，但需要依赖于Gopher+
   项描述中的“？”标签来知道什么时候处理这种情况。Gopher+项中的“?”被用来与Gopher+
   协议中这种符号的用法相兼容。
   3．4．6 Gopher+项属性集
   为了表示项的Gopher+属性，Gopher URL的Gopher+字符串由“!”或者“$”组成。“!”
   涉及Gopher+项的所有属性。“$”则涉及Gopher目录中所有项的所有项属性。
   3．4．7涉及特定的Gopher+属性
   为了表示特殊的属性，URL的gopher+_string是“!<attribute_name>”或者
   “$<attribute_name>”。例如，gopher+_string的值为“!+ABSTRACT” 表示属性包含
   一个项的抽象。
   为了表示几个属性，gopher+_string可以由几个属性名组成，并且用已编码的空格分隔
   开。例如，“!+ABSTRACT%20+SMELL”代表一个项的+ABSTRACT和+SMELL属性。

   3．4．8 Gopher+交替视图的URL语法
   Gopher+允许项有优化的交替数据表示(交替视图)。Gopher+客户端发送适当的视图和语
   言标志（出现在项的+VIEW属性里）来获得Gopher+的交替视图。为了引用一个特定的
   Gopher+交替视图试图，URL的Gopher+字符串的形式必须如下所示：
   +<视图名称>%20<语言名称>
   例如，Gopher+字符串"+application/postscript%20Es_ES"引用了一个Gopher+项的交
   替视图的西班牙语附注。
   3．4．9 Gopher+电子表格的URL语法
   一个引用了填充有特定数据的Gopher+电子表格（一个ASK块）所参考的项的URL的
   Gopher+字符串是通过对客户送给服务器的gopher+字符串进行编码得到的。这个gopher+
   字符串的形式如下所示：
   +%091%0D%0A+-1%0D%0A<ask_item1_value>%0D%0A<ask_item2_value>%0D%0A.%0D%0A
   Gopher客户端为了获得这个项，它发送如下信息给Gopher服务器：
   <a_gopher_selector><tab>+<tab>1<cr><lf>
   +-1<cr><lf>
   <ask_item1_value><cr><lf>
   <ask_item2_value><cr><lf>
   .<cr><lf>
   3．5 MAILTO
   mailto URL方案是用来指明一个个体或者服务的因特网邮件地址的。它只代表因特网邮
   件地址，而不表示任何其它的附加信息。
   Mailto URL的形式如下所示：
mailto:<rfc822-addr-spec>
	   这里的<rfc822-addr-spec>是地址说明（的编码），这在RFC822[6]中进行了详细的说明。
	   在mailto URL中没有保留字。

	   注意百分符号（"%"）在RFC822中用得比较普遍，它必须被编码。

	   不像许多URL，mailto方案不代表可直接访问的数据对象；也没有迹象表面它代表一个
	   对象。它的使用方法不同于MIME中的报文/外部实体类型。
	   3．6 NEWS（新闻）
	   新闻URL方案被用来查阅新闻组或者USENET新闻上的独立文章，这一点在RFC1036中详
	   细说明了。

	   新闻URL的形式是下面两个之一：
news:<newsgroup-name>
news:<message-id>
	 <newsgroup-name>是一个用句点分隔的层次名称，例如：
	 “comp.infosystems.www.misc”。<message-id>与RFC1036中的2.1.5节中的
	 Message-ID一样，只是后者没有用“<”和“>”括起来；它的形式如下
	 <unique>@<full_domain_name>。消息标志符通过代表“在（at）”的“@”字符和新闻组
	 名称相区分。除此之外，在新闻URL组件中没有其它的保留字符。

	 如果<newsgroup-name>是“*”（例如：<URL:news:*>），那么它表示“所有可用的新闻组”。

	 新闻URL是不同寻常的，因为它们自身不包含足够的信息来定位一个单一资源，但是它
	 们的位置是任意的。

	 3．7 NNTP（Network News Transfer Protocol,网络新闻传
	 输协议）

	 网络新闻传输协议URL方案是引用新闻文章的另一个方法，这个方案在用来从NNTP服务
	 器指定新闻文章时是非常有用的（RFC977）。

	 网络新闻传输协议URL的形式如下：
nntp://<host>:<port>/<newsgroup-name>/<article-number>

这里的<host>和<port>在3.1节进行了阐述。如果省略:<port>，那么端口缺省为119。

	<newsgroup-name>是组名，<article-number>是新闻组中文章的数字编号。

注意nntp:URL指定了文章资源的一个唯一的位置，大多数因特网上的NNTP服务器目前
	   进行的配置只允许本地客户端访问，因此nntp URL并不代表全球可访问的资源。这样URL
的news:形式成为标志新闻文章的首选方法。
	  3．8 TELNET
	  远程登录URL方案被用来指明交互式服务，这种服务可以通过Telnet协议来进行访问。

	  telnet URL的形式如下：
telnet://<user>:<password>@<host>:<port>/

向3.1节所讲的那样，最后面的“/”字符可以被省略。如果:<port>被省略，那么端口
缺省为23。:<password>也可以被省略，<user>:<password>部分也可以整个被省略。

	   这个URL并不指定一个数据对象，而是指定一个交互式的服务。远程交互式服务在允许
	   远程登录的方法上差别很大。实际上，提供的<user>和<password>仅供参考：正在访问
	   一个telnet URL的客户端仅仅建议所暗示的用户名和密码的用户。

	   3．9 WAIS（Wide Area Information Servers,广域信息服
	   务系统）
	   WAIS URL 方案用来指示WAIS数据库，搜索或者WAIS数据库中可用的单个文档。WAIS
	   在[7]中进行了描述。RFC1625[17]对WAIS协议进行了阐述。虽然WAIS协议是基于
	   Z39.50-1988的，但 WAIS URL方案并不是特意用来和任意的Z39.50服务一起使用的。

	   WAIS URL有如下几个形式：

wais://<host>:<port>/<database>
wais://<host>:<port>/<database>?<search>
wais://<host>:<port>/<database>/<wtype>/<wpath>

这里的<host>和<port>在3.1节阐述过了。如果省略了:<port>，那么端口缺省为210。
	第一种形式指定了一个可以用来搜索的WAIS服务器。第二种形式表明了一个特定的搜索。
	<database>是被查询的WAIS数据库名。

	第三种形式表明了WAIS数据中的一个要获取的特定文档。在这种形式中<wtype>是对象
	类型的WAIS表示。许多WAIS实现需要一个在取得信息之间就能够认识对象“类型”的
	客户端，这个类型和搜索响应中的内部对象标志符一起返回。<wtype>包含在URL中，这
	是为了让客户端能理解URL的足够信息来取得文档。

	WAIS URL的<wpath>由WAIS文档标志组成，这个文档标志使用了2.2节所叙述的方法进
	行编码。WAIS 文档标志在处理时应该是不透明的；它仅可以被发布它的服务器分解。

	3．10 FILES(文件)
	文件URL方案被用来指定那些特定主机上的可访问的文件。这个方案和其它大多数方案
	不一样，因为它并不表示一个在因特网上普遍可访问的资源。

	文件URL的形式如下：
file://<host>/<path>
这里的<host>是系统域名的全称，在这个系统中<path>是可访问的，它是形如
	<directory>/<directory>/.../<name>的层次目录。

	例如，一个VMS文件：
DISK$USER:[MY.NOTES]NOTE123456.TXT
		  的形式如下：
		  <URL:file://vms.host.edu/disk$user/my/notes/note12345.txt>

有一种特殊情况，就是<host>可以是字符串“localhost”或者空字符串；它被解释为解
	释这个URL的主机。

	文件URL方案是与众不同的，因为它不指定一个因特网协议或者访问这些文件的方法；
	这样它在主机间网络协议上的效用是有限的。

	3．11 PROSPERO

	Prospero URL方案是用来指定那些可以通过Prospero目录服务访问的资源。Prospero
	协议在其它地方介绍了[14]。
	Prospero URL的形式如下：
prospero://<host>:<port>/<hsoname>;<field>=<value>
这里<host>和<port>和3.1节介绍的一样。如果省略了:<port>，那么端口缺省为1525。
	这里不需要用户名和密码。

	<hsoname>是Prospero协议中特定主机的对象名称，它需要被编码。这个名称是不透明
	的，它是由Prospero服务器解释。分号“;”是保留字符，它不能不经过引用就出现在
	<hsoname>中.

	Prospero URL是通过联系特定主机和端口上的Prospero目录服务器来解释的，然后用来
	决定访问资源的合适的方法，这些资源自身可能被表示成不同的URL。外部的Prospero
	链接被表示成采用底层访问方法的URL，而不是表示成Prospero URL。

	注意斜线“/”可以不经过引用就出现在<hsoname>中，应用程序假定它不代表任何意义。
	尽管斜线在服务器上可以用来表示层次结构，但是这些结构并不被承认。注意许多
	<hsoname>是由斜线开头，在这种情况下，主机或者端口之后将紧跟一个双斜线：前面是
	URL语法的斜线，后面是<hsoname>的开始斜线。（举例来说：
	<URL:prospero://host.dom//pros/name>表示<hsoname>为“/pros/name”）。

另外，与Prospero链接相关联的任意的字段和值都可以成为URL中<hsoname>之后的一
	个部分。在这种情况下，每个“字段/值”组合对都用一个“;”（分号）相互以及与URL
	的其它部分分隔开。字段名称和它的值用一个“＝”（等号）分隔开。如果出现这种情况，
	这些域将用来标志URL的目标。例如，OBJECT-VERSION域可以被用来标志对象的特定版
	本。

	4． 新方案的注册
	可以通过定义一个到相应URL语法的映射和使用一个新的前缀来引入一个新的方案。URL
	试验方案可以通过团体间的共同协议来使用。用字符“x-”开头的方案名称是保留给试验
	方案用的。

	国际数字分配权威（IANA，Internet Assigned Numbers Authority）将管理URL方案的
	注册。任何提交的新URL方案都必须包含一个访问该方案中资源的法则的定义还必须包
	含描述这个方案的语法。

	URL方案必须具有可论证的实用性和可操作性。提供这样一个示范的方法就是借助一个为
	使用已有协议的客户端提供新方案中的对象的网关。如果新方案不能够定位一个数据对象
	资源，那么这个新领域中的名称的属性必须要进行清晰的定义。

	新方案应该在适当的地方努力遵从与已有方案相同的语法规则。对于可以用URL访问的
	协议的地方也是同样的。客户端软件被规定配置成使用特定网关定位符来通过新的命名方
	案间接访问。

	下面的方案已经多次被提议，但这个文档没有定义它自己的语法。它建议IANA保留它们
	的方案名以备将来定义：
	afs   Andrew 文件系统全局文件名（Andrew File System global file         names）。
	mid   		电子邮件报文标志（Message identifiers for electronic mail）.
	cid    		MIME主体部分的内容标志符（ Content identifiers for MIME body 
	parts）.
	nfs   	 	网络文件系统（NFS）文件名（Network File System file names）.
	tn3270	 	交互式3270竞争会话(Interactive 3270 emulation sessions).
	mailserver   访问邮件服务器上的有效数据（Access to data available from mail 
	servers）.
	z39.50       访问ANSI Z39.50服务(Access to ANSI Z39.50 services).

	5．特定URL方案的BNF（巴柯斯范式）
	这是统一资源定位器语法的类BNF描述，它使用了RFC822中的约定，除了用“|”表示
	选择，用方括号[]将可选或者重复的元素括起来之外。简单地说就是文字用引号""引起
	来，可选元素放在方括号[]内，元素可以用<n>*来开头表明有n个或者更多个此元素；n
	缺省为0。

	;URL的一般形式如下：
	genericurl     = scheme ":" schemepart

	;特定的预定义方案在这里进行定义；新方案可以在IANA那儿注册
	url            = httpurl | ftpurl | newsurl |
	nntpurl | telneturl | gopherurl |
	waisurl | mailtourl | fileurl |
	prosperourl | otherurl
	;新方案遵从一般语法
	otherurl       = genericurl
	;方案都是小写的；解释程序应该忽略大小写
	scheme         = 1*[ lowalpha | digit | "+" | "-" | "." ]
schemepart     = *xchar | ip-schemepart
	;基于协议的ip的URL方案部分：
	ip-schemepart  = "//" login [ "/" urlpath ]

login          = [ user [ ":" password ] "@" ] hostport
	hostport       = host [ ":" port ]
host           = hostname | hostnumber
	hostname       = *[ domainlabel "." ] toplabel
	domainlabel    = alphadigit | alphadigit *[ alphadigit | "-" ] alphadigit
	toplabel       = alpha | alpha *[ alphadigit | "-" ] alphadigit
	alphadigit     = alpha | digit
	hostnumber     = digits "." digits "." digits "." digits
	port           = digits
	user           = *[ uchar | ";" | "?" | "&" | "=" ]
password       = *[ uchar | ";" | "?" | "&" | "=" ]
urlpath        = *xchar    ;建立在3.1节的协议基础上
	;预定义方案：
	;FTP(文件传输协议，请参考RFC959)
	ftpurl         = "ftp://" login [ "/" fpath [ ";type=" ftptype ]]
fpath          = fsegment *[ "/" fsegment ]
fsegment       = *[ uchar | "?" | ":" | "@" | "&" | "=" ]
ftptype        = "A" | "I" | "D" | "a" | "i" | "d"
	;FILE(文件)
	fileurl        = "file://" [ host | "localhost" ] "/" fpath
	;HTTP(超文本传输协议)
	httpurl        = "http://" hostport [ "/" hpath [ "?" search ]]
hpath          = hsegment *[ "/" hsegment ]
hsegment       = *[ uchar | ";" | ":" | "@" | "&" | "=" ]
search         = *[ uchar | ";" | ":" | "@" | "&" | "=" ]
;GOPHER(请参考RFC1436)
	gopherurl      = "gopher://" hostport [ / [ gtype [ selector
	[ "%09" search [ "%09" gopher+_string ] ] ] ] ]
gtype          = xchar
	selector       = *xchar
	gopher+_string = *xchar
	;MAILTO(请参考RFC822)
	mailtourl      = "mailto:" encoded822addr
	encoded822addr = 1*xchar               ;在RFC822中进一步定义了
	;NEWS（新闻,请参考RFC1036）
	newsurl        = "news:" grouppart
	grouppart      = "*" | group | article
	group          = alpha *[ alpha | digit | "-" | "." | "+" | "_" ]
article        = 1*[ uchar | ";" | "/" | "?" | ":" | "&" | "=" ] "@" host
	;NNTP（网络新闻传输协议,请参考RFC977）
	nntpurl        = "nntp://" hostport "/" group [ "/" digits ]
;TELNET(远程登录协议)
	telneturl      = "telnet://" login [ "/" ]
;WAIS(广域信息服务系统，请参考RFC1625)
	waisurl        = waisdatabase | waisindex | waisdoc
	waisdatabase   = "wais://" hostport "/" database
	waisindex      = "wais://" hostport "/" database "?" search
	waisdoc        = "wais://" hostport "/" database "/" wtype "/" wpath
	database       = *uchar
	wtype          = *uchar
	wpath          = *uchar
	;PROSPERO
	prosperourl    = "prospero://" hostport "/" ppath *[ fieldspec ]
ppath          = psegment *[ "/" psegment ]
psegment       = *[ uchar | "?" | ":" | "@" | "&" | "=" ]
fieldspec      = ";" fieldname "=" fieldvalue
	fieldname      = *[ uchar | "?" | ":" | "@" | "&" ]
fieldvalue     = *[ uchar | "?" | ":" | "@" | "&" ]
;杂七杂八的定义
	lowalpha       = "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" |
	"i" | "j" | "k" | "l" | "m" | "n" | "o" | "p" |
	"q" | "r" | "s" | "t" | "u" | "v" | "w" | "x" |
	"y" | "z"
	hialpha        = "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" |
	"J" | "K" | "L" | "M" | "N" | "O" | "P" | "Q" | "R" |
	"S" | "T" | "U" | "V" | "W" | "X" | "Y" | "Z"
	alpha          = lowalpha | hialpha
	digit          = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" |
	"8" | "9"
	safe           = "$" | "-" | "_" | "." | "+"
	extra          = "!" | "*" | "'" | "(" | ")" | ","
	national       = "{" | "}" | "|" | "\" | "^" | "~" | "[" | "]" | "`"
	punctuation    = "<" | ">" | "#" | "%" | <">


	reserved       = ";" | "/" | "?" | ":" | "@" | "&" | "="
	hex            = digit | "A" | "B" | "C" | "D" | "E" | "F" |
	"a" | "b" | "c" | "d" | "e" | "f"
	escape         = "%" hex hex

	unreserved     = alpha | digit | safe | extra
	uchar          = unreserved | escape
	xchar          = unreserved | reserved | escape
	digits         = 1*digit

	6．安全事项
	URL方案自身并不会造成安全威胁。用户需要小心的是：在一个时刻指向一个给定对象的
	URL并不会保证一直指向这个对象。甚至也不保证因服务器上对象的移动而会在后来指向
	另一个不同的对象。
	一种同URL相关的安全威胁是：构建一个试图执行像取回对象这样无害的等幂操作的URL
	有时可能会导致发生破坏性的远程操作。这个不安全的URL通常是通过指定一个除了那
	些保留给正在讨论的网络协议用的端口数产生的。客户端在无意间同一个服务器打了交
	道，而这个服务器实际上正在运行一个不同的协议，这样就导致URL内容中包含的指令
	被其他的协议解释了，从而产生意外操作。一个例子就是使用gopher URL来生成一个原
	始的消息并通过SMTP服务器来发送。在使用那些指定端口不是缺省端口的URL时应该进
	行警告，尤其是在这个端口数出现在保留空间里面的情况下。
	当URL包含有嵌入式已编码的特定协议中的分隔符（例如，telnet协议的CR和LF字符）
	并且在传输前没有被解码时应该引起注意。这样除了可能被用来模拟一个超出其范围的操
	作或者参数，会干扰这个协议，并再一次引起执行意想不到的而且可能是有害的远程操作。
	使用包含应该作为秘密的密码的URL是非常轻率的。
	7．感谢
	这份文件建立在基本WWW设计（RFC1630）和许多人在网络上对这个观点进行的诸多讨论
	的基础上。这些讨论受到了Clifford Lynch，Brewster Kahle [10] 和 Wengyik Yeong 
	[18]的文章的极大鼓舞。这份文件综合了John Curran, Clifford Neuman, Ed Vielmetti
	和后来的IETF URL BOF与URI工作小组的成果。

	Dan Connolly, Ned Freed, Roy Fielding, Guido van Rossum, Michael Dolan, Bert 
	Bos, John Kunze, Olle Jarnefors, Peter Svanberg最近详细审阅了这份文件并提出
	了宝贵的意见。还有许多人为这份RFC的完善提供了很大帮助。

	附录：上下文URL的推荐标准
	URI（统一资源标志符），包括URL，趋向于通过这样的协议来传输：这些协议为它们的解
	释提供了上下文。
	在有些情况下，有必要区分URL和语法结构中其他可能的数据结构。在这种情况下，建
议在URL之前加上一个有字符“URL:”组成的前缀，这个前缀可以用来把URL和其它种
				   类的URI区分开。

				   此外，其它种类的文字中包含URL的情况也很常见。例如包含在电子邮件中，USENET 新
				   闻消息中或者印在纸上。在这些情况下，可以很方便的用一个单独的语法分隔符号来分隔
				   URL并把它和文字的其它部分相分离。在一些特殊情况下，标点符号标记可能会造成URL
				   的其它部分出错。因为这个原因，建议使用尖括号（“<”和“>”）并使用“URL:”前缀
				   来界定URL。这些界定符号不会出现在URL中，也不应该出现在指定这个界定符的上下文
				   中。
				   在一个“片断/锚点”（fragment/anchor）标志符和一个URL（在一个“#”之后）相关联
				   的情况下，这个标志符也应该放到括号中。
				   在有些情况下，需要加一些额外的空白（空格，回车，制表符等）来打断那些超过一行的
				   长URL。在提取URL时这些空白应该被忽略。
				   在连字符（“-”）后不应该加入空白。因为有些排字机和打印机在打断一行是可能会（错
				   误地）在行末加入一个连字符，解释程序在解释一个在连字符后包含一个行中断的URL
				   时应该忽略行中断左右所有未编码的空白,并且应该注意到连字符可能是也可能不是URL
				   的一个部分。
				   例如：
				   是的，Jim，我发现它在<URL:ftp://info.cern.ch/pub/www/doc;type=d>上，但是你可
能能够从<URL:ftp://ds.internic.net/rfc>那儿获得它。请注意
<URL:http://ds.internic.net/instructions/overview.html#WARNING>上的警告。

参考文献：
	[1] Anklesaria, F., McCahill, M., Lindner, P., Johnson, D.,
	Torrey, D., and B. Alberti, "The Internet Gopher Protocol
	(a distributed document search and retrieval protocol)",
	RFC 1436, University of Minnesota, March 1993.
	<URL:ftp://ds.internic.net/rfc/rfc1436.txt;type=a>

[2] Anklesaria, F., Lindner, P., McCahill, M., Torrey, D.,
	Johnson, D., and B. Alberti, "Gopher+: Upward compatible
	enhancements to the Internet Gopher protocol",
	University of Minnesota, July 1993.
	<URL:ftp://boombox.micro.umn.edu/pub/gopher/gopher_protocol
/Gopher+/Gopher+.txt>

	[3] Berners-Lee, T., "Universal Resource Identifiers in WWW: A
	Unifying Syntax for the Expression of Names and Addresses of
	Objects on the Network as used in the World-Wide Web", RFC
	1630, CERN, June 1994.
	<URL:ftp://ds.internic.net/rfc/rfc1630.txt>

[4] Berners-Lee, T., "Hypertext Transfer Protocol (HTTP)",
	CERN, November 1993.
	<URL:ftp://info.cern.ch/pub/www/doc/http-spec.txt.Z>

[5] Braden, R., Editor, "Requirements for Internet Hosts --
	Application and Support", STD 3, RFC 1123, IETF, October 1989.
	<URL:ftp://ds.internic.net/rfc/rfc1123.txt>

[6] Crocker, D. "Standard for the Format of ARPA Internet Text
	Messages", STD 11, RFC 822, UDEL, April 1982.
	<URL:ftp://ds.internic.net/rfc/rfc822.txt>

[7] Davis, F., Kahle, B., Morris, H., Salem, J., Shen, T., Wang, R.,
	Sui, J., and M. Grinbaum, "WAIS Interface Protocol Prototype
	Functional Specification", (v1.5), Thinking Machines
	Corporation, April 1990.
	<URL:ftp://quake.think.com/pub/wais/doc/protspec.txt>

[8] Horton, M. and R. Adams, "Standard For Interchange of USENET
	Messages", RFC 1036, AT&T Bell Laboratories, Center for Seismic
	Studies, December 1987.
	<URL:ftp://ds.internic.net/rfc/rfc1036.txt>

[9] Huitema, C., "Naming: Strategies and Techniques", Computer
	Networks and ISDN Systems 23 (1991) 107-110.
	[10] Kahle, B., "Document Identifiers, or International Standard
	Book Numbers for the Electronic Age", 1991.
	<URL:ftp://quake.think.com/pub/wais/doc/doc-ids.txt>

[11] Kantor, B. and P. Lapsley, "Network News Transfer Protocol:
	A Proposed Standard for the Stream-Based Transmission of News",
	RFC 977, UC San Diego & UC Berkeley, February 1986.
	<URL:ftp://ds.internic.net/rfc/rfc977.txt>

[12] Kunze, J., "Functional Requirements for Internet Resource
	Locators", Work in Progress, December 1994.
	<URL:ftp://ds.internic.net/internet-drafts
/draft-ietf-uri-irl-fun-req-02.txt>

	[13] Mockapetris, P., "Domain Names - Concepts and Facilities",
	STD 13, RFC 1034, USC/Information Sciences Institute,
	November 1987.
	<URL:ftp://ds.internic.net/rfc/rfc1034.txt>

[14] Neuman, B., and S. Augart, "The Prospero Protocol",
	USC/Information Sciences Institute, June 1993.
	<URL:ftp://prospero.isi.edu/pub/prospero/doc
/prospero-protocol.PS.Z>

	[15] Postel, J. and J. Reynolds, "File Transfer Protocol (FTP)",
	STD 9, RFC 959, USC/Information Sciences Institute,
	October 1985.
	<URL:ftp://ds.internic.net/rfc/rfc959.txt>

[16] Sollins, K. and L. Masinter, "Functional Requirements for
	Uniform Resource Names", RFC 1737, MIT/LCS, Xerox Corporation,
	December 1994.
	<URL:ftp://ds.internic.net/rfc/rfc1737.txt>

[17] St. Pierre, M, Fullton, J., Gamiel, K., Goldman, J., Kahle, B.,
	Kunze, J., Morris, H., and F. Schiettecatte, "WAIS over
	Z39.50-1988", RFC 1625, WAIS, Inc., CNIDR, Thinking Machines
	Corp., UC Berkeley, FS Consulting, June 1994.
	<URL:ftp://ds.internic.net/rfc/rfc1625.txt>

[18] Yeong, W. "Towards Networked Information Retrieval", Technical
	report 91-06-25-01, Performance Systems International, Inc.
	<URL:ftp://uu.psi.com/wp/nir.txt>, June 1991.

[19] Yeong, W., "Representing Public Archives in the Directory",
	Work in Progress, November 1991.

	[20] "Coded Character Set -- 7-bit American Standard Code for
	Information Interchange", ANSI X3.4-1986.

	编者地址：
	Tim Berners-Lee
	World-Wide Web project
	CERN,
	1211 Geneva 23,
	Switzerland

	电话：+41 (22)767 3755
	传真：+41 (22)767 7155
	Email：timbl@info.cern.ch

	Larry Masinter
	Xerox PARC
	3333 Coyote Hill Road
	Palo Alto, CA 94034

电话: (415) 812-4365
传真: (415) 812-4333
EMail: masinter@parc.xerox.com


	   Mark McCahill
	   Computer and Information Services,
	   University of Minnesota
	   Room 152 Shepherd Labs
	   100 Union Street SE
	   Minneapolis, MN 55455

电话: (612) 625 1300
EMail: mpm@boombox.micro.umn.edu



	   RFC1738——Uniform Resource Locators (URL)             统一资源定位器（URL）


	   1
	   RFC文档中文翻译计划
*/

/*





Network Working Group                                     T. Berners-Lee
Request for Comments: 1738                                          CERN
Category: Standards Track                                    L. Masinter
Xerox Corporation
M. McCahill
University of Minnesota
Editors
December 1994


Uniform Resource Locators (URL)

Status of this Memo

This document specifies an Internet standards track protocol for the
Internet community, and requests discussion and suggestions for
improvements.  Please refer to the current edition of the "Internet
Official Protocol Standards" (STD 1) for the standardization state
and status of this protocol.  Distribution of this memo is unlimited.

Abstract

This document specifies a Uniform Resource Locator (URL), the syntax
and semantics of formalized information for location and access of
resources via the Internet.

1. Introduction

This document describes the syntax and semantics for a compact string
representation for a resource available via the Internet.  These
strings are called "Uniform Resource Locators" (URLs).

The specification is derived from concepts introduced by the World-
Wide Web global information initiative, whose use of such objects
dates from 1990 and is described in "Universal Resource Identifiers
in WWW", RFC 1630. The specification of URLs is designed to meet the
requirements laid out in "Functional Requirements for Internet
Resource Locators" [12].

This document was written by the URI working group of the Internet
Engineering Task Force.  Comments may be addressed to the editors, or
to the URI-WG <uri@bunyip.com>. Discussions of the group are archived
at <URL:http://www.acl.lanl.gov/URI/archive/uri-archive.index.html>








Berners-Lee, Masinter & McCahill                                [Page 1]

 RFC 1738            Uniform Resource Locators (URL)        December 1994


 2. General URL Syntax

 Just as there are many different methods of access to resources,
 there are several schemes for describing the location of such
 resources.

 The generic syntax for URLs provides a framework for new schemes to
 be established using protocols other than those defined in this
 document.

 URLs are used to `locate' resources, by providing an abstract
 identification of the resource location.  Having located a resource,
 a system may perform a variety of operations on the resource, as
 might be characterized by such words as `access', `update',
 `replace', `find attributes'. In general, only the `access' method
 needs to be specified for any URL scheme.

 2.1. The main parts of URLs

 A full BNF description of the URL syntax is given in Section 5.

 In general, URLs are written as follows:

 <scheme>:<scheme-specific-part>

 A URL contains the name of the scheme being used (<scheme>) followed
 by a colon and then a string (the <scheme-specific-part>) whose
 interpretation depends on the scheme.

 Scheme names consist of a sequence of characters. The lower case
 letters "a"--"z", digits, and the characters plus ("+"), period
 ("."), and hyphen ("-") are allowed. For resiliency, programs
 interpreting URLs should treat upper case letters as equivalent to
 lower case in scheme names (e.g., allow "HTTP" as well as "http").

 2.2. URL Character Encoding Issues

 URLs are sequences of characters, i.e., letters, digits, and special
 characters. A URLs may be represented in a variety of ways: e.g., ink
 on paper, or a sequence of octets in a coded character set. The
 interpretation of a URL depends only on the identity of the
 characters used.

 In most URL schemes, the sequences of characters in different parts
 of a URL are used to represent sequences of octets used in Internet
 protocols. For example, in the ftp scheme, the host name, directory
 name and file names are such sequences of octets, represented by
 parts of the URL.  Within those parts, an octet may be represented by



 Berners-Lee, Masinter & McCahill                                [Page 2]
 
  RFC 1738            Uniform Resource Locators (URL)        December 1994


  the chararacter which has that octet as its code within the US-ASCII
  [20] coded character set.

  In addition, octets may be encoded by a character triplet consisting
  of the character "%" followed by the two hexadecimal digits (from
  "0123456789ABCDEF") which forming the hexadecimal value of the octet.
  (The characters "abcdef" may also be used in hexadecimal encodings.)

  Octets must be encoded if they have no corresponding graphic
  character within the US-ASCII coded character set, if the use of the
  corresponding character is unsafe, or if the corresponding character
  is reserved for some other interpretation within the particular URL
  scheme.

  No corresponding graphic US-ASCII:

  URLs are written only with the graphic printable characters of the
  US-ASCII coded character set. The octets 80-FF hexadecimal are not
  used in US-ASCII, and the octets 00-1F and 7F hexadecimal represent
  control characters; these must be encoded.

  Unsafe:

  Characters can be unsafe for a number of reasons.  The space
  character is unsafe because significant spaces may disappear and
  insignificant spaces may be introduced when URLs are transcribed or
  typeset or subjected to the treatment of word-processing programs.
  The characters "<" and ">" are unsafe because they are used as the
  delimiters around URLs in free text; the quote mark (""") is used to
  delimit URLs in some systems.  The character "#" is unsafe and should
  always be encoded because it is used in World Wide Web and in other
  systems to delimit a URL from a fragment/anchor identifier that might
  follow it.  The character "%" is unsafe because it is used for
  encodings of other characters.  Other characters are unsafe because
  gateways and other transport agents are known to sometimes modify
  such characters. These characters are "{", "}", "|", "\", "^", "~",
  "[", "]", and "`".

  All unsafe characters must always be encoded within a URL. For
  example, the character "#" must be encoded within URLs even in
  systems that do not normally deal with fragment or anchor
  identifiers, so that if the URL is copied into another system that
  does use them, it will not be necessary to change the URL encoding.








  Berners-Lee, Masinter & McCahill                                [Page 3]
  
   RFC 1738            Uniform Resource Locators (URL)        December 1994


   Reserved:

   Many URL schemes reserve certain characters for a special meaning:
   their appearance in the scheme-specific part of the URL has a
   designated semantics. If the character corresponding to an octet is
   reserved in a scheme, the octet must be encoded.  The characters ";",
   "/", "?", ":", "@", "=" and "&" are the characters which may be
   reserved for special meaning within a scheme. No other characters may
   be reserved within a scheme.

   Usually a URL has the same interpretation when an octet is
   represented by a character and when it encoded. However, this is not
   true for reserved characters: encoding a character reserved for a
   particular scheme may change the semantics of a URL.

   Thus, only alphanumerics, the special characters "$-_.+!*'(),", and
   reserved characters used for their reserved purposes may be used
   unencoded within a URL.

   On the other hand, characters that are not required to be encoded
   (including alphanumerics) may be encoded within the scheme-specific
   part of a URL, as long as they are not being used for a reserved
   purpose.

   2.3 Hierarchical schemes and relative links

   In some cases, URLs are used to locate resources that contain
   pointers to other resources. In some cases, those pointers are
   represented as relative links where the expression of the location of
   the second resource is in terms of "in the same place as this one
   except with the following relative path". Relative links are not
   described in this document. However, the use of relative links
   depends on the original URL containing a hierarchical structure
   against which the relative link is based.

   Some URL schemes (such as the ftp, http, and file schemes) contain
   names that can be considered hierarchical; the components of the
   hierarchy are separated by "/".













   Berners-Lee, Masinter & McCahill                                [Page 4]
   
	RFC 1738            Uniform Resource Locators (URL)        December 1994


	3. Specific Schemes

	The mapping for some existing standard and experimental protocols is
	outlined in the BNF syntax definition.  Notes on particular protocols
	follow. The schemes covered are:

	ftp                     File Transfer protocol
	http                    Hypertext Transfer Protocol
	gopher                  The Gopher protocol
	mailto                  Electronic mail address
	news                    USENET news
	nntp                    USENET news using NNTP access
	telnet                  Reference to interactive sessions
	wais                    Wide Area Information Servers
	file                    Host-specific file names
	prospero                Prospero Directory Service

	Other schemes may be specified by future specifications. Section 4 of
	this document describes how new schemes may be registered, and lists
	some scheme names that are under development.

	3.1. Common Internet Scheme Syntax

	While the syntax for the rest of the URL may vary depending on the
	particular scheme selected, URL schemes that involve the direct use
	of an IP-based protocol to a specified host on the Internet use a
	common syntax for the scheme-specific data:

	//<user>:<password>@<host>:<port>/<url-path>

	Some or all of the parts "<user>:<password>@", ":<password>",
	":<port>", and "/<url-path>" may be excluded.  The scheme specific
	data start with a double slash "//" to indicate that it complies with
	the common Internet scheme syntax. The different components obey the
	following rules:

	user
	An optional user name. Some schemes (e.g., ftp) allow the
	specification of a user name.

	password
	An optional password. If present, it follows the user
	name separated from it by a colon.

	The user name (and password), if present, are followed by a
	commercial at-sign "@". Within the user and password field, any ":",
	"@", or "/" must be encoded.




	Berners-Lee, Masinter & McCahill                                [Page 5]
	
	 RFC 1738            Uniform Resource Locators (URL)        December 1994


	 Note that an empty user name or password is different than no user
	 name or password; there is no way to specify a password without
	 specifying a user name. E.g., <URL:ftp://@host.com/> has an empty
	 user name and no password, <URL:ftp://host.com/> has no user name,
	 while <URL:ftp://foo:@host.com/> has a user name of "foo" and an
	 empty password.

	 host
	 The fully qualified domain name of a network host, or its IP
	 address as a set of four decimal digit groups separated by
	 ".". Fully qualified domain names take the form as described
	 in Section 3.5 of RFC 1034 [13] and Section 2.1 of RFC 1123
	 [5]: a sequence of domain labels separated by ".", each domain
	 label starting and ending with an alphanumerical character and
	 possibly also containing "-" characters. The rightmost domain
	 label will never start with a digit, though, which
	 syntactically distinguishes all domain names from the IP
	 addresses.

	 port
	 The port number to connect to. Most schemes designate
	 protocols that have a default port number. Another port number
	 may optionally be supplied, in decimal, separated from the
	 host by a colon. If the port is omitted, the colon is as well.

	 url-path
	 The rest of the locator consists of data specific to the
	 scheme, and is known as the "url-path". It supplies the
	 details of how the specified resource can be accessed. Note
	 that the "/" between the host (or port) and the url-path is
	 NOT part of the url-path.

	 The url-path syntax depends on the scheme being used, as does the
	 manner in which it is interpreted.

	 3.2. FTP

	 The FTP URL scheme is used to designate files and directories on
	 Internet hosts accessible using the FTP protocol (RFC959).

	 A FTP URL follow the syntax described in Section 3.1.  If :<port> is
	 omitted, the port defaults to 21.









	 Berners-Lee, Masinter & McCahill                                [Page 6]
	 
	  RFC 1738            Uniform Resource Locators (URL)        December 1994


	  3.2.1. FTP Name and Password

	  A user name and password may be supplied; they are used in the ftp
	  "USER" and "PASS" commands after first making the connection to the
	  FTP server.  If no user name or password is supplied and one is
	  requested by the FTP server, the conventions for "anonymous" FTP are
	  to be used, as follows:

	  The user name "anonymous" is supplied.

	  The password is supplied as the Internet e-mail address
	  of the end user accessing the resource.

	  If the URL supplies a user name but no password, and the remote
	  server requests a password, the program interpreting the FTP URL
	  should request one from the user.

	  3.2.2. FTP url-path

	  The url-path of a FTP URL has the following syntax:

	  <cwd1>/<cwd2>/.../<cwdN>/<name>;type=<typecode>

	  Where <cwd1> through <cwdN> and <name> are (possibly encoded) strings
	  and <typecode> is one of the characters "a", "i", or "d".  The part
	  ";type=<typecode>" may be omitted. The <cwdx> and <name> parts may be
	  empty. The whole url-path may be omitted, including the "/"
	  delimiting it from the prefix containing user, password, host, and
	  port.

	  The url-path is interpreted as a series of FTP commands as follows:

	  Each of the <cwd> elements is to be supplied, sequentially, as the
	  argument to a CWD (change working directory) command.

	  If the typecode is "d", perform a NLST (name list) command with
	  <name> as the argument, and interpret the results as a file
	  directory listing.

	  Otherwise, perform a TYPE command with <typecode> as the argument,
	  and then access the file whose name is <name> (for example, using
	  the RETR command.)

	  Within a name or CWD component, the characters "/" and ";" are
	  reserved and must be encoded. The components are decoded prior to
	  their use in the FTP protocol.  In particular, if the appropriate FTP
	  sequence to access a particular file requires supplying a string
	  containing a "/" as an argument to a CWD or RETR command, it is



	  Berners-Lee, Masinter & McCahill                                [Page 7]
	  
	   RFC 1738            Uniform Resource Locators (URL)        December 1994


	   necessary to encode each "/".

	   For example, the URL <URL:ftp://myname@host.dom/%2Fetc/motd> is
	   interpreted by FTP-ing to "host.dom", logging in as "myname"
	   (prompting for a password if it is asked for), and then executing
	   "CWD /etc" and then "RETR motd". This has a different meaning from
	   <URL:ftp://myname@host.dom/etc/motd> which would "CWD etc" and then
	   "RETR motd"; the initial "CWD" might be executed relative to the
	   default directory for "myname". On the other hand,
	   <URL:ftp://myname@host.dom//etc/motd>, would "CWD " with a null
	   argument, then "CWD etc", and then "RETR motd".

	   FTP URLs may also be used for other operations; for example, it is
	   possible to update a file on a remote file server, or infer
	   information about it from the directory listings. The mechanism for
	   doing so is not spelled out here.

	   3.2.3. FTP Typecode is Optional

	   The entire ;type=<typecode> part of a FTP URL is optional. If it is
	   omitted, the client program interpreting the URL must guess the
	   appropriate mode to use. In general, the data content type of a file
	   can only be guessed from the name, e.g., from the suffix of the name;
	   the appropriate type code to be used for transfer of the file can
	   then be deduced from the data content of the file.

	   3.2.4 Hierarchy

	   For some file systems, the "/" used to denote the hierarchical
	   structure of the URL corresponds to the delimiter used to construct a
	   file name hierarchy, and thus, the filename will look similar to the
	   URL path. This does NOT mean that the URL is a Unix filename.

	   3.2.5. Optimization

	   Clients accessing resources via FTP may employ additional heuristics
	   to optimize the interaction. For some FTP servers, for example, it
	   may be reasonable to keep the control connection open while accessing
	   multiple URLs from the same server. However, there is no common
	   hierarchical model to the FTP protocol, so if a directory change
	   command has been given, it is impossible in general to deduce what
	   sequence should be given to navigate to another directory for a
	   second retrieval, if the paths are different.  The only reliable
	   algorithm is to disconnect and reestablish the control connection.







	   Berners-Lee, Masinter & McCahill                                [Page 8]
	   
		RFC 1738            Uniform Resource Locators (URL)        December 1994


		3.3. HTTP

		The HTTP URL scheme is used to designate Internet resources
		accessible using HTTP (HyperText Transfer Protocol).

		The HTTP protocol is specified elsewhere. This specification only
		describes the syntax of HTTP URLs.

		An HTTP URL takes the form:

		http://<host>:<port>/<path>?<searchpart>

		where <host> and <port> are as described in Section 3.1. If :<port>
		is omitted, the port defaults to 80.  No user name or password is
		allowed.  <path> is an HTTP selector, and <searchpart> is a query
		string. The <path> is optional, as is the <searchpart> and its
		preceding "?". If neither <path> nor <searchpart> is present, the "/"
		may also be omitted.

		Within the <path> and <searchpart> components, "/", ";", "?" are
		reserved.  The "/" character may be used within HTTP to designate a
		hierarchical structure.

		3.4. GOPHER

		The Gopher URL scheme is used to designate Internet resources
		accessible using the Gopher protocol.

		The base Gopher protocol is described in RFC 1436 and supports items
		and collections of items (directories). The Gopher+ protocol is a set
		of upward compatible extensions to the base Gopher protocol and is
		described in [2]. Gopher+ supports associating arbitrary sets of
		attributes and alternate data representations with Gopher items.
		Gopher URLs accommodate both Gopher and Gopher+ items and item
		attributes.

		3.4.1. Gopher URL syntax

		A Gopher URL takes the form:

		gopher://<host>:<port>/<gopher-path>

		where <gopher-path> is one of

		<gophertype><selector>
		<gophertype><selector>%09<search>
		<gophertype><selector>%09<search>%09<gopher+_string>




		Berners-Lee, Masinter & McCahill                                [Page 9]
		
		 RFC 1738            Uniform Resource Locators (URL)        December 1994


		 If :<port> is omitted, the port defaults to 70.  <gophertype> is a
		 single-character field to denote the Gopher type of the resource to
		 which the URL refers. The entire <gopher-path> may also be empty, in
		 which case the delimiting "/" is also optional and the <gophertype>
		 defaults to "1".

		 <selector> is the Gopher selector string.  In the Gopher protocol,
		 Gopher selector strings are a sequence of octets which may contain
		 any octets except 09 hexadecimal (US-ASCII HT or tab) 0A hexadecimal
		 (US-ASCII character LF), and 0D (US-ASCII character CR).

		 Gopher clients specify which item to retrieve by sending the Gopher
		 selector string to a Gopher server.

		 Within the <gopher-path>, no characters are reserved.

		 Note that some Gopher <selector> strings begin with a copy of the
		 <gophertype> character, in which case that character will occur twice
		 consecutively. The Gopher selector string may be an empty string;
		 this is how Gopher clients refer to the top-level directory on a
		 Gopher server.

		 3.4.2 Specifying URLs for Gopher Search Engines

		 If the URL refers to a search to be submitted to a Gopher search
		 engine, the selector is followed by an encoded tab (%09) and the
		 search string. To submit a search to a Gopher search engine, the
		 Gopher client sends the <selector> string (after decoding), a tab,
		 and the search string to the Gopher server.

		 3.4.3 URL syntax for Gopher+ items

		 URLs for Gopher+ items have a second encoded tab (%09) and a Gopher+
		 string. Note that in this case, the %09<search> string must be
		 supplied, although the <search> element may be the empty string.

		 The <gopher+_string> is used to represent information required for
		 retrieval of the Gopher+ item. Gopher+ items may have alternate
		 views, arbitrary sets of attributes, and may have electronic forms
		 associated with them.

		 To retrieve the data associated with a Gopher+ URL, a client will
		 connect to the server and send the Gopher selector, followed by a tab
		 and the search string (which may be empty), followed by a tab and the
		 Gopher+ commands.






		 Berners-Lee, Masinter & McCahill                               [Page 10]
		 
		  RFC 1738            Uniform Resource Locators (URL)        December 1994


		  3.4.4 Default Gopher+ data representation

		  When a Gopher server returns a directory listing to a client, the
		  Gopher+ items are tagged with either a "+" (denoting Gopher+ items)
		  or a "?" (denoting Gopher+ items which have a +ASK form associated
		  with them). A Gopher URL with a Gopher+ string consisting of only a
		  "+" refers to the default view (data representation) of the item
		  while a Gopher+ string containing only a "?" refer to an item with a
		  Gopher electronic form associated with it.

		  3.4.5 Gopher+ items with electronic forms

		  Gopher+ items which have a +ASK associated with them (i.e. Gopher+
		  items tagged with a "?") require the client to fetch the item's +ASK
		  attribute to get the form definition, and then ask the user to fill
		  out the form and return the user's responses along with the selector
		  string to retrieve the item.  Gopher+ clients know how to do this but
		  depend on the "?" tag in the Gopher+ item description to know when to
		  handle this case. The "?" is used in the Gopher+ string to be
		  consistent with Gopher+ protocol's use of this symbol.

		  3.4.6 Gopher+ item attribute collections

		  To refer to the Gopher+ attributes of an item, the Gopher URL's
		  Gopher+ string consists of "!" or "$". "!" refers to the all of a
		  Gopher+ item's attributes. "$" refers to all the item attributes for
		  all items in a Gopher directory.

		  3.4.7 Referring to specific Gopher+ attributes

		  To refer to specific attributes, the URL's gopher+_string is
		  "!<attribute_name>" or "$<attribute_name>". For example, to refer to
		  the attribute containing the abstract of an item, the gopher+_string
		  would be "!+ABSTRACT".

		  To refer to several attributes, the gopher+_string consists of the
		  attribute names separated by coded spaces. For example,
		  "!+ABSTRACT%20+SMELL" refers to the +ABSTRACT and +SMELL attributes
		  of an item.

		  3.4.8 URL syntax for Gopher+ alternate views

		  Gopher+ allows for optional alternate data representations (alternate
		  views) of items. To retrieve a Gopher+ alternate view, a Gopher+
		  client sends the appropriate view and language identifier (found in
		  the item's +VIEW attribute). To refer to a specific Gopher+ alternate
		  view, the URL's Gopher+ string would be in the form:




		  Berners-Lee, Masinter & McCahill                               [Page 11]
		  
		   RFC 1738            Uniform Resource Locators (URL)        December 1994


		   +<view_name>%20<language_name>

		   For example, a Gopher+ string of "+application/postscript%20Es_ES"
		   refers to the Spanish language postscript alternate view of a Gopher+
		   item.

		   3.4.9 URL syntax for Gopher+ electronic forms

		   The gopher+_string for a URL that refers to an item referenced by a
		   Gopher+ electronic form (an ASK block) filled out with specific
		   values is a coded version of what the client sends to the server.
		   The gopher+_string is of the form:

		   +%091%0D%0A+-1%0D%0A<ask_item1_value>%0D%0A<ask_item2_value>%0D%0A.%0D%0A

		   To retrieve this item, the Gopher client sends:

		   <a_gopher_selector><tab>+<tab>1<cr><lf>
		   +-1<cr><lf>
		   <ask_item1_value><cr><lf>
		   <ask_item2_value><cr><lf>
		   .<cr><lf>

		   to the Gopher server.

		   3.5. MAILTO

		   The mailto URL scheme is used to designate the Internet mailing
		   address of an individual or service. No additional information other
		   than an Internet mailing address is present or implied.

		   A mailto URL takes the form:

		   mailto:<rfc822-addr-spec>

		   where <rfc822-addr-spec> is (the encoding of an) addr-spec, as
		   specified in RFC 822 [6]. Within mailto URLs, there are no reserved
		   characters.

		   Note that the percent sign ("%") is commonly used within RFC 822
		   addresses and must be encoded.

		   Unlike many URLs, the mailto scheme does not represent a data object
		   to be accessed directly; there is no sense in which it designates an
		   object. It has a different use than the message/external-body type in
		   MIME.





		   Berners-Lee, Masinter & McCahill                               [Page 12]
		   
			RFC 1738            Uniform Resource Locators (URL)        December 1994


			3.6. NEWS

			The news URL scheme is used to refer to either news groups or
			individual articles of USENET news, as specified in RFC 1036.

			A news URL takes one of two forms:

			news:<newsgroup-name>
			news:<message-id>

			A <newsgroup-name> is a period-delimited hierarchical name, such as
			"comp.infosystems.www.misc". A <message-id> corresponds to the
			Message-ID of section 2.1.5 of RFC 1036, without the enclosing "<"
			and ">"; it takes the form <unique>@<full_domain_name>.  A message
			identifier may be distinguished from a news group name by the
			presence of the commercial at "@" character. No additional characters
			are reserved within the components of a news URL.

			If <newsgroup-name> is "*" (as in <URL:news:*>), it is used to refer
			to "all available news groups".

			The news URLs are unusual in that by themselves, they do not contain
			sufficient information to locate a single resource, but, rather, are
			location-independent.

			3.7. NNTP

			The nntp URL scheme is an alternative method of referencing news
			articles, useful for specifying news articles from NNTP servers (RFC
			977).

			A nntp URL take the form:

			nntp://<host>:<port>/<newsgroup-name>/<article-number>

			where <host> and <port> are as described in Section 3.1. If :<port>
			is omitted, the port defaults to 119.

			The <newsgroup-name> is the name of the group, while the <article-
			number> is the numeric id of the article within that newsgroup.

			Note that while nntp: URLs specify a unique location for the article
			resource, most NNTP servers currently on the Internet today are
			configured only to allow access from local clients, and thus nntp
			URLs do not designate globally accessible resources. Thus, the news:
			form of URL is preferred as a way of identifying news articles.





			Berners-Lee, Masinter & McCahill                               [Page 13]
			
			 RFC 1738            Uniform Resource Locators (URL)        December 1994


			 3.8. TELNET

			 The Telnet URL scheme is used to designate interactive services that
			 may be accessed by the Telnet protocol.

			 A telnet URL takes the form:

			 telnet://<user>:<password>@<host>:<port>/

			 as specified in Section 3.1. The final "/" character may be omitted.
			 If :<port> is omitted, the port defaults to 23.  The :<password> can
			 be omitted, as well as the whole <user>:<password> part.

			 This URL does not designate a data object, but rather an interactive
			 service. Remote interactive services vary widely in the means by
			 which they allow remote logins; in practice, the <user> and
			 <password> supplied are advisory only: clients accessing a telnet URL
			 merely advise the user of the suggested username and password.

			 3.9.  WAIS

			 The WAIS URL scheme is used to designate WAIS databases, searches, or
			 individual documents available from a WAIS database. WAIS is
			 described in [7]. The WAIS protocol is described in RFC 1625 [17];
			 Although the WAIS protocol is based on Z39.50-1988, the WAIS URL
			 scheme is not intended for use with arbitrary Z39.50 services.

			 A WAIS URL takes one of the following forms:

			 wais://<host>:<port>/<database>
			 wais://<host>:<port>/<database>?<search>
			 wais://<host>:<port>/<database>/<wtype>/<wpath>

			 where <host> and <port> are as described in Section 3.1. If :<port>
			 is omitted, the port defaults to 210.  The first form designates a
			 WAIS database that is available for searching. The second form
			 designates a particular search.  <database> is the name of the WAIS
			 database being queried.

			 The third form designates a particular document within a WAIS
			 database to be retrieved. In this form <wtype> is the WAIS
			 designation of the type of the object. Many WAIS implementations
			 require that a client know the "type" of an object prior to
			 retrieval, the type being returned along with the internal object
			 identifier in the search response.  The <wtype> is included in the
			 URL in order to allow the client interpreting the URL adequate
			 information to actually retrieve the document.




			 Berners-Lee, Masinter & McCahill                               [Page 14]
			 
			  RFC 1738            Uniform Resource Locators (URL)        December 1994


			  The <wpath> of a WAIS URL consists of the WAIS document-id, encoded
			  as necessary using the method described in Section 2.2. The WAIS
			  document-id should be treated opaquely; it may only be decomposed by
			  the server that issued it.

			  3.10 FILES

			  The file URL scheme is used to designate files accessible on a
			  particular host computer. This scheme, unlike most other URL schemes,
			  does not designate a resource that is universally accessible over the
			  Internet.

			  A file URL takes the form:

			  file://<host>/<path>

			  where <host> is the fully qualified domain name of the system on
			  which the <path> is accessible, and <path> is a hierarchical
			  directory path of the form <directory>/<directory>/.../<name>.

			  For example, a VMS file

			  DISK$USER:[MY.NOTES]NOTE123456.TXT

			  might become

			  <URL:file://vms.host.edu/disk$user/my/notes/note12345.txt>

			  As a special case, <host> can be the string "localhost" or the empty
			  string; this is interpreted as `the machine from which the URL is
			  being interpreted'.

			  The file URL scheme is unusual in that it does not specify an
			  Internet protocol or access method for such files; as such, its
			  utility in network protocols between hosts is limited.

			  3.11 PROSPERO

			  The Prospero URL scheme is used to designate resources that are
			  accessed via the Prospero Directory Service. The Prospero protocol is
			  described elsewhere [14].

			  A prospero URLs takes the form:

			  prospero://<host>:<port>/<hsoname>;<field>=<value>

			  where <host> and <port> are as described in Section 3.1. If :<port>
			  is omitted, the port defaults to 1525. No username or password is



			  Berners-Lee, Masinter & McCahill                               [Page 15]
			  
			   RFC 1738            Uniform Resource Locators (URL)        December 1994


			   allowed.

			   The <hsoname> is the host-specific object name in the Prospero
			   protocol, suitably encoded.  This name is opaque and interpreted by
			   the Prospero server.  The semicolon ";" is reserved and may not
			   appear without quoting in the <hsoname>.

			   Prospero URLs are interpreted by contacting a Prospero directory
			   server on the specified host and port to determine appropriate access
			   methods for a resource, which might themselves be represented as
			   different URLs. External Prospero links are represented as URLs of
			   the underlying access method and are not represented as Prospero
			   URLs.

			   Note that a slash "/" may appear in the <hsoname> without quoting and
			   no significance may be assumed by the application.  Though slashes
			   may indicate hierarchical structure on the server, such structure is
			   not guaranteed. Note that many <hsoname>s begin with a slash, in
			   which case the host or port will be followed by a double slash: the
			   slash from the URL syntax, followed by the initial slash from the
			   <hsoname>. (E.g., <URL:prospero://host.dom//pros/name> designates a
			   <hsoname> of "/pros/name".)

			   In addition, after the <hsoname>, optional fields and values
			   associated with a Prospero link may be specified as part of the URL.
			   When present, each field/value pair is separated from each other and
			   from the rest of the URL by a ";" (semicolon).  The name of the field
			   and its value are separated by a "=" (equal sign). If present, these
			   fields serve to identify the target of the URL.  For example, the
			   OBJECT-VERSION field can be specified to identify a specific version
			   of an object.

			   4. REGISTRATION OF NEW SCHEMES

			   A new scheme may be introduced by defining a mapping onto a
			   conforming URL syntax, using a new prefix. URLs for experimental
			   schemes may be used by mutual agreement between parties. Scheme names
			   starting with the characters "x-" are reserved for experimental
			   purposes.

			   The Internet Assigned Numbers Authority (IANA) will maintain a
			   registry of URL schemes. Any submission of a new URL scheme must
			   include a definition of an algorithm for accessing of resources
			   within that scheme and the syntax for representing such a scheme.

			   URL schemes must have demonstrable utility and operability.  One way
			   to provide such a demonstration is via a gateway which provides
			   objects in the new scheme for clients using an existing protocol.  If



			   Berners-Lee, Masinter & McCahill                               [Page 16]
			   
				RFC 1738            Uniform Resource Locators (URL)        December 1994


				the new scheme does not locate resources that are data objects, the
				properties of names in the new space must be clearly defined.

				New schemes should try to follow the same syntactic conventions of
				existing schemes, where appropriate.  It is likewise recommended
				that, where a protocol allows for retrieval by URL, that the client
				software have provision for being configured to use specific gateway
				locators for indirect access through new naming schemes.

				The following scheme have been proposed at various times, but this
				document does not define their syntax or use at this time. It is
				suggested that IANA reserve their scheme names for future definition:

				afs              Andrew File System global file names.
				mid              Message identifiers for electronic mail.
				cid              Content identifiers for MIME body parts.
				nfs              Network File System (NFS) file names.
				tn3270           Interactive 3270 emulation sessions.
				mailserver       Access to data available from mail servers.
				z39.50           Access to ANSI Z39.50 services.

				5. BNF for specific URL schemes

				This is a BNF-like description of the Uniform Resource Locator
				syntax, using the conventions of RFC822, except that "|" is used to
				designate alternatives, and brackets [] are used around optional or
				repeated elements. Briefly, literals are quoted with "", optional
				elements are enclosed in [brackets], and elements may be preceded
				with <n>* to designate n or more repetitions of the following
				element; n defaults to 0.

				; The generic form of a URL is:

				genericurl     = scheme ":" schemepart

				; Specific predefined schemes are defined here; new schemes
				; may be registered with IANA

				url            = httpurl | ftpurl | newsurl |
				nntpurl | telneturl | gopherurl |
				waisurl | mailtourl | fileurl |
				prosperourl | otherurl

				; new schemes follow the general syntax
				otherurl       = genericurl

				; the scheme is in lower case; interpreters should use case-ignore
				scheme         = 1*[ lowalpha | digit | "+" | "-" | "." ]



				Berners-Lee, Masinter & McCahill                               [Page 17]
				
				 RFC 1738            Uniform Resource Locators (URL)        December 1994


				 schemepart     = *xchar | ip-schemepart


				 ; URL schemeparts for ip based protocols:

				 ip-schemepart  = "//" login [ "/" urlpath ]

				 login          = [ user [ ":" password ] "@" ] hostport
				 hostport       = host [ ":" port ]
				 host           = hostname | hostnumber
				 hostname       = *[ domainlabel "." ] toplabel
				 domainlabel    = alphadigit | alphadigit *[ alphadigit | "-" ] alphadigit
				 toplabel       = alpha | alpha *[ alphadigit | "-" ] alphadigit
				 alphadigit     = alpha | digit
				 hostnumber     = digits "." digits "." digits "." digits
				 port           = digits
				 user           = *[ uchar | ";" | "?" | "&" | "=" ]
				 password       = *[ uchar | ";" | "?" | "&" | "=" ]
				 urlpath        = *xchar    ; depends on protocol see section 3.1

				 ; The predefined schemes:

				 ; FTP (see also RFC959)

				 ftpurl         = "ftp://" login [ "/" fpath [ ";type=" ftptype ]]
				 fpath          = fsegment *[ "/" fsegment ]
				 fsegment       = *[ uchar | "?" | ":" | "@" | "&" | "=" ]
				 ftptype        = "A" | "I" | "D" | "a" | "i" | "d"

				 ; FILE

				 fileurl        = "file://" [ host | "localhost" ] "/" fpath

				 ; HTTP

				 httpurl        = "http://" hostport [ "/" hpath [ "?" search ]]
				 hpath          = hsegment *[ "/" hsegment ]
				 hsegment       = *[ uchar | ";" | ":" | "@" | "&" | "=" ]
				 search         = *[ uchar | ";" | ":" | "@" | "&" | "=" ]

				 ; GOPHER (see also RFC1436)

				 gopherurl      = "gopher://" hostport [ / [ gtype [ selector
				 [ "%09" search [ "%09" gopher+_string ] ] ] ] ]
				 gtype          = xchar
				 selector       = *xchar
				 gopher+_string = *xchar




				 Berners-Lee, Masinter & McCahill                               [Page 18]
				 
				  RFC 1738            Uniform Resource Locators (URL)        December 1994


				  ; MAILTO (see also RFC822)

				  mailtourl      = "mailto:" encoded822addr
				  encoded822addr = 1*xchar               ; further defined in RFC822

				  ; NEWS (see also RFC1036)

				  newsurl        = "news:" grouppart
				  grouppart      = "*" | group | article
				  group          = alpha *[ alpha | digit | "-" | "." | "+" | "_" ]
				  article        = 1*[ uchar | ";" | "/" | "?" | ":" | "&" | "=" ] "@" host

				  ; NNTP (see also RFC977)

				  nntpurl        = "nntp://" hostport "/" group [ "/" digits ]

				  ; TELNET

				  telneturl      = "telnet://" login [ "/" ]

				  ; WAIS (see also RFC1625)

				  waisurl        = waisdatabase | waisindex | waisdoc
				  waisdatabase   = "wais://" hostport "/" database
				  waisindex      = "wais://" hostport "/" database "?" search
				  waisdoc        = "wais://" hostport "/" database "/" wtype "/" wpath
				  database       = *uchar
				  wtype          = *uchar
				  wpath          = *uchar

				  ; PROSPERO

				  prosperourl    = "prospero://" hostport "/" ppath *[ fieldspec ]
				  ppath          = psegment *[ "/" psegment ]
				  psegment       = *[ uchar | "?" | ":" | "@" | "&" | "=" ]
				  fieldspec      = ";" fieldname "=" fieldvalue
				  fieldname      = *[ uchar | "?" | ":" | "@" | "&" ]
				  fieldvalue     = *[ uchar | "?" | ":" | "@" | "&" ]

				  ; Miscellaneous definitions

				  lowalpha       = "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" |
				  "i" | "j" | "k" | "l" | "m" | "n" | "o" | "p" |
				  "q" | "r" | "s" | "t" | "u" | "v" | "w" | "x" |
				  "y" | "z"
				  hialpha        = "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" |
				  "J" | "K" | "L" | "M" | "N" | "O" | "P" | "Q" | "R" |
				  "S" | "T" | "U" | "V" | "W" | "X" | "Y" | "Z"



				  Berners-Lee, Masinter & McCahill                               [Page 19]
				  
				   RFC 1738            Uniform Resource Locators (URL)        December 1994


				   alpha          = lowalpha | hialpha
				   digit          = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" |
				   "8" | "9"
				   safe           = "$" | "-" | "_" | "." | "+"
				   extra          = "!" | "*" | "'" | "(" | ")" | ","
				   national       = "{" | "}" | "|" | "\" | "^" | "~" | "[" | "]" | "`"
				   punctuation    = "<" | ">" | "#" | "%" | <">


				   reserved       = ";" | "/" | "?" | ":" | "@" | "&" | "="
				   hex            = digit | "A" | "B" | "C" | "D" | "E" | "F" |
				   "a" | "b" | "c" | "d" | "e" | "f"
				   escape         = "%" hex hex

				   unreserved     = alpha | digit | safe | extra
				   uchar          = unreserved | escape
				   xchar          = unreserved | reserved | escape
				   digits         = 1*digit

				   6. Security Considerations

				   The URL scheme does not in itself pose a security threat. Users
				   should beware that there is no general guarantee that a URL which at
				   one time points to a given object continues to do so, and does not
				   even at some later time point to a different object due to the
				   movement of objects on servers.

				   A URL-related security threat is that it is sometimes possible to
				   construct a URL such that an attempt to perform a harmless idempotent
				   operation such as the retrieval of the object will in fact cause a
				   possibly damaging remote operation to occur.  The unsafe URL is
				   typically constructed by specifying a port number other than that
				   reserved for the network protocol in question.  The client
				   unwittingly contacts a server which is in fact running a different
				   protocol.  The content of the URL contains instructions which when
				   interpreted according to this other protocol cause an unexpected
				   operation. An example has been the use of gopher URLs to cause a rude
				   message to be sent via a SMTP server.  Caution should be used when
				   using any URL which specifies a port number other than the default
				   for the protocol, especially when it is a number within the reserved
				   space.

				   Care should be taken when URLs contain embedded encoded delimiters
				   for a given protocol (for example, CR and LF characters for telnet
				   protocols) that these are not unencoded before transmission.  This
				   would violate the protocol but could be used to simulate an extra
				   operation or parameter, again causing an unexpected and possible
				   harmful remote operation to be performed.



				   Berners-Lee, Masinter & McCahill                               [Page 20]
				   
					RFC 1738            Uniform Resource Locators (URL)        December 1994


					The use of URLs containing passwords that should be secret is clearly
					unwise.

					7. Acknowledgements

					This paper builds on the basic WWW design (RFC 1630) and much
					discussion of these issues by many people on the network. The
					discussion was particularly stimulated by articles by Clifford Lynch,
					Brewster Kahle [10] and Wengyik Yeong [18]. Contributions from John
					Curran, Clifford Neuman, Ed Vielmetti and later the IETF URL BOF and
					URI working group were incorporated.

					Most recently, careful readings and comments by Dan Connolly, Ned
					Freed, Roy Fielding, Guido van Rossum, Michael Dolan, Bert Bos, John
					Kunze, Olle Jarnefors, Peter Svanberg and many others have helped
					refine this RFC.



































					Berners-Lee, Masinter & McCahill                               [Page 21]
					
					 RFC 1738            Uniform Resource Locators (URL)        December 1994


					 APPENDIX: Recommendations for URLs in Context

					 URIs, including URLs, are intended to be transmitted through
					 protocols which provide a context for their interpretation.

					 In some cases, it will be necessary to distinguish URLs from other
					 possible data structures in a syntactic structure. In this case, is
					 recommended that URLs be preceeded with a prefix consisting of the
					 characters "URL:". For example, this prefix may be used to
					 distinguish URLs from other kinds of URIs.

					 In addition, there are many occasions when URLs are included in other
					 kinds of text; examples include electronic mail, USENET news
					 messages, or printed on paper. In such cases, it is convenient to
					 have a separate syntactic wrapper that delimits the URL and separates
					 it from the rest of the text, and in particular from punctuation
					 marks that might be mistaken for part of the URL. For this purpose,
					 is recommended that angle brackets ("<" and ">"), along with the
					 prefix "URL:", be used to delimit the boundaries of the URL.  This
					 wrapper does not form part of the URL and should not be used in
					 contexts in which delimiters are already specified.

					 In the case where a fragment/anchor identifier is associated with a
					 URL (following a "#"), the identifier would be placed within the
					 brackets as well.

					 In some cases, extra whitespace (spaces, linebreaks, tabs, etc.) may
					 need to be added to break long URLs across lines.  The whitespace
					 should be ignored when extracting the URL.

					 No whitespace should be introduced after a hyphen ("-") character.
					 Because some typesetters and printers may (erroneously) introduce a
					 hyphen at the end of line when breaking a line, the interpreter of a
					 URL containing a line break immediately after a hyphen should ignore
					 all unencoded whitespace around the line break, and should be aware
					 that the hyphen may or may not actually be part of the URL.

					 Examples:

					 Yes, Jim, I found it under <URL:ftp://info.cern.ch/pub/www/doc;
					 type=d> but you can probably pick it up from <URL:ftp://ds.in
					 ternic.net/rfc>.  Note the warning in <URL:http://ds.internic.
					 net/instructions/overview.html#WARNING>.








					 Berners-Lee, Masinter & McCahill                               [Page 22]
					 
					  RFC 1738            Uniform Resource Locators (URL)        December 1994


					  References

					  [1] Anklesaria, F., McCahill, M., Lindner, P., Johnson, D.,
					  Torrey, D., and B. Alberti, "The Internet Gopher Protocol
					  (a distributed document search and retrieval protocol)",
					  RFC 1436, University of Minnesota, March 1993.
					  <URL:ftp://ds.internic.net/rfc/rfc1436.txt;type=a>

					  [2] Anklesaria, F., Lindner, P., McCahill, M., Torrey, D.,
					  Johnson, D., and B. Alberti, "Gopher+: Upward compatible
					  enhancements to the Internet Gopher protocol",
					  University of Minnesota, July 1993.
					  <URL:ftp://boombox.micro.umn.edu/pub/gopher/gopher_protocol
					  /Gopher+/Gopher+.txt>

					  [3] Berners-Lee, T., "Universal Resource Identifiers in WWW: A
					  Unifying Syntax for the Expression of Names and Addresses of
					  Objects on the Network as used in the World-Wide Web", RFC
					  1630, CERN, June 1994.
					  <URL:ftp://ds.internic.net/rfc/rfc1630.txt>

					  [4] Berners-Lee, T., "Hypertext Transfer Protocol (HTTP)",
					  CERN, November 1993.
					  <URL:ftp://info.cern.ch/pub/www/doc/http-spec.txt.Z>

					  [5] Braden, R., Editor, "Requirements for Internet Hosts --
					  Application and Support", STD 3, RFC 1123, IETF, October 1989.
					  <URL:ftp://ds.internic.net/rfc/rfc1123.txt>

					  [6] Crocker, D. "Standard for the Format of ARPA Internet Text
					  Messages", STD 11, RFC 822, UDEL, April 1982.
					  <URL:ftp://ds.internic.net/rfc/rfc822.txt>

					  [7] Davis, F., Kahle, B., Morris, H., Salem, J., Shen, T., Wang, R.,
					  Sui, J., and M. Grinbaum, "WAIS Interface Protocol Prototype
					  Functional Specification", (v1.5), Thinking Machines
					  Corporation, April 1990.
					  <URL:ftp://quake.think.com/pub/wais/doc/protspec.txt>

					  [8] Horton, M. and R. Adams, "Standard For Interchange of USENET
					  Messages", RFC 1036, AT&T Bell Laboratories, Center for Seismic
					  Studies, December 1987.
					  <URL:ftp://ds.internic.net/rfc/rfc1036.txt>

					  [9] Huitema, C., "Naming: Strategies and Techniques", Computer
					  Networks and ISDN Systems 23 (1991) 107-110.





					  Berners-Lee, Masinter & McCahill                               [Page 23]
					  
					   RFC 1738            Uniform Resource Locators (URL)        December 1994


					   [10] Kahle, B., "Document Identifiers, or International Standard
					   Book Numbers for the Electronic Age", 1991.
					   <URL:ftp://quake.think.com/pub/wais/doc/doc-ids.txt>

					   [11] Kantor, B. and P. Lapsley, "Network News Transfer Protocol:
					   A Proposed Standard for the Stream-Based Transmission of News",
					   RFC 977, UC San Diego & UC Berkeley, February 1986.
					   <URL:ftp://ds.internic.net/rfc/rfc977.txt>

					   [12] Kunze, J., "Functional Requirements for Internet Resource
					   Locators", Work in Progress, December 1994.
					   <URL:ftp://ds.internic.net/internet-drafts
					   /draft-ietf-uri-irl-fun-req-02.txt>

					   [13] Mockapetris, P., "Domain Names - Concepts and Facilities",
					   STD 13, RFC 1034, USC/Information Sciences Institute,
					   November 1987.
					   <URL:ftp://ds.internic.net/rfc/rfc1034.txt>

					   [14] Neuman, B., and S. Augart, "The Prospero Protocol",
					   USC/Information Sciences Institute, June 1993.
					   <URL:ftp://prospero.isi.edu/pub/prospero/doc
					   /prospero-protocol.PS.Z>

					   [15] Postel, J. and J. Reynolds, "File Transfer Protocol (FTP)",
					   STD 9, RFC 959, USC/Information Sciences Institute,
					   October 1985.
					   <URL:ftp://ds.internic.net/rfc/rfc959.txt>

					   [16] Sollins, K. and L. Masinter, "Functional Requirements for
					   Uniform Resource Names", RFC 1737, MIT/LCS, Xerox Corporation,
					   December 1994.
					   <URL:ftp://ds.internic.net/rfc/rfc1737.txt>

					   [17] St. Pierre, M, Fullton, J., Gamiel, K., Goldman, J., Kahle, B.,
					   Kunze, J., Morris, H., and F. Schiettecatte, "WAIS over
					   Z39.50-1988", RFC 1625, WAIS, Inc., CNIDR, Thinking Machines
					   Corp., UC Berkeley, FS Consulting, June 1994.
					   <URL:ftp://ds.internic.net/rfc/rfc1625.txt>

					   [18] Yeong, W. "Towards Networked Information Retrieval", Technical
					   report 91-06-25-01, Performance Systems International, Inc.
					   <URL:ftp://uu.psi.com/wp/nir.txt>, June 1991.

					   [19] Yeong, W., "Representing Public Archives in the Directory",
					   Work in Progress, November 1991.





					   Berners-Lee, Masinter & McCahill                               [Page 24]
					   
						RFC 1738            Uniform Resource Locators (URL)        December 1994


						[20] "Coded Character Set -- 7-bit American Standard Code for
						Information Interchange", ANSI X3.4-1986.

						Editors' Addresses

						Tim Berners-Lee
						World-Wide Web project
						CERN,
						1211 Geneva 23,
						Switzerland

						Phone: +41 (22)767 3755
						Fax: +41 (22)767 7155
						EMail: timbl@info.cern.ch


						Larry Masinter
						Xerox PARC
						3333 Coyote Hill Road
						Palo Alto, CA 94034

						Phone: (415) 812-4365
						Fax: (415) 812-4333
						EMail: masinter@parc.xerox.com


						Mark McCahill
						Computer and Information Services,
						University of Minnesota
						Room 152 Shepherd Labs
						100 Union Street SE
						Minneapolis, MN 55455

						Phone: (612) 625 1300
						EMail: mpm@boombox.micro.umn.edu
















						Berners-Lee, Masinter & McCahill                               [Page 25]
						

*/