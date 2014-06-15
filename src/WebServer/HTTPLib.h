/* Copyright (C) 2011 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#pragma once
#include "fastcgi.h"
#include "ATW.h"
#include "TimerQueue.h"
#include "Pipes/Pipe.h"
#include "IOStateMachine.h"

/*
* 全局日志
*/
#include "logger.h"
LOGGER_USING(theLogger);

/*
* 编译参数
*/
#define MAX_WINIO 16384 // 系统允许的最大同时使用 OSFile() 打开的文件数
#define G_BYTES (1024 * 1024 * 1024) // 1GB
#define M_BYTES (1024 * 1024)		 // 1MB
#define K_BYTES 1024				 // 1KB
#define HOUR (60 * 60)
#define MINUTE 60
#define SECOND 1
#define MAX_DIRECTORY_LIST_SIZE (128 * K_BYTES) // 目录列表
#define MAX_SOCKBUFF_SIZE 4096 // 接收缓冲区. 
#define FCGI_BUFFER_SIZE 4096 // 不能小于 24, 否则 FCGI 缓冲区不够. 一般应该保证在512以上.
#define MAX_METHODSIZE 100 // 用于保存HTTP方法字符串的长度,不宜超过 200
#define MAX_REQUESTHEADERSIZE (K_BYTES * 4) // 最大请求头长度限制
#define MAX_RESPONSEHEADERSIZE (K_BYTES * 4) // 最大请求头长度限制
#define MAX_IP_LENGTH 64 // IP地址的最大长度
#define MIN_SIZE_ONSPEEDLIMITED 512 // 达到速度限制时,发送的最小包字节数.
#define MAX_WAITTIME_ONSPEEDLIMITED 2000 // 达到速度限制时,最多等待多少毫秒发下一个包.这个值如果设置得过长,有可能导致客户端任务服务器没响应
#define ETAG_BYTE_SIZE 5 // 对于内存数据,创建ETag时抽取的字节数.
#define FCGI_CONNECT_TIMEO 5000 // 连接FCGI命名管道的超时时间
#define FCGI_MAX_IDLE_SECONDS 5
#define SERVER_SOFTWARE "Q++ HTTP Server/0.20"
#define MAX_POST_DATA (G_BYTES) // 最多接收1G
#define POST_DATA_CACHE_SIZE (8 * K_BYTES) // 超过8KB的POST DATA将被写入文件
#define FCGI_CACHE_SIZE (16 * K_BYTES)
#define FCGI_PIPE_BASENAME "\\\\.\\pipe\\fast_cgi_ques" // FCGI 命名管道名

/*
* 常量和类型定义
*/
// HTTP 方法
enum HTTP_METHOD
{
	METHOD_UNDEFINE = 0,
	METHOD_GET = 1,
	METHOD_POST,
	METHOD_PUT,
	METHOD_HEAD, // 只返回响应头
	METHOD_DELETE, // 删除
	METHOD_TRACE,
	METHOD_CONNECT,

	METHOD_UNKNOWN = 100
};

// 服务器响应码
#define SC_UNKNOWN 0
#define	SC_OK 200
#define	SC_NOCONTENT 204
#define	SC_PARTIAL 206
#define SC_OBJMOVED 302
#define	SC_BADREQUEST 400
#define	SC_FORBIDDEN 403
#define	SC_NOTFOUND 404
#define	SC_BADMETHOD 405
#define	SC_SERVERERROR 500
#define SC_SERVERBUSY 503

// 返回值定义(错误类型定义)
#define SE_RUNING 1 // 正在运行
#define SE_STOPPED 2 // 已经停止
#define SE_CREATESOCK_FAILED 100 // 套接字创建失败
#define SE_CREATETIMER_FAILED 103 // 无法创建定时器
#define SE_CREATE_IOCP_FAILED 104
#define SE_INVALID_PARAM 105

#define SE_SUCCESS 0
#define SE_NETWORKFAILD 1  // 本地网络模块发生错误
#define SE_REMOTEFAILD 3// 套接字的远程端已经关闭
#define SE_ACCEPTFAILD 4 // accept 本地调用失败
#define SE_RECVFAILD 5 // recv 接收失败
#define SE_SENDFAILD 6 // send 发送失败
#define SE_BINDFAILED 7
#define SE_LISTENFAILED 8 
#define SE_UNKNOWN 1000

// 把 SE_XXX 错误码映射为一个描述字符串
std::string get_error_message(int err);

// HTTP 连接对象退出码
typedef enum HTTP_CLOSE_TYPE
{
	CT_SUCESS = 0,

	CT_CLIENTCLOSED = 10, // 客户端关闭了连接
	CT_SENDCOMPLETE, // 发送完成
	CT_SEND_TIMEO,
	CT_RECV_TIMEO,
	CT_SESSION_TIMEO,
	CT_BADREQUEST,
	
	CT_FCGI_SERVERERROR = 20,
	CT_FCGI_CONNECT_FAILED,
	CT_FCGI_SEND_FAILED,
	CT_FCGI_RECV_FAILED,
	CT_FCGI_RECV_TIMEO,
	CT_FCGI_SEND_TIMEO,
	CT_FCGI_ABORT,

	CT_NETWORK_FAILED = 50,
	CT_INTERNAL_ERROR,

	CT_UNKNOWN = 999 // 未知.	
}HTTP_CONNECTION_EXITCODE;


typedef std::map<std::string, unsigned int> str_int_map_t;
typedef std::vector<std::string> str_vec_t;
typedef void* conn_id_t;
const conn_id_t INVALID_CONNID = NULL;

// 外部定义的字符串
extern const char* g_HTTP_Content_NotFound;
extern const char* g_HTTP_Bad_Request;
extern const char* g_HTTP_Bad_Method;
extern const char* g_HTTP_Server_Error;
extern const char* g_HTTP_Forbidden;
extern const char* g_HTTP_Server_Busy;

// 把一个时间格式化为 HTTP 要求的时间格式(GMT).
std::string format_http_date(__int64* ltime);
std::string to_hex(const unsigned char* pData, int nSize);
std::string decode_url(const std::string& inputStr);
bool map_method(HTTP_METHOD md, char *str);
int http_header_end(const char *data, size_t len);
std::string get_field(const char* buf, const char* key);
void get_file_ext(const std::string &fileName, std::string &ext);
bool match_file_ext(const std::string &ext, const std::string &extList);
std::string get_last_error(DWORD errCode = 0);
size_t split_strings(const std::string &str, str_vec_t &vec, const std::string &sp);
bool get_ip_address(std::string& str);
std::string format_time(size_t ms);
std::string format_size(__int64 bytes);
std::string format_speed(__int64 bytes, unsigned int timeUsed);


// Fast CGI 服务器定义
typedef struct FCGI_SERVER_CONTEXT
{
	char name[MAX_PATH + 1];
	bool status;
	char path[MAX_PATH + 1]; // ip地址(远程模式)或者命令行(本地模式)
	u_short port; // 端口. 0表示是本地模式
	char exts[MAX_PATH + 1]; // 文件扩展名,逗号分隔
	size_t maxConnections; // 最大连接数
	size_t maxWaitListSize; // 等待队列大小
	int cacheAll;	// 是否缓存数据
}fcgi_server_ctx_t;

/*
* HTTP 配置接口
*/
class IHTTPConfig
{
public:
	IHTTPConfig() {};
	virtual ~IHTTPConfig() {};

	virtual std::string docRoot() = 0;
	virtual std::string tmpRoot() = 0;
	virtual std::string defaultFileNames() = 0;
	virtual std::string ip() = 0;
	virtual u_short port() = 0;
	virtual bool dirVisible() = 0;
	virtual size_t maxConnections() = 0;
	virtual size_t maxConnectionsPerIp() = 0;
	virtual size_t maxConnectionSpeed() = 0;
	virtual size_t sessionTimeout() = 0;
	virtual size_t recvTimeout() = 0;
	virtual size_t sendTimeout() = 0;
	virtual size_t keepAliveTimeout() = 0;
	virtual bool getFirstFcgiServer(fcgi_server_ctx_t *serverInf) = 0;
	virtual bool getNextFcgiServer(fcgi_server_ctx_t *serverInf) = 0;
};

class IRequest : public IOStateMachine
{
public:
	virtual ~IRequest(){};

	virtual bool isValid() = 0;
	virtual HTTP_METHOD method() = 0;
	virtual std::string uri(bool decode) = 0;
	virtual std::string field(const char* key) = 0;
	virtual bool keepAlive() = 0;
	virtual bool range(__int64 &from, __int64 &to) = 0;
	virtual size_t contentLength() = 0;
	virtual void statistics(__int64* bytesRecved, __int64* bytesSent) = 0;

	virtual std::string getHeader() = 0;
	virtual IPipe* getPostData() = 0;

	virtual void nextRequest() = 0;
};

class IResponder : public IOStateMachine
{
public:
	virtual ~IResponder() {};

	/* 返回响应头 */
	virtual IRequest* getRequest() = 0;
	virtual std::string getHeader() = 0;
	virtual int getServerCode() = 0;
	virtual void statistics(__int64* bytesRecved, __int64* bytesSent) = 0;
};

class IResponderFactory
{
public:
	IResponderFactory(){};
	virtual ~IResponderFactory(){};

	virtual IResponder* catchRequest(IRequest* req) = 0;
	virtual void releaseResponder(IResponder* res) = 0;
};

/*
* HTTP Server 的抽象接口
*/
class IHTTPServer
{
public:
	virtual ~IHTTPServer() {};
	virtual int start(IHTTPConfig *conf) = 0;
	virtual int stop() = 0;

	/*
	* 捕捉请求
	*/
	virtual void catchRequest(IRequest* req, IResponder** res, IResponderFactory** factory) = 0;

	/*
	* 获取SERVER信息
	*/
	virtual bool mapServerFilePath(const std::string& url, std::string& serverPath) = 0;
	virtual std::string tmpFileName() = 0;
	virtual const std::string& docRoot() = 0;
	virtual bool isDirectoryVisible() = 0;
	virtual const std::string& defaultFileNames() = 0;
	virtual const std::string& ip() = 0;
	virtual u_short port() = 0;
	virtual size_t maxConnectionsPerIp() = 0;
	virtual size_t maxConnections() = 0;
	virtual size_t maxConnectionSpeed() = 0;
	virtual unsigned long sessionTimeout() = 0;
	virtual unsigned long recvTimeout() = 0;
	virtual unsigned long sendTimeout() = 0;
	virtual unsigned long keepAliveTimeout() = 0;
};

/* 外部使用的工厂函数 */
extern IHTTPConfig* create_http_config_xml(const char* fileName);
extern void destroy_http_config_xml(IHTTPConfig* conf);
extern IHTTPServer* create_http_server();
extern void destroy_http_server(IHTTPServer* svr);