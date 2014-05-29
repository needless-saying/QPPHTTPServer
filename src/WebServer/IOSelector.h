#pragma once

/* Copyright (C) 2014 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

/*
 * V0.3
 *
 * 网络模块的设计目的是实现一个相对一致的接口封装各个平台比较流向的IO复用模式, Windows IOCP 和 Linux EPOLL,总体行为趋向类Unix epoll 的 ET 模式.
 * Windows IOCP实现简单的说就是为每个套接字单独分配接收缓冲和发送缓冲,调用者提交数据后,后台(IOCPSelector)有线程刷新2个缓冲区,符合条件时触发可读/
 * 可写事件.调用者通过 IOSelector 的epoll函数得到活跃(有事件被触发)的 IOAdapter.
 * 主要目的是避免回调函数的设计,个人部喜欢回调函数,代码被动运行感觉上不够优雅.
 *
 * IOAdapter 封装了套接字的全部细节
 * IOSelector封装了网络模型的细节.
 *
 * 调用说明:
 * 1. 通过accept得到的IOAdapter初始处于可读可写的状态.
 * 2. 调用 recv 返回 IO_ERROR_SUCESS 表示数据已经被复制到指定缓冲区内,长度由len参数指定; 返回IO_ERROR_RECVPENDING表示IO操作正在进行; 返回
 *    IO_ERROR_RECVFAILD 表示套接字异常.
 * 3. 调用send和recv的情况类似.
 *
 * 调用限制: 
 * 1. 一个IOAdapter 最多允许最多有一个 RECV IO 和 一个 SEND IO 同时进行或者只允许一个 IO(由宏定义 DOUBLEIO 控制). 调用者应该确保这一点(即程序
 *    逻辑应该确保只有在收到可读/可写通知后,调用recv/send函数直到返回IOPENDING为止).
 * 2. IOAdapter 由 IOSelector创建,所以应该先删除所有的 IOAdapter 再删除IOSelector.
 * 
 * by Q++ Studio 2014.3.25

 * IOCPSelector 和 IOCPAdapter 是一体的,逻辑上并不是松散的耦合关系而是紧耦合,只是提供了一个统一接口的两个部分,以方便使用.
 * 和 伯克利 recv 有一点点不一样的地方: recv 返回0时,调用getLastError 返回 IO_ERROR_SUCESS 表示没有错误发生,只是缓冲区内没有数据了,不再提供一个单独的 EAGAIN 或者 WSA_IO_PENDING之类的错误码.
 * 
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
* 派生至IPipe接口以支持Q++ HTTP Server V0.3 管道系统
* 实现IPipe接口可以直接访问IOAdapter内部的套接字缓冲区以节省内存
* 如不需要这个功能把 #define DERIVE_FROM_PIPE 删除即可
*/
#define DERIVE_FROM_PIPE
#ifdef DERIVE_FROM_PIPE
#include "Pipes/Pipe.h"
#endif

/*
* 工具宏函数,望名知义
*/
#define TEST_BIT(val, bit) (val & bit)
#define SET_BIT(val, bit) (val |= bit)
#define UNSET_BIT(val, bit) (val &= ~bit)

/*
* Last Error Type define
* Windows系统下是 WSAGetLastError 的概括(各种本地错误被合并). Linux系统下自定义.
*/
#define IO_ERROR_SUCESS 0
#define IO_ERROR_BINDFAILED 1			// 绑定失败,端口被占用.
#define IO_ERROR_LISTENFAILED 2			// 侦听失败,已经连接或者其他本地错误.
#define IO_ERROR_ACCEPTFAILED 3			// 接受失败,无法获得AcceptEx指针或者其他本地错误.
#define IO_ERROR_RECVFAILED 4			// 接收失败,本地错误.
#define IO_ERROR_SENDFAILED 5			// 发送失败,本地错误.
#define IO_ERROR_CONNECTFAILED 6		// 连接失败,本地错误.
#define IO_ERROR_BUSY 10				// 操作正在进行中.
#define IO_ERROR_HUP 11					// 连接已断开.
#define IO_ERROR_SHUTDOWN 30			// 已设置SHUTDOWN标记.
#define IO_ERROR_UNDEFINED 100

/*
* 套接字封装接口
*/
class IOAdapter : public IUserDataContainer

#ifdef DERIVE_FROM_PIPE
	, public IPipe
#endif

{
protected:
	IOAdapter(){};
	virtual ~IOAdapter(){};

public:
	virtual int setopt(int optname, const char* optval, int optlen) = 0;
	virtual int getopt(int optname, char* optval, int* optlen) = 0;
	virtual int getsockname(char *ipAddr, u_short &port) = 0;
	virtual int getpeername(char* ipAddr, u_short &port) = 0;
	virtual int getLastError() = 0;
	virtual int query(__int64* bytesReceived, __int64* bytesSent) = 0;	
	virtual int shutdown(int how) = 0;

	virtual int bind(const char* ipAddr, u_short port) = 0;
	virtual int listen(int backlog = SOMAXCONN) = 0;	
	virtual IOAdapter* accept() = 0;
	virtual int connect(const char* ipAddr, u_short port) = 0;
	virtual int recv(void *buf, size_t len) = 0;
	virtual int send(const void *buf, size_t len) = 0;
};


/*
* ctl 函数的OPP定义和EVENT定义
*/
#define IO_CTL_ADD 0
#define IO_CTL_MOD 1
#define IO_CTL_DEL 2

#define IO_EVENT_NONE 0
#define IO_EVENT_EPOLLIN 0x01 /* recv or accept available */
#define IO_EVENT_EPOLLOUT 0x02 /* send available or connect done */
#define IO_EVENT_EPOLLERR 0x04 /* error occur */
#define IO_EVENT_EPOLLHUP 0x08 /* auto set */
#define IO_EVENT_EPOLLET 0x10
#define IO_EVENT_EPOLLONESHOT 0x20
#define IO_EVENT_AUTODELETE 0x80 /* 内部使用*/

/*
* wait() 返回值
*/
#define IO_WAIT_SUCESS 0 /* 取得了一个活跃IOAdpater */
#define IO_WAIT_TIMEOUT 1 /* 超时 */
#define IO_WAIT_ERROR 2 /* 出错 */
#define IO_WAIT_EXIT 3 /* 退出 */

/*
* 选择器封装接口
*/
class IOSelector
{
protected:
	IOSelector(){};
	virtual ~IOSelector(){};

public:
	virtual int ctl(IOAdapter* adp, int op, unsigned int ev) = 0;
	virtual int wait(IOAdapter** adp, unsigned int* ev, unsigned int timeo = INFINITE) = 0;
	virtual int wakeup(size_t counts) = 0;
};

/*
* 只允许通过工厂类创建 selector 实例
*/
extern IOSelector* IO_create_selector();
extern int IO_destroy_slector(IOSelector* s);
extern IOAdapter* IO_create_adapter();
extern IOAdapter* IO_create_adapter(const char* filename);
extern int IO_destroy_adapter(IOAdapter* adp);
extern bool IO_formate_error_message(int errorCode, std::string* msg);