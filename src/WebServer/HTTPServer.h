/* Copyright (C) 2011 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#if !defined(_HTTPFILESERVER_H_)
#define _HTTPFILESERVER_H_

/*
#include "winsock2.h"
#pragma comment(lib, "ws2_32.lib")
*/
#include <functional>
#include "HTTPLib.h"
#include "HTTPListener.h"
/*
* HTTPServer类
* 目的: 代表一个HTTP Server在内存中的存在.
* 1. 管理侦听套接字.
* 2. 管理HTTP连接包括 HTTPRequest, HTTPResponder 等,当有新连接进入时创建 HTTPRequest 对象并调用它的入口函数 run()
*	 以启动对这个新连接的处理过程.
* 3. 提供参数获取的渠道.
* 
* by 阙荣文 - Que's C++ Studio
* 2011.7.7
*/

typedef std::vector<IResponderFactory*> resfactory_list_t;
class HTTPConnection;
class HTTPServer : public IHTTPServer
{
protected:
	IOStateMachineScheduler* _scheduler; /* 状态机调度器 */
	iosm_list_t _listenerList; /* 侦听器队列 */
	iosm_list_t _stmList; /* 连接队列 */
	
	resfactory_list_t _responderFactoryList;
	str_int_map_t _connectionIps; /* 客户端IP地址表(每IP对应一个记录,用来限制每客户的最大连接数 */
	Lock _lock; /* 同步控制对象 */
	int _tmpFileNameNo; /* 临时文件名序号 */
	char _tmpFileNamePre[5]; /* 临时文件名的前缀(用来**尽可能**避免多个HTTPServer共享同一个临时目录时的命名冲突即使系统本身可以做到) */

	/*
	* 环境配置
	*/
	std::string _docRoot; /*根目录*/
	std::string _tmpRoot; /* 临时文件跟目录 */
	bool _isDirectoryVisible; /*是否允许浏览目录*/
	std::string _dftFileName; /*默认文件名*/
	std::string _ip; /*服务器IP地址*/
	u_short _port; /*服务器侦听端口*/
	size_t _maxConnections; /*最大连接数*/
	size_t _maxConnectionsPerIp; /*每个IP的最大连接数*/
	size_t _maxConnectionSpeed; /*每个连接的速度限制,单位 b/s.*/
	unsigned long _sessionTimeout; /*会话超时*/
	unsigned long _recvTimeout; /*recv, connect, accept 操作的超时*/
	unsigned long _sendTimeout; /*send 操作的超时*/
	unsigned long _keepAliveTimeout; /* keep-alive 超时 */

	int install(IOAdapter* adp, u_int ev, IOStateMachine* sm, sm_handler_t onStepDone, sm_handler_t onDone, sm_handler_t onAbort);
	int uninstall(IOAdapter* adp);

	/*
	* HTTPServer本身控制的状态机处理函数
	*/
	void handleListener(HTTPListener* listener, IOAdapter* adp, IOStateMachine* sm, stm_result_t* res);
	void handleConnection(HTTPConnection* conn, IOAdapter* adp, IOStateMachine* sm, stm_result_t* res);
	
public:
	HTTPServer();
	~HTTPServer();

	/*
	* IHTTPServer
	*/
	int start(IHTTPConfig *conf);
	int stop();
	void catchRequest(IRequest* req, IResponder** res, IResponderFactory** factory);
	bool mapServerFilePath(const std::string& url, std::string& serverPath);
	std::string tmpFileName();
	inline const std::string& docRoot() { return _docRoot; }
	inline bool isDirectoryVisible() { return _isDirectoryVisible; }
	inline const std::string& defaultFileNames() { return _dftFileName; }
	inline const std::string& ip() { return _ip; }
	inline u_short port() { return _port; }
	inline size_t maxConnectionsPerIp() { return _maxConnectionsPerIp; }
	inline size_t maxConnections() { return _maxConnections; }
	inline size_t maxConnectionSpeed() { return _maxConnectionSpeed; }
	inline unsigned long sessionTimeout() { return _sessionTimeout; }
	inline unsigned long recvTimeout() { return _recvTimeout; }
	inline unsigned long sendTimeout() { return _sendTimeout; }
	inline unsigned long keepAliveTimeout() { return _keepAliveTimeout; }
};

#endif
