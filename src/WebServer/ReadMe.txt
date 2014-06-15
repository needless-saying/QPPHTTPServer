V1.5
====

1. 所有需要同步的操作,都调用 HttpFileServer 的成员, 避免死锁. 
不要把 关键段对象 传给线程函数.

2. 需要发送状态信息的, 调用  HttpFileServer 的 OnError() 函数, 传递一个结构体.
包含 N1, N2 和 STR , 只传值, 不传递字符串.
字符串又界面函数处理.
使用 SendMessage() 阻塞处理.


/////////////////
分别定义一个 HTTPRequest 和 HTTPResponse 类, 包装对应的操作.
LinstenProc 和 ServiceProc 只负责套接字操作, 接收发送. 其他不管.

1. HTTPRequest.PushData() 方法接收客户端请求.
2. HTTPResponse.PopData() 弹出响应数据给客户端.
3. HTTPServerImp中另外写一个函数 GetResponse(),根据HTTPRequest生成一个响应HTTPResponse
即使客户端的参数,格式错误,或者使用了非GET方法,也应该响应,提示客户端.


// 
V1.52Beta Build 152710

1. 在状态栏显示当前使用的带宽.
2. 在状态栏显示当前一共有多少个连接.

3. _wfindfrist 应该改为 _wfindfirst64
4. 超出的日志每次只移除前面100行(原来移除200行)
5. tagClientInf 新增两个字段,分别记录连接开始和连接结束的 GetTickcount 以统计一个连接一共占用了多长时间.
6. HTTPServerImp 添加 OnRecv() 和 OnSend() 函数,把原来SericeProc中关于接收和发送的代码分别移入这两个函数中,这样逻辑更清晰.
ServiceProc 线程函数只负责驱动 HTTPServerImp.

7. 允许设置最大连接数

V1.52 Build 152711
OnTimer() 没有正确同步. 修正: 计算带宽时有可能会有微小的误差.

V1.52 Beta 152712
CLIENTINF 关于统计发送字节数应该用 __int64 类型.

V1.52 Beta 152713
下行带宽统计错误,误写了一个变量.

V1.52 Beta 152714
1. 浏览目录时,超过1G的文件显示为 GB 而不是 MB.
2. 设置 WSARecv 和 WSASend 超时,已减少死连接.
3. 统计带宽时,内部使用原子锁同步,以提高效率.(原来使用临界段变量)
4. 连接刚被接受时,只发去一个缓冲区为0的 WSARecv 操作,以节省资源.(在我们这个程序中没有必要.)
5. 第一个IOCP线程兼具AcceptEx()的作用,这样可以节省一个侦听线程,并且可以有效避免死连接.

V1.52 Beta 153715
1. 工具条按钮的颜色.

2. 可以设置每个IP最多多少个连接.
另外做一个map: key = ipaddress string, value = connection count, OnAccept()中检查这个map.

3. 可以设置每个连接的最大带宽.

(1) 在OnSend()中,计算当前连接的带宽,如果超过指定值,那么设置一个定时器,在若干MS后调用 OnSend()发送,而当前执行的OnSend()直接返回.
可能需要另一个TimerQueue
如果等待的时间超过死连接超时,则要修改死连接超时并且增加额外的50MS使下次调用 OnSend()前不会因为速度限制延时发送而被判定为死连接
OnSend()增加一个 bDelay 的参数以标识是否是延时调用.

(2) 另一种方法
在OnSend()中计算当前带宽,如果超过指定值,则计算最小应该发送多少个字节,而不在发送最大缓冲的数据.
使带宽逐渐降低到指定值.

4. 把侦听套接字也用完成端口处理,加大并发处理能力.

5. 连接结束时,计算平均速度.注意除数为0的情况.

6. 修正错误:当一个请求头被分为多次接收时,会导致连接被关闭.

7. 保存是否禁用日志的设置.

8. 允许启用文件日志.

9. 在禁止浏览目录的情况下,允许设置默认文件名

10. 创建3个定时器队列,分别对应3种定时器,以提高性能.

V1.52 Build 152716

V1.52 Build 152718
1. doAccept()中的OnAccept()调用有小问题.
2. 日志信息修改,所有ip地址和端口都放在一行的最前面.

===========
V2.00 Beta

Bug Fix list:
1. 执行网络操作时,如果操作成功返回但是传送的字节数为0判断为是客户端关闭连接.
2. HTTPServerImp::mapServerFile()中 "strFilePath.end()--" 改为 "--strFilePath.end()" 才能去除最后的"\"以得到正确的路径.
3. 由于限制带宽而延时发送,导致的定时器资源泄漏.并且如果此时客户端关闭了连接将导致死锁.
4. 会话超时时,在不确定另一个线程运行状况的情况下,直接关闭其有可能使用的套接字句柄,将导致一个同步问题. 现在更改会话超时的检测策略.

--------------------
2011-12-04
主要目的: 添加 FastCGI 支持,实现 remote 模式,并考虑 locale 模式扩展.
不管 remote 还是 locale 只是数据通道的区别,上面的核心代码不应该受此影响.

网络驱动模块的原则: 任意时刻,每个套接字只有一个读或者一个写或者一个读一个写操作.

0. 科普 - CGI 和 FastCGI

0.1 什么是CGI? 它是一个标准,详见(http://tools.ietf.org/html/rfc3875),站在Web服务器的角度,如何让我的服务器支持CGI?
0.1.1 在 UNIX 环境下, 通过 pipe(), fork(), execve(), dup(), dup2() 等函数由Web Server进程创建 CGI 子进程,并且把 stdin 和 stdout 重定向到管道,
同时用HTTP参数生成一个 env 表,通过 execve() 传递到 CGI 子进程从而实现 Web Server 和 CGI Process 之间的数据传递.
0.1.2 在Windows 环境下, 使用文件系统: Web Server 把参数写入一个临时文件然后启动 CGI 子进程, CGI 子进程把处理结果输出为指定的另一个文件.

0.2 什么是 FastCGI? 它是一个开放协议,详见(http://www.fastcgi.com/).
0.2.1 FastCGI Application 有本地模式和远程模式两种运行模式. 如果 FastCGI Application 运行在远程,那么FastCGI Application将监听
一个TCP端口, Web Server连接该端口后按照 FastCGI 协议与之交换数据.
0.2.2 如果是 Unix 环境,可以使用 Unix域套接字(类似于管道)交换,如果是Windows环境只能用TCP连接了.
0.2.3 连接建立后,按照 FastCGI 协议交换数据,控制 FastCGI Application 的生存周期,并且连接可以保留,而不是每次都关闭.

以上,可以参考 Lighttpd 的源代码.

1. 要支持POST方法,客户端递交的数据由一个HTTPContent管理, FastCGI 模块从该对象读取数据.

1.1 HTTPContent 负责从客户端读取数据,处理chunked编码? 至少记录统计数据,然后再递交给 FastCGI 连接.
1.2 

2. FastCGI 连接也一起由完成端口线程驱动, 由 CFastCGIConn 封装, 包含在tagClientInf中.

2.1 CFastCGIConn 方法
2.1.1 Connect(),Close()等.
2.1.2 Proc(),作为网络事件完成后的回调函数,在该函数内部完成 FastCGI 协议规定的动作.

3. 是否维持一个 CFastCGIConn 连接池?

4. 是否维持一个 FastCGI Application 的进程池?

2011-12-05

1. 定义 CFastCGIFactory 用来维护本地FastCGI进程池和连接池.
1.1 CFastCGIFactory::GetConnection() 分配一个活跃FCGI连接.
1.2 CFastCGIFactory::Catch() 根据扩展名如PHP判断是否由 FastCGI捕获当前请求,如果不捕获则默认处理.
1.3 CFastCGIFactory捕获请求后,逻辑上所有的后续处理全部由 CFastCGIConn 处理,所以不再生成Response对象,由CFastCGIConn直接操作客户端连接套接字.

2011-12-06

1. 分离网络模块, IOCPNetwork, 负责创建套接字,关闭套接字,管理定时器队列包括死链接超时和会话超时.
上层模块通过 intptr_t (就是SOCKET句柄) 操作,但是不直接操作套接字.
定义一个纯虚接口,用来接受回调事件,如 OnRead() OnRecv() OnClose() OnTimeout() 等.
或者只是一个回调函数resultfunc_t,因为都依赖于GetQueuedCompletionStatus()的返回结果,网络模块并不知道是send,recv还是accept.

或者 IOCPNetwork 不创建套接字,只是通过 registerSocket(bool add_or_remove) 登记/取消 要用IOCP管理的套接字.
这样似乎更合理一些.

2. HTTPServerImp
2.1 HTTPServerImp::CatchFastCGIRequest() 捕获FastCGI请求
2.2 HTTPServerImp::ReleaseFastCGIRequest() FastCGI模块处理完FastCGI请求后,调用该函数以回收资源.

3. 核心代码(除了界面)去除MFC依赖
3.1 用 _beginthreadex 或者 _beginthread 代替 AfxBeginthread
3.2 尽量用运行时库函数(C/C++标准库函数 > Microsoft CRT (MS 对标准库的扩展) > 直接调用Windows API) / 数据结构用STL,不要用MFC的库.
3.3 我现在更倾向于编写ANSI兼容的UTF-8编码的程序而不是UNICODE版,因为函数调用的问题,只有windows平台下有那些 w_**** 函数.
并且我认为 ANSI兼容的程序才是程序的本来面目,对于中文来说内码只是编码方式并不影响程序逻辑,而全部使用UNICODE版本的API不仅小题大作费时费力而且完全没有意义. UNICODE不仅是必要的而且是好的,用UNICODE函数代替ANSI函数重新写代码不仅是没必要的而且是愚蠢的.要解决中文乱码问题
像LINUX一样,编译用UTF-8,函数接口用ANSI,显示环境用UTF-8才是合乎逻辑的,完美的解决方案.

2011-12-07

1. 允许绑定到本机的某个IP地址或者所有,并在日志或者界面的某个位置显示当前绑定的IP地址.

2. 在HTTPServerImp中包含一个 ServerEnv 作为 asp.net / jsp / php 编程时用到的 "server"对象的实现.
只提供一些静态的,需要对外公布的数据.
2.1 mapPath()
2.2 maxConnections()
2.3 maxSpeed() / maxConnSpeed()
2.4 sessionTimeout()
2.5 deadTimeout()
2.6 rootPath();
2.7 isDirectoryVisible();
2.8 port();
2.9 ipAddress();
2.10 defaultFileNames();
2.11 curConnectionsCount(); // 和 SOCKINFMAP.size() 重复了.

3. 可以把 ServerEnv 传递给普通连接(由 HTTPServerImp 本身管理) 或者 任何其他支持的连接如FastCGI连接.

4. HTTPApplication 表示这个HTTP应用,而把HTTPServerImp改名为 HTTPServerImp 后用来表示HTTP应用中的一个服务,就像在虚拟主机上建多个网站一样.
HTTPServerImp拥有自己的网络模块,管理自己的所有连接. HTTPApplication 只是维护一个或多个HTTPServerImp实例.
似乎也没什么必要...HTTPApplication的功能太少了,没有存在的必要,界面代码直接创建多个 HTTPServerImp实例即可.

2011-12-08

1. HTTPServerImp 添加一个 ServerEnv 对象,用来维护 "服务器" 相关的静态信息和配置,是 PHP 中 SERVER 的实现,需要把 ServerEnv 对象传递给 
HttpConnection 和 FastCGIConnection.

2. 逻辑上需要 HttpConnection 管理普通的HTTP连接和 FastCGIConnection 管理FastCGI 连接,但是由于 HttpConnection 功能并不多,所以直接把代码
合并到 HTTPServerImp 中.

3. FastCGIConnection::proc(ServerEnv* svr ...) HTTPServerImp根据proc()的返回值判断 FastCGI 模块是否已经处理完这个请求以回收资源.

==================
2011-12-21

1. 用优先队列(最小堆)和一个线程实现定时器. 还是不要使用 Timer Queue, Timer Queue使用了太多线程.

创建一个WaitableTimer和一个Event,记录最近的超时时间. 定时器线程使用 Muti-Wait等待着2个对象.
当有新的定时器需要加入时,SetEvent是定时线程醒来,然后比较得到需要等待的时间,再调用 SetWaitbleTimer.


2. 把所有网络相关的功能,发送接收超时,速度限制等都集中的网络模块中.创建一个独立的线程用来执行延时操作,定时器到期后只插入延时队列,然后就返回尽量减少定时器线程的执行时间.

3. 对网络模块的所有操作加锁以实现如下目的
3.1 允许同时有一个接收请求和一个发送请求.
3.2 允许超时控制.
3.3 运行单独控制发送速度限制和接收速度限制.

所有的操作都加锁还是有意义的,因为回调函数并不加锁,虽然网络动作都是顺序执行,但是回调函数可以并行执行.

2011-12-22
1. 网络模块只实现对IOCP多线程模型的最小封装,既不做同步控制也不保存 OVERLAPPED 数据结构.
定义结构 IOCPOVERLAPPED 多一个字段以保存回调函数的地址.
不管怎样设计,要达到并发接收和发送前提下的超时控制,总要同步控制而且上层调用者也需要做同步控制,所以作为网络模块还是做最小设计.

2. 用队列和WaitableTimer实现定时队列. 作为超时,速度限制的定时器.
会话超时还是在 OnSend 和 OnRecv中检测,提高效率.

2012-1-2
1. 用 QueryPerformanceCounter 实现了精度1ms的定时器队列. 虽然对本程序没什么意义.

2012-1-18
1. 又绕回去了... IOCPNetwork 管理重叠结构,提供超时,速度限制功能,否则网络设置如何应用到FCGI模块中?
2. 定时IHTTPServer接口,供其他模块(如FCGI)回调,这样HTTPServerImp才能获得信息,如已发送的字节数,是否关闭等.


2012-2-5
1. HTTPServerImp对象只管理侦听套接字,新连接被接受后, HTTPServerImp对象根据url扩展名的类型创建不同类型的HTTP连接对象.
HTTPConnection 或者 FCGIConnection.
2. HTTPConnection对象管理一个普通的HTTP请求,目录列表或者静态文件.直接操作网络模块,处理回调函数.
3. FCGIConnection对象管理FCGI请求.直接操作网络模块,处理回调函数.
4. HTTPConnection 等连接对象处理完数据之后,回调 HTTPServerImp 对象的 onClose 接口,用来回收资源.

2012-2-9
1. HTTPContent 包含在 HTTPRequest 和 HTTPResponse 中,对外部不可见.
2. HTTPRequest 和 HTTPResponse 分别提供 push 和 pop 方法,一个只写,一个只读.
3. HTTPResponse 提供 setFile 和 setData 用来设置文件或者数据,隐藏 HTTPContent 的存在.

2012-2-13
1. HTTPRequest 提供 recvFromClient 函数,并且管理socket buffer, HTTPServerImp对象在接收到新客户连接后,只生成一个 HTTPRequest 对象,
然后调用 recvFromClient(). 网络模块依然由 HTTPServerImp 管理.
2. HTTPResponse 提供 sendToClient 函数,同理.
3. FCGIResponse 提供 recvFromFCGI, sendToFCGI, sendToClient 函数.
4. 不再有 HTTPConnection 对象的存在.

5. HTTPRequest 和 IResponse 分别提供用于统计的函数,计算接收和发送的数据.


========================================
1. 一个connection分为两个逻辑部分
1.1 HTTPRequest,负责接收和解析HTTP请求,管理对应的网络事件.
1.2 HTTPResponder,负责发送相应,管理对应的网络事件.

2. IHTTPServer 提供 onRequest 和 onResponser 回调.

3. FCGIResponder 和 HTTPResponder 都派生自接口 IResponder

4. HTTPResponseHeader

2012-2-14
1. 支持 keep-alive 选项.现在保留连接用于处理多个请求.

================================

2012-2-17
只剩下FCGIResponder需要处理了.

1. 用一个 memfile 作为FCGI发送缓冲,可以把多个变长的FCGIRecord写入memfile,然后发送整个memfile 之后,再做下一步处理
这样就解决了使用固定缓冲区长度溢出问题.

2. 发送玩 FCGI_BEGIN_REQUEST和 FCGI_PARAMS之后,就进入了最复杂的状态
2.1 从HTTP读取STDIN
2.2 发送STDIN到FCGI
2.3 从FCGI读取STDOUT
2.4 发送STDOUT到HTTP
以上四步可能同时进行,同步控制应该怎么做呢?

2012-2-22
1. 参数发送成功后分为两条线处理 FCGIResponder
1.1 HTTP -> STDIN
1.2 STDOUT -> HTTP
这两条线可以并发进行,只需要两个缓冲区即可.
模仿 FCGIContent , 添加 eof() 只有收到 FCGI_END_REQUEST 之后 eof() 才返回true.

2012-2-23
FCGIWriterPipe - 类似过滤器的方式,把普通数据输入FCGIPipe 得到打包好后的输出数据.
FCGIReaderPipe - 把从FCGIServer收到的数据输入,得到解包后的普通的数据
需要有自己的缓冲区.

2012-2-28
phpinfo()函数已经能成功运行得到结果.
现在需要再考虑程序结构的问题了.
目标:用最少的缓冲区实现.

实现一个类似管道的 socket_buffer_t, 从FCGI server 收到数据后可以写入缓冲,分多次也没问题,大小可以自动增长.

2012-3-2
2.0版本最后要做的事:
1. 去掉 UNICODE, 还是使用 ANSI-UTF8编程比较好. 才是程序的本源. 字符编码仅仅是显示问题.
2. 用 HTTPConf 类保存配置XML格式.


2012-3-6
FCGIResponder 收工. 
现在考虑 FCGIFactory 管理模块的问题
1. 远程FCGI server 连接的统一管理,回收.
2. 本地FCGI 进程的管理,回收.
3. 错误处理和恢复,远程TCP连接端口,本地进程无响应时,应该怎么做.
4. 同一个TCP/PIPE,多个FCGI请求并发时,怎么做.

2012-3-9
由于多个 FCGIResponder 需要共享同一个FCGI连接(套接字或者管道),所以必须由 FCGIFactory 来管理FCGI连接.
对来自不同的 FCGIResponder 的发送请求排队,以确保数据流中 FCGI Record 的完整性.
同时也需要对 接收请求 排队: FCGIFactory 不能无限制的缓存来自 FCGI连接的数据(控制内存数量),
必须在 FCGIResponder 提交接收请求之后才使用 FCGI连接收取数据,并且需要保证 FCGI record 的完整性.

2012-3-11
如果多个 FCGIResponder 复用同一个FCGI连接,那么各个 FCGIResponder 会相互影响,速度快的客户端需要等待速度慢的客户端.
因为不能缓存过多数据.
另一种思路:  FCGIFactory 实现一个连接池,对每个 FCGIResponder 分配一个单独的FCGI连接,用完后回收.这样,虽然看似消耗多一些,但是
结构上比较简单,而且相互之间不受影响.
缺点: 并发连接数受到限制,尤其是本地模式下, 无法启动过多的FCGI server进程,如果某个客户端的响应时间长就会导致该FCGI连接(进程)
一直被占用.
在本地用远程模式运行FCGIserver是一个折中方案,就像 nginx 那样.

2012-3-12
对于使用连接池的方案,可以增加一个等待队列并记录入队时间(调用getConnection() 中提供一个回调函数). 但有连接被释放时(即 releaseConnection
被调用时)查看等待队列,如果有 FCGIResponder 正在等待则调用回调函数,同时要检查等待时间,如果等待时间过长,则发送 HTTP 503.
如果等待队列过长,则移除队头记录并发送 HTTP503.

远程模式下,套接字的 connect 动作由 FCGIFactory 来执行还是 FCGIResponder 来执行呢?
逻辑上考虑应该由 FCGIFactory, 问题是 connect 是一个异步操作,如果由 FCGIFactory 执行,则必须要回调操作.

2012-3-13
connect 动作由 FCGIFactory 执行, 创建套接字后,进入忙碌队列,设置FCGI连接状态为 CONNECTING,同时 FCGIResponder 进入等待队列.
连接完成后,回调.
原则: 凡是 getConnection 得到的FCGI连接都是可直接用的连接.

2012-3-14
对FCGI Windows NT下命名管道的使用理解有误.
看来一个 PHP-CGI 进程同时只能处理一个连接(因为只有一个命名管道实例即STDIN),监听命名管道, HTTP server 不可能知道到底是哪一个命名管道实例(PHP-CGI)在处理当前连接,除非使用不同的管道名称.
必须要实现一个进程池,创建和维护 PHP-CGI 进程. FCGIProcessContainer
有多少个 PHP-CGI 进程就有多少个命名管道实例,就能创建多少个FCGI连接而不阻塞.

2012-3-15
每个PHP-CGI进程对应一个单独名称的命名管道,对应一个FCGI连接.
这样做的好处是HTTP服务器可以通过FCGI连接的状态判断对应的那个PHP-CGI进程是否正在处理请求.
目的是: FCGIFactory 要有弹性的管理若干个(比如25个)PHP-CGI进程,在压力低的时候,启动尽可能少的 PHP-CGI进程,一旦发现 空闲的PHP-CGI进程数(即空闲的FCGI连接数)超过一个特定数值(比如5个),则杀死一定比例(比如3个,只保留2个空闲进程在空闲队列内等待下一次使用)并且空闲时间超过一定数值(比如5秒)的PHP-CGI进程.

2012-3-18
FCGIResponder
如果不是因为FCGI server的原因非正常中断,则应该发送一个 FCGI_ABORT请求中断服务.
这样归还到FCGIFactory的连接就可以正常继续使用.

2012-3-19
大量使用回调函数其实是一种很不好的设计,对函数的调用顺序,路径必须要非常清楚.
尤其是在有同步锁的情况下,容易造成死锁.
问题是对应这种网络事件驱动的程序不用回调函数又有什么其他办法呢? 生产者-消费者模型?
越做越觉得软件架构的重要性,可惜我醒悟得太晚.

2012-3-23
为了尽量降低FCGI脚本的运行时间,需要缓存来自FCGI的响应
1. 用 memfile 在内存缓存.
2. 用 tmpfile 在磁盘中缓存.

同样也需要缓存来自HTTP的输入
1. 用 memfile 缓存.
2. post 的文件需要写入磁盘的临时文件.

HTTP Server 要创建一个文件夹用于存放临时文件.

HTTPRequest 是否在收到所有数据之后才回调 IHTTPserver onrequest?
长度短,则放入 memcontent,长则放入 file_content.

2012-3-24
只是FCGIResponder需要接收 request input, 所以只在 FCGIResponder 中缓冲数据.

2012-3-26
HTTPRequest 在接收完POST DATA之后才回调HTTPserver.

2012-4-1
"退出点"
FCGI->Cache 和 Cache->HTTP 两个数据流启动后就一直运行,直到有错误发生或者数据流执行完毕才结束.
每个数据流在IO操作完成的一刻间检查另一个数据流的状态,如果发现另一个数据流已经发生错误,则退出.这"一刻间"就是退出点.

2012-4-6
1. FCGIResponder 退出点在:
1.1 进行下一个网络操作前
1.2 网络操作失败时

2. 会话超时如何实现
HTTPServerImp为每个连接分配一个定时器,定时器超时时,调用 IRequest::stop() 和 IResponder::stop(),最大问题依然是同步.

3. IOCPNetwork 还需要在斟酌一下同步控制, onIoFinished 中何时删除定时器比较好?

2012-4-9
应该在 mapServerFile() 中处理默认文件名的问题. 对于 HTTPRequest, HTTPResponder, FCGIResponder 等对象来说默认文件名是透明的.

由于系统对应使用 fopen() 同时打开的文件数有限制(2048),如何支持更多的并发连接数呢? 排队?
看看 lighttpd 如何实现.

http://stackoverflow.com/questions/870173/is-there-a-limit-on-number-of-open-files-in-windows

If you use the standard C/C++ POSIX libraries with Windows, the answer is "yes", there is a limit.

However, interestingly, the limit is imposed by the kind of C/C++ libraries that you are using.

I came across with the following JIRA thread (http://bugs.mysql.com/bug.php?id=24509) from MySQL. They were dealing with the same problem about the number of open files.

However, Paul DuBois explained that the problem could effectively be eliminated in Windows by using ...

    Win32 API calls (CreateFile(), WriteFile(), and so forth) and the default maximum number of open files has been increased to 16384. The maximum can be increased further by using the --max-open-files=N option at server startup.

Naturally, you could have a theoretically large number of open files by using a technique similar to database connections-pooling, but that would have a severe effect on performance.

Indeed, opening a large number of files could be bad design. However, some situations call require it. For example, if you are building a database server that will be used by thousands of users or applications, the server will necessarily have to open a large number of files (or suffer a performance hit by using file-descriptor pooling techniques).

My 2 cents.

Luis

直接使用 Windows API 创建/打开/读写/关闭 文件 class OSFile 而不再使用 C的流文件 fopen 等. 然后再使用等待队列进一步增大并发数.

2012-4-10
FCGIResponder 中2个使用了大局部变量的函数 sendPostData() 直接使用发送缓冲 reserve(), initFCGIEnv() 也如此,避免栈溢出.

2012-4-11
XmlDocument 在两处使用递归函数的地方使用栈和循环代替.

2012-4-15
1. XmlDocument 处理 xml协议节点的方式还要在斟酌一下.
2. XMLNode::GetNode 缓冲区越界的问题.

2012-4-16
如何简单的支持 XPath
1. 对于绝对路径使用 /root/child1/child2
2. 对于相对路径使用 ./child1/child2 或者 nodename/child1/child2

目前就先这样好了.

定义一个 class XPath 
1. 构造函数接受一个字符串.
2. bool GetFirst()
3. bool GetNext()
4. bool IsAbsolutePath()

或者 

XPath(const std::string &path, XmlDocument *doc);
XMLHANDLE XPath::GetNode();

问题: XPath 是直接操作 XmlDocument 好还是只是提供解析 XPath 字符串的功能好?

2012-4-17
XMLNode 内码还是UNICODE,用不用 UTF-8?
OS_Conv() 的接口重新设计
int OS_Conv("utf-8", "gb2312", src, srcLen, dest, destLen);
Windows 平台下的具体实现要囊括所有的中文编码.

====================
改进XML模块,减少内存的使用量
1. 分析函数 LoadNode() 直接接受 char字符串输入.
2. 分析函数 LoadNode() 可分块调用,每个状态都可以恢复.
3. 输出函数 GetNode() 可分块调用.

2012-4-22
v0.2版的功能已经实现.
1. IOCPNetwork 和 XmlDocument 两个类还需要再改进.
2. XPath 要实现,这样 xml 模块才能真正发挥作用. 返回节点集的时候,可以用类似智能指针的技术,返回一个 new 的 list指针, 用一个类包装它,析构是自动删除.

===========================
2012-5-17
1.重新整理 FCGIResponder 的三个数据流的代码,现在的代码写得太恶心了.
2.添加全缓冲模式.

====
2013-8-22

V0.3

1. 用管道的概念改写: HTTP请求通过 Socket/SSL 管道提交给 IRequest, IResponder 通过: File/Buffer管道 -> Socket/SSL管道
IResonder通过: File/FCGI管道->Cache管道->Socket/SSL管道把数据把数据送回去.

2. 改写套接字的管理机制,每个socket都有一个缓冲区,应用层和IOCP分别负责一个方向的操作,然后模拟出"可读""可写"等 select 状态.
这样可以把SSL,Linux平台以及Windows平台的各种网络模型统一起来,缺点是需要多一倍的缓冲区.

3. 回调函数这样的恶心东西应该消灭.

==
2013-8-26

1. Pipe 而不是 BufferedPipe
2. BufferPipe + FilePipe 正好就是 FIFOCache

== 
2013-8-27

1. Pipe 双向链接,读是抽,写是推.
2. FCGIResponderFactory / DefaultResponderFactory 的概念
3. 把 Pipe 的read write变为内部函数,当_prev存在时关闭外部 write 调用,当_next存在时关闭外部 read 调用.这样做的目的是确保数据流的有序性.否则很难理解.

==
2013-8-28

1. "可执行单元" 的概念 IExecutableUnit.
   Q++ HTTP Server 就是有 IHTTPServer 管理若干个 "可执行单元" 每个 unit 都可以分段占用运行时间,就是操作系统中的进程一样被调度.
   可执行单元包括 侦听, HTTP请求,HTTP响应等,每个 unit 独立完成特定的功能互不干扰.

2. ExecutableUnitScheduler(调度器) 调度各个单元,把处于 可读/写 状态的 unit 选出,并调用 IExecutableUnit::exec()
   事先需要把 unit 注册到 调度器中 token = socket / 文件 的句柄.

==
2013-8-29

1. IOCPNetwork 超时可以用 PostQueuedCompletionStatus 发送一个特定的包.
总之应该尽量用 PostQueuedCompletionStatus 使程序执行流都从工作线程这个出口出来.

2. ExecUnit 保管 socket/handle 作为 token

==
2013-8-30

1. IOCPNetwork 包办一切,在 add() 函数中为套接字/句柄分配两个缓冲(可以延迟到实际读写时在真正分配).
2. 外部函数读写时,只读写 IOCPNetwork 分配的缓冲区,就好像普通的异步套接字那样,直接返回. IOCPNetwork 内部不停的根据 IOCP 模型的返回结果清空内部缓冲区.
3. HTTPServerImp 充当调用器,开多个线程调用 IOCPNetwork::select() 然后通过 iocp_key_t 找到对应的 IExecUnit,再调用 exec()完成整个流程.内部使用管道系统.这就是V0.3的主体结构.
4. IExecUnit保管 iocp_key_t,并负责回话超时处理.

2013-9-1

1. IOCPNetwork 创建工作线程,不断清空写缓冲和刷新读缓冲. 内部用两个队列,分别保存可读/可写的套接字.
2. 工作线程和select之间用线程同步工具来通信.
3. 如果注册了监听可读事件,那么IOCPnetwork应该发起一个 WSARecv 或者 AcceptEx 操作 (getsockopt SO_ACCEPTCONN).
4. ConnectEx 调用完成后,监听可写事件. SO_CONNECT_TIME 可以判断套接字是否连接.
3.4 应该在 add 中处理,即第一步应该由 IOCPNetwork 发起.

XXX Connect 和 accept 的第一步由应用调用.
对于应用来说调用 Connect 一次,马上返回 IO_PENDING,然后监听可写事件,以判断是否连接完成.
调用 accept 一次返回 IO_PENDING,然后监听可读事件,以判断是否新连接是否完成.
这个逻辑还是比较自然的,可以接受.
当然 add 依然要判断 1. listening 的套接字 2. un connect 套接字 3. connected 套接字
只有 3 才直接进入可写队列.

add() 注册.
modify()修改监听事件.

===
2013-9-10

V0.3 设计总结

目标功能: 支持SSL,支持GZIP, deflate 压缩流

1. 宏观方面, HTTPServerImp 管理若干个可执行单元(IExecUnit),并且在适当的时候切换执行之,这是整个程序的运作方式.
(1) IExecUnit 包括 HTTP请求,HTTP响应,SSL握手,侦听套接字,等凡是可以分步执行的状态机.
(2) 消除了回调函数方式, HTTPserver 是主动的调用各执行单元,并检查它们的状态.

2. 微观细节
(1) 管道系统,向后读,向前写简化数据流.可以添加各种过滤器实现类似GZIP压缩之类的功能.
(2) Responder factory的思想. HTTPserver本身实现了 Default responder factory 用来生成默认IHTTPResponder.

3. 网络模块.
为配合可执行单元的设计把IOCP包装为类似select的模型,这样也就从结构上为使用其他的网络模型(如windows select, event select, linux poll等)做好了准备.
(1) 每个socket分配2个缓冲区.
(2) 后台有线程不停的填写/清空缓冲区,然后使用事件通知.

==
2013-9-13

1. 消灭回调,不再提供 IHTTPServerStatusHandler 接口,而是提供一个状态查询接口,有UI界面主动查询. 是否可行,是否及时?
2. 重构的问题,面对如此多的修改,怎么下手? 先宏观再微观,先把大结构建起来?

==
2013-9-19

1. 动手,从大结构开始.
2. 问题: HTTPListener 接受一个新连接后,如何添加到 HTTPServerImp 中?
 2.1 回调的方式 onNewConnection()
 2.2 addConnection()
 2.3 根据 exec 的返回值分类处理,这样做的话似乎违反了 OO 的原则,封装成 EXECUNIT 调用 exec 然后又需要转变为具体的子类以调用某些方法.
 根据"在OO模型中,凡是觉得别扭的地方,抽象的模型肯定有问题"的原则,需要重新构思.
 造成这个问题的原因是 HTTPServerImp 和 HTTPListener 在两层(前者包含于后者)所以让低层反过来调用父层感觉就很别扭.如果把两者放在同一层,上面再加一层统一管理它们会好点吗?

 ==
 2013-9-23

 1. HTTPServerImp 扮演的角色
 1) 提供IHTTPServer的实现给各个对象(IRequest, IResponder 等)调用
 2) 实现 Core Driver 角色,调用网络模块驱动个可执行对象运行.
 3) 作为外部调用的主体对象.

 2. 完全的OOP,各个IExecUnit对象独立运行,通过调用 IServer 的方法实现逻辑.这和回调的方式在本质上是不一样的思想.

==
2014-3-6
1. IOCPNetwork 改成异步套接字行为模式,内部缓冲区,后台填充/清空缓冲区.添加 select 函数.
不需要 ioadapter接口.

2014-3-8
1. Pipe系统添加 pump()接口用来抽取数据到内部缓冲区.
2. 用shutdown来使iocp返回之后再closesocket,这样比较好.

2014-3-10
1. IOAdapter 封装套接字,如果某次 send IOCP返回只发送成功了一部分,怎么处理? 后台继续发送还是设置一个可写状态?
后台有线程刷新缓冲区..
send(buf == NULL)表示继续发送.

IOCPSelector 的目标是像epoll一样

2014-3-11
1. IOCPSelector 创建一个/多个线程刷新所有IOAdapter的缓冲区,管理一个活跃对象队列.(事件,同步对象等等的开销是少不了的)
一共有3种方法.
(1) 外部调用 select 从系统IOCP对象中获取一个结果包. (主要问题是IO操作只是部分完成的情况,因为由外部调用select所以网络模块不能及时刷新,以send为例,假设发送1KB的数据而IOCP完成了其中512KB,那么剩余的512KB什么时候发送?由谁来调用这个send函数? 当然可以提供接口(比如IOAdapter中提供接口查询内部缓冲区是否还有数据,提供一个 send(NULL, 0) 的特殊约定用来发送缓冲区中剩余的数据)来处理,不过不够优雅. 我认为凡是不够优雅的设计就是有缺陷的设计,那么缺陷在哪里?
(2) 和(1)类似,只是在select中如果发现部分完成则自动触发下一个IO操作然后再递归调用select 确保一个IO操作完整的完成才从select中返回.(还是不能从根本上解决,虽然实际应用中外部调用一般能迅速调用select,不过从逻辑上来说不够完美.
(3) 用一个后台线程刷新缓冲区,外部调用只从活跃队列中取结果.(逻辑上可以使网络模块独立,不过有很多同步开销,队列维护开销).

如原来用的"回调函数"的设计,这是使用IOCP模型最自然而然的想法,不过回调函数这种设计导致程序被动运行,一向不欣赏这种设计.

2014-3-12
采用(3)来设计IOCPSelector (后台线程刷新buf).
1. 用信号量表示活跃IO对象队列的个数.
2. IOAdapter 通过 lockbuf 和 unlockbuf 接口提供内部缓冲区的访问,这样 SocketPipe 就不用再分配缓冲区了.

2014-3-14
1. IOCPAdapter 添加 ctl 函数实现和 epoll_ctl类似的功能,这样就可以解决 recv/send 何时开始何时结束的问题.
2. recv 函数返回 SOCKET_ERROR(-1) lasterror = EAGAIN 时发起下一次 WSARecv IO请求.
3. send 函数总是在后台自动发送已经进入缓冲区内的数据,每次WSASend IO完成都进入活跃队列/或者当发送缓冲区变空时进入.

2014-3-15
1. recv 逻辑: ctl 函数添加可读事件时,自动发起一次 WSARecv IO,IO操作完成后刷新缓冲区,等待数据被读取,此时即使缓冲区未满也不发起下一次读IO,当缓冲区内的数据被读取完毕(即 recv 返回-1时),如果依然关注可读事件则自动发起下一次读IO.这样设计可以避免多余的一次读IO(如果一直自动发起读IO,假设来自对方的数据都已经接收完毕,那么最后的一次读IO无法完成或者超时).
2. send 逻辑: 后台用 WSASend 写IO清空缓冲区,独立的线程,只需做好同步就可以,只要 ctl 添加了可写事件,那么每一次写IO完成都进入活跃队列,如果没关注可写事件,后台线程清空缓冲区后暂停.
3. accept 逻辑: 总是关注可读事件,所以 listen 被调用后自动发起 AcceptEx IO,以及 accept被调用后也继续.

2014-3-16
1. recv/send 函数和 WinSock 一样,两个线程不能同时调用 recv.
2. 基于上一个原因,如果 send 函数后台线程清空缓冲区,每次IOCP返回都触发可写事件,那么就会出现上述情况,所以 send的逻辑改为提交数据后,投递IO操作,直到IOCP返回,如果数据一次没有发送完毕,则继续投递IO直到数据全部发送完毕或者发生错误,然后再触发可写事件.这样就不需要做同步了.

2014-3-17
1. IOCP模块epoll式封装基本完成(超时和速度限制还没添加),设计定型. recv/send都是一次IO完成后触发可读/可写事件; recv只有在数据都被外层调用处理完才会触发下一个IO;send只有在提交的数据在后台全部(不是部分,所以可能要投递多个IO)发送完毕才触发可写事件;一个套接字最多同时可以进行一个 RECV IO 和一个 SEND IO;同时只能有一个线程调用 recv/send 函数(从设计的角度上说也没必要多个线程对同一个套接字同时调用recv/send).
2. 下一步检查TimerQueue代码,是否添加事件式设计待定(目前是回调式设计)也可以考虑像 IOCPSelector那样用信号量/队列来实现,整个程序统一风格,消灭回调.
根据 v0.2版的使用情况,估计TimerQueue有bug(很可能是某个同步没处理好)

2014-3-18
1. TimerQueue 不需要两个队列,直接在同步保护下操作定时器队列即可.操作队列是多余的. 可行吗?还要再脑过一遍.

2014-3-19
1. 确定 FCGIResponder 的程序执行流有BUG(可以想象,那么跳跃,同步没处理好,导致FCGI数据流和HTTP数据流都终止了,没有网络操作就不会有超时或者失败,处于僵死状态,所以日志表现为这个请求一直没有处理完,也不会超时),缓存所有的FCGI输出可以规避这个问题. 既然V0.3版要重写这个部分,暂时先不去找V0.2的BUG了.
2. 状态回调接口不是个好设计,HTTPServerImp提供一个状态查询接口代替,有UI层每隔一段时间主动查询当前的连接数,带宽等信息.

2014-3-21
1. 调用ACCEPTEX时,提交一个很小长度的接收缓冲,这样新accepted的套接字就处于可读状态,解决了何时由谁来调用第一次recv的问题,同时恶意连接也可以避免.
3. 网络模块的要求再降低: 一次只能有一个IO在操作,这样就不需要做同步了,实际上达到这个要求就足够了,看不到有同时多个IO设计的必要.
4. 速度限制的功能放在 IOCPSocket 合适吗? 是不是应该放在 HTTP连接 这个层面呢? HTTPServerImp 中 IExecUnit run() 返回sleep,这样来控制速度.

2014-3-22
1. IOCPAdapter要互斥锁保护所有操作,才能实现安全关闭,同时做错误检测也就实现了.(不允许同时有多个 send/recv 等)
2. 怎样安全的关闭才是个难题
要站在 IOselector的角度来考虑这个问题: IOAdapter空闲状态下才能删除,删除之后就不能再访问.要做到这点要么用锁,同步控制保存状态一致性;要么永远单线程访问
_onIoCompleted 先做完IOCPSelector要做的事,再设置 IOAdapter为空闲状态(或者继续发送状态),这样在 close 函数中得到状态就是一致的了.(原来的问题在于 close 得到 Adpater状态为空闲,删除,但是_onIoCompleted又访问了被删除的Adapter)
IOCPAdater添加一个函数 onLastIOComplete 专门用于处理上一次IO的操作结果(不用加锁,关键,道理上来说 onLastIOComplete 还是上次IO的延续,不修改oppType,所以不用加锁),另外一个函数 clean 用来清理状态,需要加锁检测autodelete

2014-3-24
IOCP模块的同步问题

1. IOCPAdapter的同步,维护内部状态的统一, oppType 字段体现. close() - recv/send/connect/accept等所有需要访问sock句柄的函数 - onIOCompleted() 3个线程需要同步

2. IOCPSelector的同步
2.1 活跃队列需要同步,这个好办,用信号量和lock可以解决.
2.2 close() 时,删除 IOCPAdapter指针和 _onIOCompleted 刷新IOCPAdapter缓存之间需要同步.

关于同步,2个问题需要考虑,是否需要,怎么做.
要解决是否需要做同步的问题,先明确一下需求和设计原则:
(1) 对于一个 IOAdapter,允许同时最多有一个 recv类IO和一个 send类IO(可以同时投递一个recv IO 加 一个 send IO,但是不能同时投递两个 recv IO),这个要求可以降低到任何时候最多只有一个 IO
(2) 外部调用通过 epoll式的程序结构得到活跃的IOAdapter.
(3) 有超时控制
(4) 可以在IO操作没有完成的时候强制关闭IOAdapter(对于外部调用者来说,close()之后就算完了, IOCPSelector在后台要做必要的收尾工作.)

2014-3-25
1. 如果遵守调用规则(调用者一个线程调用 recv 或者 send 直到返回 -1,然后在下次可读/可写事件通知后再继续调用) IOCPAdapter 不需要做同步.  (recv 和 send 的接口要重新设计)
2. IOCPSelector 用一个全局锁同步所有操作.

如果需要支持同时RECV 和 SEND 那么应该用锁保护,如果只支持只有一个RECV 或者 SEND IO,那么可以不用锁 IO_DOUBLE = false

olp结构添加errcode字段用来保存各自的错误码,getlasterror根据两个值生成一个最终值,这样就基本不用同步就可以实现 recv / send 同时进行. 超时关闭时另一个操作可能会两个线程同时访问一个套接字句柄一个关闭,另一个发送或者接收,这样行吗?还是加锁吧.

2014-3-28
1. IOAdapter实现 IPipe接口以访问内部缓冲区.

2014-3-29
IOCP到底如何封装还要斟酌.假设IOCP模块不进行超时控制(超时控制由IOExecUnit模块控制,exec返回超时时间,HTTPServerImp做个定时器队列,到了超时时间调用exec运行,和速度限制类似)
那么IOCP的recv io 就可以始终进行, send io也一直在后台已另一个线程运行,只有缓冲区被读空或者写满之后,才触发可读/可写事件.
超时这个东西本来就应该在网络模块之外? 所以微软才没有给IOCP模型添加超时控制?接收数据时,可能是对方没有发送,也可能是网络慢,作为网络模块,无法判断怎样才算超时,只有上一层才知道.发送超时还好理解一点.
总之,要么网络模块完全不管超时,要么网络模块能分辨接收io是自动发起,则不超时,调用发起,则设置超时.

2014-3-31
1. IOCP模块采用完全线程同步,后台线程不停的刷新/清空缓冲区. IOCPAdpater内部做同步. IOCPSelector采用环形队列不加锁.
   IOCP模块不再管超时和速度限制
2. V0.3主要工作量在于管道系统和IOSelector,只要这两个模块完成,其他就简单了.
3. 由于使用了缓冲,在调用send成功后马上关闭套接字的问题就需要考虑: 还是按照标准的 shutdown 的用法
3.1 数据发送完毕后,调用 shutdown 关闭发送端,注册 EPOLL_EVENT_ERROR 事件,并等待.
3.2 后台用IOCP的方式自动清空发送缓冲内的数据.
3.3 数据全部发送完毕后,触发一个 EVENT_ERROR 事件,这样用户就可以知道什么时候可以调用关闭了.

2014-4-1
1. IOAdapter的关闭依然是个问题,怎样才能优雅又高效呢?(不要锁,不要等,没有内存泄露)) 看来要想个妙招.锁的意义是使无序变有序. 所以如果能设计程序流确定顺序执行,就可以去锁.
1.1 让close 和 onIoCompleted 执行顺序化.
1.2 每个工作线程都有一个队列可行吗?
1.3 IOCPAdapter做同步是不可避免的.

2014.4.2
1. 使用2个IOCP句柄,一个用于完成实际的网络操作,另一个用于通知上一层的等待函数(这样就不需要一个公用的队列,提高并发量,虽然IOCP句柄的并发性也存疑).
2. 既然IOCPAdapter的锁不可避免,那就暴露出来提供 IOCPAdapter::lock() 和 IOCPAdapter::unlock() 用各自的锁来同步删除动作,这样是不是能解决问题呢?
3. 关于线程安全IOCPSelector只能保证从close 返回后,不会再有IOAdapter从epoll函数中返回.
4. 调用者应该在接收到可读/可写事件之后,先调用 ctl 函数,再调用 recv/send 函数,如果想要精确控制事件通知的话,这个顺序很重要.

2014.4.3
 * 关于锁:
 * 1. IOCPAdapter 的 send/recv 函数需要占用自身(局部锁)和后台工作线程同步.
 * 2. 后台工作线程在某个IO操作完成后需要占用局部锁并进入广播端
 * 3. 从广播端获得IO事件通知需要占用局部锁.
 * 4. 删除时需要占用全局锁.
 * 
 * 局部锁使用太频繁了影响性能(但是对全局的并发性影响并不大),删除时占用全局锁应该没事,比较删除操作是少数.
 * 下一步优化目标就是怎样减少局部锁的使用.

 1. 如果onIoCompleted 中不调用 doRecv/doSend 就可以保证一次一个IO,不需要同步锁,但是效率更低?

 2014.4.5
 1. BufferPipe 的pump函数怎样设计才能更有效的利用内存?

 2014.4.7
 1. ctl 的功能放在 IOCPAdapter 中,逻辑判断也在其中. 否则会导致重进入的问题,
 2. IOCPSelector wait 函数返回前会自动清空 ctl注册的事件(和 epoll_wait 一样),事件处理完毕后应该再次调用 ctl MOD 把事件重新注册一下(要做好同步,即什么时候应该通知,什么时候不应该通知 event trigger)
 可以在ctl 函数中判读,如果调用ctl函数时已经有数据在缓冲区内,则触发事件(这样就合 LT 模式一样了)
 根据IOExecUnit的返回值判断是否需要再次调用 IOSelector 的 ctl 函数, IOSelector要手动生成一个事件就方便了.
 3. 先搞清楚 Linux下epoll的编程(尤其是多线程编程)细节再说.
 3.1 LT模式: 只要调用 epoll_wait 的时候,某个套接字内有数据/或者缓冲有空间就可以返回(不管数据是不是新收到的)
 3.2 ET模式: 调用epoll_wait时,没有新的数据到达/缓冲区被清空一部分,那么就不通知. 问题:如果有新的数据到达,但是缓冲区没满,通知不通知呢?
 3.3 多个线程调用 epoll_wait时,如果已经返回了一个套接字,又有事件被触发,不是会造成重入吗? 还是必须要用 IO_EVENT_EPOLLONESHOT 来控制?

 原来关于ET模式的理解应该使正确的,只有缓冲区满/空的状态变化时才触发,只是应该进一步在 ctl 函数中判读是不是应该触发, 否则 ctl 函数和read 函数如何配合呢,有丢失的可能.
 比如: 先调用 read 知道返回 -1 no = EAGAIN,表示正在后台接收数据,然后再调用 ctl 注册可读,但是如果后台在 read 和 ctl 函数中间时刻完成了读,此时套接字并没有注册事件(ctl还没执行),然后ctl执行注册了可读事件,那就阻塞了.

 4. 用LT模式+ONSHOT应该可以解决问题.效率也不会差多少.
 不管哪种模式,调用 ctl 函数时如果套接字已经处于可读/可写状态就应该触发事件(手动触发).
 对于多线程调用 epoll_wait,必须要添加 ONSHOT ,如果ctl 函数按照上述设计,那么ET模式和LT模式没有区别.

 2014.4.9
1. 添加 HTTPConnection类.
2. exec 的返回值设计 struct execres_t

2014.4.10
1. IO_ERROR_PENDING 是不是用 IO_ERROR_SUCESS 代替
2. shutdown 还是要再考虑细节.
3. 实现管道化连接的处理.

2014.4.12
1. HTTPRequest 作为IOExecUnit执行完毕后怎么和HTTPServerImp通信?
目前是用 onRequest 回调的方式通信,总觉得不舒服.根据我的原则,凡是觉得别扭的地方,肯定是有些问题没有考虑清楚,抽象的模型有问题.
想法1: 像COM一样采用 QueryInterface 这样的方法从IOExecUnit中的到对应的接口. 要求 exec 函数用返回值表明. 似乎也不是很好,只是一个取巧的方法罢了.

2014.4.13
到处都用到状态机,设计一个漂亮的基类很有必要.

2014.4.14
还是那个问题,选择活跃的 IOAdapter 之后怎么处理
1. 根据 IOAdapter map 得到 IOStateMachine 然后运行之. 问题是添加修改删除,尤其是删除是个问题,不够优雅(在成员函数内删除自己?)并且很容易出错.

2014.4.15
HTTPServerImp为每个注册的IOStateMachine 分配一个结构,包含了一些信息包括调度完成时调用的函数指针,这样IOStateMachine就不再需要关心回调问题.

2014.4.16
抽象状态机和状态机调度器. 调度器负责在适当的时机调度IO状态机运行,并且根据运行结果调用预先注册的处理函数.
这样,程序的其他组件也可以通过调度器接口得以运行自己的状态机,比如FCGIFactory.

2014.4.18
网络模块还是不好(网络模块是核心也是难题,搞好了,整个程序的结构,效率都上去,搞不好,其他部分再怎么做都是事倍功半).
目标: adpater 和 selector 互相分离,说到底就是用 Windows 的 IOCP 尽量模拟 Linux 的 epoll.

1. adapter 只是一个操作接口,维护 SOCKET 句柄,独立完成所有不需要缓冲区的操作,如setsockopt,bind,listen 等.
2. selector 维护 iocp结构,包括两个 olp 和 各自的缓冲区已经一些其他控制用变量.
3. adapter 没有加入到 selector 时,只能完成不需要缓冲区的操作(完整的设计应该让adapter 没有加入到selector前实现阻塞套接字或者异步套接字的功能,就像 socket 没有用 select / epoll 前一样可以完成这些功能一样,对于服务器来说没什么意义,以后再完善).
4. selector封装的是网络模型的细节,所以各种网络模型有各自的selector实现,比如iocpselector,epollselector,sselector,eventselector等,现在只讨论 iocpselector.
5. adapter 加入到 selector 之后,iocpselector为每个adapter分配一个 iocp_context 包括缓冲区等, adapter 也拿到这个指针, 所有需要缓冲区支持的功能用户通过 adapter的接口操作 iocp_context 实现.
6. iocpselector在后台也同步操作这个 iocp_context 指针,刷新缓冲区,并触发各种事件.
7. 关闭套接字时, 用户首先应该调用 ctl 函数从 selector中删除(这是理所当然的),此时分配给adapter的 iocp_context 指针被收回(不是删除),用户就可以直接删除 adapter实例,实现了selector 和 adapter的分离. iocpselector可能还需要在后台做一些清理工作,完成后再删除 iocp_context 指针.
8. 实现时 adapter 和 selector 是成对实现的,如 iocpadapter 和 iocpselector, 因为只有 iocpadapter 才知道如何操作 iocpselector 提供的 iocp_context 缓冲.

问题
1. Linux epoll 允许先执行 io 再添加到 epoll 中检测状态. 怎么实现?
2. AcceptEx 完成后收到的第一段数据怎么处理?
3. doRecv / doSend 什么时候调用?

0. 允许先执行 io 再检测状态,只有先分配 iocp_context,然后调用 ctl 添加到 selector 时,把指针委托给selector管理,一旦委托就不能再收回或者关闭时检测没有IO操作正在进行,如果没有就可会取消委托,收回指针.不过对于Windows IOCP模型这样做意义不大,没有添加实际上也不能真正io,还增加了复杂性. ctl ADD 时分配 ctx 指针, ctl DEL 时收回指针(由ioselector删除或者等IO完成后删除) 所有 ctx 指针由 selector 统一管理,这是原则.
1. 连接被建立后,用户使用adapter通过selector来判断是否可读可写,无所谓第一段数据的问题.
2. 全局工具函数 iocp_recv / iocp_send / iocp_accept / iocp_connect 操作 iocp_context 结构来发起IO. 总之 adapter 不需要知道 selector 的存在;selector只关心通过 ctl 添加进来的 adapter.
3. adapter 和 selector 都有可能调用这些全局工具函数(逻辑上来说,iocp_context 被分配给 adapter之后, adapter 当然也要操作它)
4. selector 维护一个 iocp_context 列表(可能是正在使用的,也可能是已经被标记为删除的),这样销毁selector时,可以解除所有 adapter 对 iocp_context 的引用,从而解耦 adapter 和 selector.

2014.4.20
1. 似乎由 IOCPAdapter 来分配 IOCPContext 比较合理? 加入某个 selector 就把 ctx 委托给那个 selector 管理, 退出时收回.

2014.4.22
IOCPContext 的设计有问题. Adapter 和 Selector 完全分离的话,程序逻辑无法保证删除 Adapter 之前一定会调用 ctl DEL, 另外 Adapter 和 Context 共享一个 socket handle 无法维持数据的一致性,可能导致多个线程同时访问.

1. 添加 wakeup 函数
2. selector分配adapter时全部进队列而不是delete进.
3. clone 函数复制侦听用的新套接字

2014.4.23
Accept 中由谁,何时创建 AcceptEx 用的新套接字句柄是个问题. 就是因为这个问题导致 IOCPAdapter必须包含 Selector的指针,这是很恶心的. 想个招.
看来还是得用 IOCPContext. 用户调用了 ctl ADD 就有义务调用 ctl DEL 这是可以合乎逻辑的; Adapter 在分离时, 使 IOCPContext 保存的 socket = INVALID_SOCKET.

2014.4.30
1. 直接在HTTPConnection中写日志? 这样就不用统计信息了?
2. 让 网络IOSelector 统计网络模块的数据(发送/接收速率等)

2014.5.2
1. 开始修改FCGI模块.
1.1 FCGIRecord 是一个Pipe,数据流流入,FCGIRecord块流出.
1.2 FCGICache 没必要了,直接用 BufferPipe + TmpFilePipe 代替. 

2. FCGI 数据流.
2.1 Server -> Client: FCGI IOAdapter (NamedPipe) -> FCGIRecordPipe -> FilePipe -> BufferPipe -> HTTPResponderHeader -> HTTP Client IOAdapter
做好同步,双方向驱动 FCGi IOAdapter write, HTTP Client IOAdapter pump, 由 FilePipe 和 BufferPipe 提供缓存功能.

2014.5.4
FCGIFactory 是一个大状态机,管理所有和FCGI服务器的连接(本地或者远程都抽象为IOAdapter).
有FCGIResponder时,调用调度器的reinstall把连接安装到Responder,Responder析构时再reinstall回factory.

2014.5.5
1. FCGIFactory是个代理管理fcgi连接的状态机
2. 本地模式的连接是不是可以直接用子进程的stdin句柄读写数据? 或者复制一下然后传递给子进程,还是一定要重新createfile?可以试试
3. iocpadapter构造函数直接接受句柄,另外,构造函数中的错误或者异常怎么处理? 查查C++对象模型那本书. 重载 operator !()

- 结论
1. ok
2. 只能按照0.2版的方式,创建后继承给子进程,然后wait, createfile. 直接用这里句柄逻辑上讲不通,和linuxpipe不一样.
3. 构造函数错误只能通过异常或者单独提供一个成员函数来判断状态,或者构造函数什么都不做,另外提供一个init函数来初始化(如果有init一定也要提供release才舒服,强迫症).

2014.5.6
1. FCGIFactory按前面的设计应该没有大的问题.
2. FCGIResponder 如何协调两个 IOAdapter是个难题.
出现一个IOAdp 等待另一个IOAdp时 STM_PAUSE可以让一个adp暂停,问题是何时由谁重启. 如果不等待形成空转,肯定是不行的.
新增STM_PAUSE并设置一个处理函数

2014.5.7
STM_PAUSE 必须实现,调度器实现之,网络模块不管这个.
由同一个调度器的另一个状态机触发重启,不过这样一来,状态机之间有耦合,如果是多个ioadp对应一个状态机的话,逻辑上比较好一些:pause一个状态机的某个adp,然后由同状态机的另一个adp resume.

STM_ADDADP
STM_DELADP
STM_PAUSE
STM_RESUME

fcgifactory 不把fcgi adp 安装到 fcgiresponder,仅仅是 fcgiresponder保存有这个adp的指针. 否则会出现 httpadp, fcgiadp 同时安装同一个 fcgiresponder的问题.
总之,一个连接不管有多少个adp,只安装一个状态机->httpconnection stm.

2014.5.8
1. 采用状态机运行中添加adp的方式总感觉有些取巧.正常来说应该由调度器来添加删除某个状态机的adp,但是调度器不是状态机能拿到的.
2. STM_ADDADP 的参数 stmadpdesc_t 包括新adp,epollev,当前epollev.
3. 调度器中的状态机ctx干脆不要保存adp的信息,没有意义. 通过 adp 的userdata 可以得到所有信息,反方向查找没有需求.

2014.5.11
1. FCGIFactory 的等待队列怎么处理? 新请求进来,但是所有的FCGI连接都忙碌,此时怎么处理?

2014.5.12
1. FCGIFactory catcheRequest 中,如果没有空闲的连接,则把 responder放入等待队列,然后新建一个连接,连接准备就绪后,安装到等待队列中的responder.
responder首先等待可写事件确认连接可用. responder的ondone,onabort函数由 FCGiFactory处理.

2014.5.16
1. FCGIFactory dispatch 的思路还不错,所有请求都进等待队列,然后再统一分派. 问题是如果FCGI连接失败怎么处理?
1.1 自动重新建连接,则可能导致失败循环(由于配置原因无法建立连接的话无论循环多少次都不会成功)
1.2 如果不自动重新连接,则进入等待队列的FCGIResponder会僵死.
1.2.1 FCGIResponder 设置超时.
1.2.2 FCGIFactory 建立连接失败的话,在最后一个连接失败时,清空等待队列,使所有等待中的FCGIResponder全部以获取连接失败返回.

2014.5.18
1. FCGIRecordReader / Writer 直接写 Buffer 还是 Pipe?
2. FCGIRecordFilter 还是 Pipe? 
2.1发送到FCGI服务器: FCGIRecordFilter -> BufferPipe -> FCGI Adapter
2.2从FCGI服务器接收: FCGI Adapter -> FCGIRecordFilter -> FCGIResponder
2.3发送到HTTP客户端 FCGIResponder -> FilePipe -> BufferPipe -> ChunkFilter -> HTTPResponderHeader -> HTTP Adapter

2.2 和 2.3 必须分开,不能全部连在一起. 因为从FCGI服务器发送过来的数据包含日志,错误信息等,不一定都要发送到HTTP客户端.
虽然硬要编码过滤这些信息让所有管道连在一起也是可以的,不过...别扭,就不要这样做.
2.2 和 2.3 数据通道之间由 FCGIResponder来协调.

FCGIFilter 需要缓存至少一个 FCGIRecord 才能实现. FCGIFilter结合了原来 FCGIRecordWriter 和 FCGIRecordReader 的功能.

2014.5.19
1. 关于 FCGIRecordFilter / FCGIRecordPipe 的设计
1.1 以 FCGIRecordPipe(带缓存) 先pump到内部缓冲区,在输出为FCGIRecord流; 写入时也是先写入到内部缓冲区. 这样好像有部分功能和 BufferPipe 重合了,不完美.
1.2 Pipe 增加 peek 和 putback 以 FCGIRecordFilter (无缓存) 实现,工作量更大,却更合理. (未来各种无缓冲Filter因此受益,如ChunkFilter)

2. 更新数据流
2.1发送到FCGI服务器: FCGIRecordFilter -> BufferPipe -> FCGI Adapter
2.2从FCGI服务器接收: FCGI Adapter -> BufferPipe -> FCGIRecordFilter -> FCGIResponder
2.3发送到HTTP客户端 FCGIResponder -> FilePipe -> BufferPipe -> ChunkFilter -> HTTPResponderHeader -> HTTP Adapter

2014.5.22
1. ipipe 的 read / write 逻辑应该使先循环调用 _read / _write 到返回0为止然后在调用 prev() / next() 的 read() / write
2. Uri 类定义宏 URI_TEXT 代替 _T
3. FcgiInputFilter专门用于输入post(其他地方直接用 FCGIRecord 类配合 BufferPipe). 派生至 BufferPipe (或者内部包含也可以)
逻辑: 内部bp有数据则读取返回;否则根据 prev size 写入header 到内部bp, 从 prev 读取数据到 bp,再跳回第一步.内部设置一个变量判断是否已经写入过结束标志以返回0结束输入.
是否 Filter 不以 有无 buffer 为准,主要看数据流向,以语义为要.

4. 要实现真正意义的管道系统, peek, size 无法实现,也不合适. 尤其是有过滤器存在,数据是即时生成的,无法计算总的长度,也不能"peek"再恢复.
skip 可以实现,作为 read 的特例, 读取后再丢弃. 而且数据流应该从链中的没一个ipipe实例进过,才能让其中的过滤器起效.

2014.5.23
1. pipe transformer 从 bp 派生,必须且只缓存一次变换的结果.
read 逻辑: 内部缓冲区读取; prev 读取预定的最大长度, 变换, 缓存结果, goto begin
write 逻辑: 缓存读取,写入next, 变换预定的最大长度, 缓存结果, 写入缓存, goto begin

2. 才发现管道系统的 write 逻辑居然弄错了.
管道链的数据流是单方向的 总是从 prev -> current -> next.
可以在 next 端调用 pump 抽取数据, 也可以在 prev 端 调用 push 推送数据. 两种方法的数据流方向是一致的.

2014.5.25
1. 还是 _read / _write 纯虚函数,这样比较好理解. _pump / _push 反过来的,不好理解.
2. 和 _size 对应, 添加一个 _space 函数用来确定内部缓冲区还可以写多少数据. 既然无法 _putback 就要确保从上一个管道读出的数据可以完全写入目标管道,否则数据就会丢失.

3. 思路: 反过来操作, 要从一个 pipe 中读取时,提供缓冲区,让那个pipe主动写; 要写时,同样,让目标缓冲区主动读. 这样就不会出现数据从一个管道中读出,但是写入目标管道时失败导致的数据丢失.
并且目标管道主动读写可以根据目标管道各自的特点优化,效率更高.
4. 对用户只提供 pump(IPipe* src, maxlen) 和 push(dest maxlen), 不再提供直接 read/write buffer(通过BufferPipe来完成同样的功能).

2014.6.5
1. 按照软件设计的原则, IPipe 派生类只要提供 _read _write _size _space 最好. 只是多几次内存拷贝.

2014.6.11
1. 如果还是由 FCGIResponder 处理一切,那么只能写出 V0.2那样的恶心代码.
2. 编写一个单独的状态机 FCGIConnection ,负责 发送/接收 FCIG 服务器数据. 提供一个 IPipe 和 wait 函数有数据时可以通知(不一定要用同步对象实现,FCGIFactory 可以创建一个等待队列,某个 FCIGConnection 有数据时,直接唤醒它对应的 FCGIResponder)
3. HTTPResponder 改进一下可以接收一个 FCIGConnection 提供给的 IPipe 数据源. 这样 FCGIResponder 就没有存在的必要了.
4. 由于 FCGIConnection 是长期存活的,所以它所用的缓冲区/临时文件也长期存活,效率提高了.
5. 要有机制避免临时文件一直增大的问题