#ifndef _IOCP_HEADER_PROTECTION_
#define _IOCP_HEADER_PROTECTION_
#include <map>
#include "Lock.h"
#include "IOSelector.h"

/*
* Windows系统完成端口(IOCP)模式下最小线程数,该数值应该大于0
*/
#define MIN_IOCP_THREADS 2
#define MIN_SOCKADDR_BUF_SIZE (sizeof(sockaddr_in) + 16)
#define INIT_RECV_BUF_LEN 128

#define IO_RECV_BUF_LEN (1024 * 2)
#define IO_SEND_BUF_LEN (1024 * 4)

#define IO_OPP_NONE 0
#define IO_OPP_ACCEPT 0x01
#define IO_OPP_CONNECT 0x02
#define IO_OPP_RECV 0x04
#define IO_OPP_SEND 0x08

/* 就绪状态IN(包括 accept 和 recv); OUT(包括 connect 和 send)*/
#define IO_READYTYPE_EPOLLIN 0
#define IO_READYTYPE_EPOLLOUT 1

/* 工作模式: LT模式(默认);ET模式 */
#define IO_MODE_ET 0
#define IO_MODE_LT 1

/*
* 套接字的类型,在WinSock2的基础上添加一个 SOCK_FILE 表示是文件句柄.
*/
#ifndef SOCK_STREAM
#define SOCK_STREAM     1               /* stream socket */
#define SOCK_DGRAM      2               /* datagram socket */
#define SOCK_RAW        3               /* raw-protocol interface */
#define SOCK_RDM        4               /* reliably-delivered message */
#define SOCK_SEQPACKET  5               /* sequenced packet stream */
#endif
#define SOCK_FILE 999

/*
* IOCP OVERLAPPED struct
*/
typedef struct iocp_overlapped_t
{
	OVERLAPPED olp;
	int oppType;
	byte* buf;
	size_t len;
	size_t ipos;  /* iocp operation pos */
	size_t upos;  /* user operation pos */
	bool et;	/* edge trriger flag */
	__int64 transfered; /* 总计传送的字节数 */
}IOCPOVERLAPPED;

class IOCPAdapter;
class IOCPContext
{
private:
	SOCKET _sock;
	int _sockType;
	bool _connected;
	bool _listening;

	SOCKET _newSock;
	char* _acceptBuf;
	LPFN_ACCEPTEX _lpfnAcceptEx;

	int _lastErrorCode;
	Lock* _lock;
	int _mode;
	int _shutdownFlag;
	iocp_overlapped_t _sendOlp;
	iocp_overlapped_t _recvOlp;

public:
	IOCPContext(SOCKET s, int sockType, size_t recvBufLen, size_t sendBufLen);
	~IOCPContext();

	// 一个IO操作完成,返回当前需要触发的事件
	u_int onIoCompleted(bool oppResult, IOCPOVERLAPPED* olp, size_t bytesTransfered);

	// 根据IOCPContext的内部状态返回需要触发的事件
	SOCKET attach(SOCKET s);
	SOCKET detach();

	u_int updateEvent(u_int ev);

	bool busy();
	bool ready(int t);
	int setMode(int m);
	int setLastError(int err);
	int getLastError();

	Lock* setLock(Lock* l);
	void lock();
	void unlock();

	// iocp 操作(无锁)
	int postAccept();
	int postRecv();
	int postSend();
	int postConnect(const char *ipAddr, u_short port);
	int shutdown(int how);

	// 读写内部缓冲区(有锁)
	SOCKET accept();
	int connect(const char* ipAddr, u_short port);
	int recv(void* buf, size_t len);
	int send(const void* buf, size_t len);
	size_t size(bool r);

#ifdef DERIVE_FROM_PIPE
	size_t _pump(reader_t reader, size_t maxlen);
#endif 

	/* 仅供IOCPSelector使用的变量 */
	IOCPAdapter* adapter;
	bool isAutoDelete;
	bool isInQueue;
	u_int epollEvent;
	u_int curEvent;
};

class IOCPSelector;
class IOCPAdapter : public IOAdapter
{
protected:
	SOCKET _s;
	int _sockType;
	IOCPContext *_ctx;
	IOCPAdapter *_newAdp;

public:
	IOCPAdapter(int t);
	IOCPAdapter(SOCKET s);
	IOCPAdapter(const char* filename);
	~IOCPAdapter();

	/*
	* 各种不加锁的工具函数
	*/
	inline SOCKET getSock() { return _s; }
	inline int getSockType() { return _sockType; }
	IOCPContext* attach(IOCPContext* ctx);
	IOCPContext* detach();
	IOCPContext* getContext();
	
	/*
	* IOAdapter interface
	*/
	int setopt(int optname, const char* optval, int optlen);
	int getopt(int optname, char* optval, int* optlen);
	int shutdown(int how);
	int getsockname(char *ipAddr, u_short &port);
	int getpeername(char* ipAddr, u_short &port);
	int getLastError();
	int query(__int64* bytesReceived, __int64* bytesSent);

	IOAdapter* accept();
	int connect(const char *ipAddr, u_short port);
	int bind(const char* ipAddr, u_short port);
	int listen(int backlog = SOMAXCONN);
	int recv(void *buf, size_t len);
	int send(const void* buf, size_t len);

	/*
	* IPipe interface
	*/
#ifdef DERIVE_FROM_PIPE
protected:	
	size_t _read(void* buf, size_t len);
	size_t _pump(reader_t reader, size_t maxlen);
	__int64 _size();
#endif 
};


/*
* IOCP选择器,管理所有IOCPAdapter,后台运行线程取回IO操作的结果,设置对应的IOCPAdpater的状态,维护一个"活跃"Adapter的队列.
*/
typedef std::list<IOCPAdapter*> iocp_adp_list_t;
typedef std::map<IOCPAdapter*, IOCPContext*> iocpctx_map_t;
typedef std::list<IOCPContext*> iocpctx_list_t;
class IOCPSelector : public IOSelector
{
private:
	HANDLE _iocpWorker; /* IOCP:A 后台工作端口 */
	HANDLE _iocpBroadcast; /* IOCP:B 广播端口 */
	int _threads;
	uintptr_t* _tids;
	LockPool<Lock> _lockPool;
	Lock _lock;
	iocpctx_list_t _ctxList;

	void onIoCompleted(IOCPContext* ctx, bool oppResult, IOCPOVERLAPPED* olp, size_t bytesTransfered);
	void lock();
	void unlock();

public:
	IOCPSelector();
	~IOCPSelector();

	IOCPContext* allocContext(IOCPAdapter* adp);
	void freeContext(IOCPContext* ctx);
	Lock* allocLock();
	void freeLock(Lock* l);
	int raiseEvent(IOCPContext* ctx, u_int ev);
	int loop();
	bool closeContext(IOCPContext* ctx);

	int ctl(IOAdapter* adp, int op, u_int ev);
	int wait(IOAdapter** adp, u_int* ev, u_int timeo = INFINITE);
	int wakeup(size_t count);
};

#endif
