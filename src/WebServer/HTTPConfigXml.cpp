#include "StdAfx.h"
#include "HTTPConfigXml.h"
#include "XmlDocument.h"

/*
* 默认的配置文件
*
<?xml version="1.0" encoding="utf-8" ?>
	<settings>
		<general>
			<!-- HTTP Server 通用设置 -->
			<docRoot>d:\webserver\www</docRoot>
			<tmpRoot>d:\webserver\tmp</tmpRoot>
			<defaultFileNames>index.php,index.html,default.html</defaultFileNames>
			<serverIP></serverIP>
			<serverPort>80</serverPort>
			<directoryVisible>1</directoryVisible>
			<maxConnections>5000</maxConnections>
			<maxConnectionsPerIP>0</maxConnectionsPerIP>
			<maxSpeed>0</maxSpeed>
			<sessionTimeout>0</sessionTimeout>
			<recvTimeout>5000</recvTimeout>
			<sendTimeout>5000</sendTimeout>
			<maxRequestHeaderSize>10240</maxRequestHeaderSize>

			<!-- 保留给UI模块自定义 -->
			<autoRun>on</autoRun>
		</general>
		<log>
			<level>LL_ALL</level>
			<fileName>http.log</fileName>
			<screen>off</screen>
		</log>
		<fcgi>
			<server>
				<name>PHP</name>
				<status>enabled</status>
				<path>d:\php\php-cgi.exe</path>
				<port>0</port>
				<exts>php</exts>
				<maxConnections>5</maxConnections>
				<maxWaitListSize>99999</maxWaitListSize>
				<cacheAll>yes</cacheAll>
			</server>
		</fcgi>
	</settings>
*/

HTTPConfigXml::HTTPConfigXml()
	: _curFcgiServerXmlHandle(NULL)
{
}


HTTPConfigXml::~HTTPConfigXml()
{
}

bool HTTPConfigXml::load(const std::string &fileName)
{
	if(_xmlDoc.Load(fileName.c_str(), true))
	{
		return true;
	}
	else
	{
		return _xmlDoc.Build("settings", "1.0", "utf-8") != NULL;
	}
}

bool HTTPConfigXml::save(const std::string &fileName)
{
	return _xmlDoc.Save(fileName.c_str());
}

bool HTTPConfigXml::get(const std::string &path, std::string &v)
{
	XMLHANDLE h = _xmlDoc.GetNode(path.c_str());
	if(h)
	{
		v = _xmlDoc.GetContent(h);
		return true;
	}
	else
	{
		return false;
	}
}

bool HTTPConfigXml::set(const std::string &path, const std::string &v)
{
	XMLHANDLE h = _xmlDoc.GetNode(path.c_str(), true);
	if(h)
	{
		return NULL != _xmlDoc.SetContent(h, v.c_str());
	}
	else
	{
		return false;
	}
}

std::string HTTPConfigXml::docRoot()
{
	std::string v;
	if(get("/settings/general/docRoot", v)) return v;

	/*
	* 默认值: 当前目录下 www 文件夹
	*/
	return "www";

}

bool HTTPConfigXml::setDocRoot(const std::string &str)
{
	return set("/settings/general/docRoot", str);
}

std::string HTTPConfigXml::tmpRoot()
{
	std::string v;
	if(get("/settings/general/tmpRoot", v)) return v;

	/*
	* 默认值: 当前目录下 tmp 文件夹
	*/
	return "tmp";
}

bool HTTPConfigXml::setTmpRoot(const std::string &str)
{
	return set("/settings/general/tmpRoot", str);
}

std::string HTTPConfigXml::defaultFileNames()
{
	std::string v;
	if(get("/settings/general/defaultFileNames", v)) return v;

	/*
	* 默认值: index.html,default.html,index.php,default.php
	*/
	return "index.html,default.html,index.php,default.php";
}

bool HTTPConfigXml::setDefaultFileNames(const std::string &str)
{
	return set("/settings/general/defaultFileNames", str);
}

std::string HTTPConfigXml::ip()
{
	std::string v;
	if(get("/settings/general/serverIP", v)) return v;

	/*
	* 默认值: 空值,表示绑定所有本机IP
	*/
	return "";
}

bool HTTPConfigXml::setIp(const std::string &str)
{
	return set("/settings/general/serverIP", str);
}

u_short HTTPConfigXml::port()
{
	std::string v;
	if(get("/settings/general/serverPort", v))
	{
		return static_cast<u_short>(atoi(v.c_str()));
	}

	/*
	* 默认值: 80
	*/
	return 80;
}

bool HTTPConfigXml::setPort(u_short p)
{
	char portStr[100] = {0};
	sprintf(portStr, "%d", p);
	return set("/settings/general/serverPort", portStr);
}

bool HTTPConfigXml::dirVisible()
{
	std::string v;
	if(get("/settings/general/directoryVisible", v))
	{
		return stricmp(v.c_str(), "yes") == 0;
	}

	/*
	* 默认值: "no"
	*/
	return false;
}

bool HTTPConfigXml::setDirVisible(bool visible)
{
	return set("/settings/general/directoryVisible", visible ? "yes" : "no");
}

bool HTTPConfigXml::autoRun()
{
	std::string v;
	if(get("/settings/general/autoRun", v))
	{
		return stricmp(v.c_str(), "yes") == 0;
	}

	/*
	* 默认值: "no"
	*/
	return false;
}

bool HTTPConfigXml::setAutoRun(bool yes)
{
	return set("/settings/general/autoRun", yes ? "yes" : "no");
}

size_t HTTPConfigXml::maxConnections()
{
	std::string v;
	if(get("/settings/general/maxConnections", v))
	{
		return static_cast<size_t>(atol(v.c_str()));
	}

	/*
	* 默认值: 5000
	*/
	return 5000;
}

bool HTTPConfigXml::setMaxConnections(size_t n)
{
	char str[100] = {0};
	sprintf(str, "%d", n);
	return set("/settings/general/maxConnections", str);
}

size_t HTTPConfigXml::maxConnectionsPerIp()
{
	std::string v;
	if(get("/settings/general/maxConnectionsPerIP", v))
	{
		return static_cast<size_t>(atol(v.c_str()));
	}

	/*
	* 默认值: 0, 表示不限制
	*/
	return 0;
}

bool HTTPConfigXml::setMaxConnectionsPerIp(size_t n)
{
	char str[100] = {0};
	sprintf(str, "%d", n);
	return set("/settings/general/maxConnectionsPerIP", str);
}

size_t HTTPConfigXml::maxConnectionSpeed()
{
	std::string v;
	if(get("/settings/general/maxSpeed", v))
	{
		return static_cast<size_t>(atol(v.c_str()));
	}
	
	/*
	* 默认值: 0, 表示不限制
	*/
	return 0;
}

bool HTTPConfigXml::setMaxConnectionSpeed(size_t n)
{
	char str[100] = {0};
	sprintf(str, "%d", n);
	return set("/settings/general/maxSpeed", str);
}

size_t HTTPConfigXml::sessionTimeout()
{
	std::string v;
	if(get("/settings/general/sessionTimeout", v))
	{
		return static_cast<size_t>(atol(v.c_str()));
	}
	
	/*
	* 默认值: 0, 表示不限制
	*/
	return 0;
}

bool HTTPConfigXml::setSessionTimeout(size_t n)
{
	char str[100] = {0};
	sprintf(str, "%d", n);
	return set("/settings/general/sessionTimeout", str);
}

size_t HTTPConfigXml::sendTimeout()
{
	std::string v;
	if(get("/settings/general/sendTimeout", v))
	{
		return static_cast<size_t>(atol(v.c_str()));
	}

	/*
	* 默认值: 5000ms
	*/
	return 5000;
}

bool HTTPConfigXml::setSendTimeout(size_t n)
{
	char str[100] = {0};
	sprintf(str, "%d", n);
	return set("/settings/general/sendTimeout", str);
}

size_t HTTPConfigXml::keepAliveTimeout()
{
	std::string v;
	if(get("/settings/general/keepAliveTimeout", v))
	{
		return static_cast<size_t>(atol(v.c_str()));
	}

	/*
	* 默认值: 15s
	*/
	return 15000;
}

bool HTTPConfigXml::setKeepAliveTimeout(size_t n)
{
	char str[100] = {0};
	sprintf(str, "%d", n);
	return set("/settings/general/keepAliveTimeout", str);
}

size_t HTTPConfigXml::recvTimeout()
{
	std::string v;
	if(get("/settings/general/recvTimeout", v))
	{
		return static_cast<size_t>(atol(v.c_str()));
	}

	/*
	* 默认值: 5000ms
	*/
	return 5000;
}

bool HTTPConfigXml::setRecvTimeout(size_t n)
{
	char str[100] = {0};
	sprintf(str, "%d", n);
	return set("/settings/general/recvTimeout", str);
}

bool fill_fcgi_server(XMLDocument &xmlDoc, XMLHANDLE hServer, fcgi_server_ctx_t *serverInf)
{
	XMLHANDLE h = xmlDoc.GetChildByName(hServer, "name");
	strncpy(serverInf->name, xmlDoc.GetContent(h).c_str(), MAX_PATH);

	h = xmlDoc.GetChildByName(hServer, "status");
	serverInf->status = stricmp(xmlDoc.GetContent(h).c_str(), "enabled") == 0;

	h = xmlDoc.GetChildByName(hServer, "path");
	strncpy(serverInf->path, xmlDoc.GetContent(h).c_str(), MAX_PATH);

	h = xmlDoc.GetChildByName(hServer, "port");
	serverInf->port = static_cast<u_short>(atoi(xmlDoc.GetContent(h).c_str()));

	h = xmlDoc.GetChildByName(hServer, "exts");
	strncpy(serverInf->exts, xmlDoc.GetContent(h).c_str(), MAX_PATH);

	h = xmlDoc.GetChildByName(hServer, "maxConnections");
	serverInf->maxConnections = static_cast<size_t>(atoi(xmlDoc.GetContent(h).c_str()));

	h = xmlDoc.GetChildByName(hServer, "maxWaitListSize");
	serverInf->maxWaitListSize = static_cast<size_t>(atoi(xmlDoc.GetContent(h).c_str()));

	h = xmlDoc.GetChildByName(hServer, "cacheAll");
	serverInf->cacheAll = stricmp(xmlDoc.GetContent(h).c_str(), "yes") == 0;

	return true;
}

bool HTTPConfigXml::getFirstFcgiServer(fcgi_server_ctx_t *serverInf)
{
	XMLHANDLE h = _xmlDoc.GetNode("/settings/fcgi/server");
	if(!h)
	{
		return false;
	}
	else
	{
		_curFcgiServerXmlHandle = h;

		fill_fcgi_server(_xmlDoc, _curFcgiServerXmlHandle, serverInf);

		return true;
	}
}

bool HTTPConfigXml::getNextFcgiServer(fcgi_server_ctx_t *serverInf)
{
	// 在 xml 文件中找到下一个 server 描述
	while(_curFcgiServerXmlHandle)
	{
		_curFcgiServerXmlHandle = _xmlDoc.NextSibling(_curFcgiServerXmlHandle);
		if(_xmlDoc.GetName(_curFcgiServerXmlHandle) == "server") break;
	}

	if(!_curFcgiServerXmlHandle)
	{
		return false;
	}
	else
	{
		fill_fcgi_server(_xmlDoc, _curFcgiServerXmlHandle, serverInf);

		return true;
	}
}

bool HTTPConfigXml::addFcgiServer(const fcgi_server_ctx_t *serverInf)
{
	XMLHANDLE h = _xmlDoc.GetNode("/settings/fcgi", true);
	if(!h) return false;

	h = _xmlDoc.AppendNode(h, "server");
	
	char tmpStr[100] = {0};
	XMLHANDLE tmph = _xmlDoc.AppendNode(h, "name");
	_xmlDoc.SetContent(tmph, serverInf->name);

	tmph = _xmlDoc.AppendNode(h, "status");
	_xmlDoc.SetContent(tmph, serverInf->status ? "enabled" : "disabled");

	tmph = _xmlDoc.AppendNode(h, "path");
	_xmlDoc.SetContent(tmph, serverInf->path);

	tmph = _xmlDoc.AppendNode(h, "port");
	sprintf(tmpStr, "%d", serverInf->port);
	_xmlDoc.SetContent(tmph, tmpStr);

	tmph = _xmlDoc.AppendNode(h, "exts");
	_xmlDoc.SetContent(tmph, serverInf->exts);

	tmph = _xmlDoc.AppendNode(h, "maxConnections");
	sprintf(tmpStr, "%d", serverInf->maxConnections);
	_xmlDoc.SetContent(tmph, tmpStr);

	tmph = _xmlDoc.AppendNode(h, "maxWaitListSize");
	sprintf(tmpStr, "%d", serverInf->maxConnections);
	_xmlDoc.SetContent(tmph, tmpStr);

	tmph = _xmlDoc.AppendNode(h, "cacheAll");
	_xmlDoc.SetContent(tmph, serverInf->cacheAll ? "yes" : "no");


	return true;
}

bool HTTPConfigXml::removeFcgiServer(const std::string &name)
{
	XMLHANDLE hFcgi = _xmlDoc.GetNode("/settings/fcgi", true);
	XMLHANDLE hChild = _xmlDoc.FirstChild(hFcgi);
	while(hChild)
	{
		if(_xmlDoc.GetName(hChild) == "server")
		{
			XMLHANDLE hFcgiName = _xmlDoc.GetChildByName(hChild, "name");
			if(hFcgiName && stricmp(_xmlDoc.GetContent(hFcgiName).c_str(), name.c_str()) == 0)
			{
				_xmlDoc.DeleteNode(hChild);
				return true;
			}
		}
		hChild = _xmlDoc.NextSibling(hChild);
	}
	return false;
}

bool HTTPConfigXml::updateFcgiServer(const std::string &name, const fcgi_server_ctx_t *serverInf)
{
	/* 等XMLDocument 类实现 XPath 之后就不用那么麻烦了 */
	XMLHANDLE hFcgi = _xmlDoc.GetNode("/settings/fcgi", true);
	XMLHANDLE hChild = _xmlDoc.FirstChild(hFcgi);
	XMLHANDLE hServer = NULL;
	while(hChild)
	{
		if(_xmlDoc.GetName(hChild) == "server")
		{
			XMLHANDLE hFcgiName = _xmlDoc.GetChildByName(hChild, "name");
			if(hFcgiName && stricmp(_xmlDoc.GetContent(hFcgiName).c_str(), name.c_str()) == 0)
			{
				hServer = hChild;
				break;
			}
		}
		hChild = _xmlDoc.NextSibling(hChild);
	}

	if(!hServer)
	{
		return addFcgiServer(serverInf);
	}
	else
	{
		char tmp[100];
		XMLHANDLE hTmp = NULL;

		if(NULL == (hTmp = _xmlDoc.GetChildByName(hServer, "name"))) hTmp = _xmlDoc.AppendNode(hServer, "name");
		_xmlDoc.SetContent(_xmlDoc.GetChildByName(hServer, "name"), serverInf->name);

		if(NULL == (hTmp = _xmlDoc.GetChildByName(hServer, "status"))) hTmp = _xmlDoc.AppendNode(hServer, "status");
		_xmlDoc.SetContent(hTmp, serverInf->status ? "enabled" : "disabled");

		if(NULL == (hTmp = _xmlDoc.GetChildByName(hServer, "path"))) hTmp = _xmlDoc.AppendNode(hServer, "path");
		_xmlDoc.SetContent(_xmlDoc.GetChildByName(hServer, "path"), serverInf->path);

		if(NULL == (hTmp = _xmlDoc.GetChildByName(hServer, "exts"))) hTmp = _xmlDoc.AppendNode(hServer, "exts");
		_xmlDoc.SetContent(_xmlDoc.GetChildByName(hServer, "exts"), serverInf->exts);

		if(NULL == (hTmp = _xmlDoc.GetChildByName(hServer, "port"))) hTmp = _xmlDoc.AppendNode(hServer, "port");
		sprintf(tmp, "%d", serverInf->port);
		_xmlDoc.SetContent(_xmlDoc.GetChildByName(hServer, "port"), tmp);

		if(NULL == (hTmp = _xmlDoc.GetChildByName(hServer, "maxConnections"))) hTmp = _xmlDoc.AppendNode(hServer, "maxConnections");
		sprintf(tmp, "%d", serverInf->maxConnections);
		_xmlDoc.SetContent(_xmlDoc.GetChildByName(hServer, "maxConnections"), tmp);

		if(NULL == (hTmp = _xmlDoc.GetChildByName(hServer, "maxWaitListSize"))) hTmp = _xmlDoc.AppendNode(hServer, "maxWaitListSize");
		sprintf(tmp, "%d", serverInf->maxWaitListSize);
		_xmlDoc.SetContent(_xmlDoc.GetChildByName(hServer, "maxWaitListSize"), tmp);

		if(NULL == (hTmp = _xmlDoc.GetChildByName(hServer, "cacheAll"))) hTmp = _xmlDoc.AppendNode(hServer, "cacheAll");
		_xmlDoc.SetContent(hTmp, serverInf->cacheAll ? "yes" : "no");
	}
	return false;
}

bool HTTPConfigXml::screenLog()
{
	std::string v;
	if(get("/settings/log/screenLog", v))
	{
		return stricmp(v.c_str(), "on") == 0;
	}

	/*
	* 默认开启屏幕日志
	*/
	return true;
}

bool HTTPConfigXml::enableScreenLog(bool enabled)
{
	return set("/settings/log/screenLog", enabled ? "on" : "off");
}

std::string HTTPConfigXml::logFileName()
{
	std::string v;
	if(get("/settings/log/fileName", v))
	{
		return v;
	}

	/*
	* 默认开启屏幕日志
	*/
	return "log\\http.log";
}

bool HTTPConfigXml::setLogFileName(const std::string &fileName)
{
	return set("/settings/log/fileName", fileName);
}

slogger::LogLevel HTTPConfigXml::logLevel()
{
	std::string v;
	if(get("/settings/log/level", v))
	{
		if(stricmp(v.c_str(), "LL_ALL") == 0)
		{
			return LL_ALL;
		}
		else if(stricmp(v.c_str(), "LL_TRACE") == 0)
		{
			return LL_TRACE;
		}
		else if(stricmp(v.c_str(), "LL_DEBUG") == 0)
		{
			return LL_DEBUG;
		}
		else if(stricmp(v.c_str(), "LL_INFO") == 0)
		{
			return LL_INFO;
		}
		else if(stricmp(v.c_str(), "LL_WARNING") == 0)
		{
			return LL_WARNING;
		}
		else if(stricmp(v.c_str(), "LL_ERROR") == 0)
		{
			return LL_ERROR;
		}
		else if(stricmp(v.c_str(), "LL_FATAL") == 0)
		{
			return LL_FATAL;
		}
		else
		{
			return LL_NONE;
		}
	}

	/*
	* 默认输出日志级别
	*/
	return LL_INFO;
}

bool HTTPConfigXml::setLogLevel(slogger::LogLevel ll)
{
	std::string level;
	if(LL_ALL == ll)
	{
		level = "LL_ALL";
	}
	else if(LL_TRACE == ll)
	{
		level = "LL_TRACE";
	}
	else if(LL_DEBUG == ll)
	{
		level = "LL_DEBUG";
	}
	else if(LL_INFO == ll)
	{
		level = "LL_INFO";
	}
	else if(LL_WARNING == ll)
	{
		level = "LL_WARNING";
	}
	else if(LL_ERROR == ll)
	{
		level = "LL_ERROR";
	}
	else if(LL_FATAL == ll)
	{
		level = "LL_FATAL";
	}
	else
	{
		level = "LL_NONE";
	}
	return set("/settings/log/level", level);
}