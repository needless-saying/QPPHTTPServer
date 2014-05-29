#pragma once
#include <functional>
#include <list>
#include "IOSelector.h"

/*
* IOStateMachine 状态机基类
* 1. 最多可以有10个状态
* 2. stepx的返回值true表示继续执行,false表示等待下一次调度.
*
* by Q++ Studio 阙荣文 2014.4.14
*/

/* 
* 实现一:
* 1. 基类维护一个 std::function 队列
* 2. 派生类用 std::bind 把各个状态处理函数按顺序添加进队列,就可以实现任意多个 step 而不仅仅是 step0 ~ 9.
*

* 实现二:
* 预定义 step0 ~ 9 一共10个虚函数,派生类只要按需实现几个即可.
* 看起来很笨的办法,派生类编写却非常简便,尤其是在参数固定的条件下.
*/

/* 状态机 run 返回值 */
#define STM_ABORT -1		//终止
#define STM_CONTINUE 0		//继续执行 epoll
#define STM_STEPDONE 1		//一个step执行完毕
#define STM_DONE 2			//状态机执行完毕
#define STM_PAUSE 5			//暂停状态机(由同一个状态机的另一个IOAdapter负责启动,取决于具体设计
#define STM_RESUME 6		//继续运行
#define STM_SLEEP 10		//睡眠一段时间后唤醒继续调用exec

typedef union stm_result_param
{
	unsigned int sleepTime;
	unsigned int epollEvent;
	unsigned int errorCode;
	IOAdapter*	resumeAdp;
}stm_res_param_t;

typedef struct STM_RESULT
{
	int rc;						// result code STM_XXX
	int st;						// state index
	stm_res_param_t param;		// parameter
}stm_result_t;

/* 状态机基类 */
class IOStateMachine
{
private:
	int _state;

protected:
	virtual bool beforeStep(IOAdapter* adp, int ev, stm_result_t* res);
	virtual bool step0(IOAdapter* adp, int ev, stm_result_t* res);
	virtual bool step1(IOAdapter* adp, int ev, stm_result_t* res);
	virtual bool step2(IOAdapter* adp, int ev, stm_result_t* res);
	virtual bool step3(IOAdapter* adp, int ev, stm_result_t* res);
	virtual bool step4(IOAdapter* adp, int ev, stm_result_t* res);
	virtual bool step5(IOAdapter* adp, int ev, stm_result_t* res);
	virtual bool step6(IOAdapter* adp, int ev, stm_result_t* res);
	virtual bool step7(IOAdapter* adp, int ev, stm_result_t* res);
	virtual bool step8(IOAdapter* adp, int ev, stm_result_t* res);
	virtual bool step9(IOAdapter* adp, int ev, stm_result_t* res);
	virtual bool afterStep(IOAdapter* adp, int ev, stm_result_t* res);

	IOStateMachine();
public:
	virtual ~IOStateMachine();
	int current();
	int forward(size_t steps = 1);
	int backward(size_t steps = 1);
	void run(IOAdapter* adp, int ev, stm_result_t* res);
};

/* 状态机对象队列 */
typedef std::list<std::pair<IOAdapter*, IOStateMachine*> > iosm_list_t;

/*
* IO状态机调度器
* 
* TIP: 设计时候有个问题,既然状态机都是自定义的,为什么还需要3个处理函数呢? 直接在状态机派生类内实现这些功能不就可以了吗?调度器只需要调度状态机运行即可.
* 不这样做的原因主要是删除的问题.假设状态机派生类内实现 onDone, onDone 要做的事之一往往是删除这个状态机,就会出现自杀的问题,虽然可以实现,但是很容易出错(删除后就不能再访问类成员,this指针已经失效)
* 而如果使用调度器管理的处理函数指针,调度器可以保证指针的有效性.另外感觉上也更自然一些.
*/
typedef std::function<void (IOAdapter*, IOStateMachine*, stm_result_t*)> sm_handler_t;
class IOStateMachineScheduler
{
protected:
	virtual ~IOStateMachineScheduler() {};
public:
	// 安装状态机,运行完毕后自动卸载.
	virtual int install(IOAdapter* adp, u_int ev, IOStateMachine* sm, sm_handler_t onStepDone, sm_handler_t onDone, sm_handler_t onAbort) = 0;
	virtual int reinstall(IOAdapter* adp, u_int ev, IOStateMachine* sm, sm_handler_t onStepDone, sm_handler_t onDone, sm_handler_t onAbort) = 0;
	virtual int uninstall(IOAdapter* adp) = 0;

	// 开始/停止调度
	virtual int start(size_t threads) = 0;
	virtual int stop() = 0;
};

/* 工厂类 */
extern IOStateMachineScheduler* create_scheduler();
extern void destroy_scheduler(IOStateMachineScheduler* scheduler);
