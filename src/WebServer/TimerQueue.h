#pragma once

/* Copyright (C) 2011 阙荣文
 *
 * 这是一个开源免费软件,您可以自由的修改和发布.
 * 禁止用作商业用途.
 *
 * 联系原作者: querw@sina.com 
*/

#include <list>
#include <vector>
#include "Lock.h"

/*
* 函数返回值常数
*/
const int TQ_SUCCESS = 0;
const int TQ_ERROR = 1;

/*
* 使用 QueryPerformanceFrequency() 实现的高精度计时器
*/
class HighResolutionTimer
{
private:
	__int64 _frequency;
public:
	HighResolutionTimer(bool high);
	~HighResolutionTimer();

	__int64 now(); /* 返回当前时刻 */
	__int64 getCounters(__int64 ms);
	__int64 getMs(__int64 counter);
};

/*
* 定时器队列
* 目的: 实现定时器队列.
* 
* 一个工作线程维护一个定时器队列,定时器队列是一个有序队列(对于所有有效定时器而言),最快超时的定时器总是在队头.
* 只有工作线程可以操作定时器队列.
*
* 另外有一个专门的操作队列.
* 添加/修改/删除操作时,在把操作类型放入操作队列中,然后唤醒工作线程.
* 工作线程一次处理完所有操作队列中排队的操作.
*
* 2014-3-19 - V0.3
* 1. 取消操作队列,直接修改定时器队列.
* 2. 修正删除最后一个定时器时正好该定时器超时的临界状态_curTimer指针失效BUG.
* 3. changTimer()函数现在可用.
* 4. 运用"定时器工作状态"的概念设计定时器队列,现在逻辑上应该更好理解一点.
*/

class TimerQueue
{
public:
	/*
	* 类型定义
	*/
	typedef void* timer_t;
	typedef void (*timer_func_t)(timer_t, void*);

protected:

	typedef struct timer_desc_t	
	{
		timer_func_t funcPtr; /* 回调函数地址 */
		void* param;
		HANDLE ev;	/* 回调函数完成后的通知事件 */
		__int64 expireCounters; /* 超时结束时间 */
		int st; /* 定时器状态: 0 - 未执行; 1 - 正在执行回调函数; 2 - 已完成;*/
		bool waitting; /* 是否有线程正在等待回调函数执行完毕 */
		bool autoDelete;
	};
	typedef std::list<timer_desc_t*> timer_list_t;
	
	/*
	* 自定义TimerQueue
	*/
	HANDLE _waitableTimer; /* 系统定时器对象 */
	uintptr_t _timerThread; /* 定时器工作线程 */
	int _state;	/* 定时器队列的工作状态[正常TIMER/重置定时器/退出] */

	timer_list_t* _timerList; /* 定时器对象队列 */
	timer_desc_t* _curTimer; 
	Lock* _lock; /* 同步锁 */
	HighResolutionTimer _hrt; /* 高精度计时器 */

	/* 
	* 定时器缓存池 
	*/
	size_t _poolSize;
	size_t _poolPos;
	timer_desc_t** _timerPool;

	timer_desc_t* allocTimer();
	int freeTimer(timer_desc_t* timerPtr);

	/*
	* 工作线程函数
	*/
	static unsigned int __stdcall workerProc(void* param);

	/* 
	* WaitableTimer 被激活时由工作线程调用 
	*/
	bool proc(HANDLE ce); 

	/*
	* 辅助函数,不加锁
	*/
	int setNextTimer(); /* 计算定时器触发的时间 */
	timer_desc_t* getFirstTimer();
	bool isValidTimer(timer_desc_t* timerPtr);
	bool inqueue(timer_desc_t* timerPtr); /* 如果需要重新设置定时器则返回true,否则返回false */
	void wakeup();	/* 唤醒工作线程 */
	void oppAcquire(timer_desc_t* timerPtr, int oppType);

public:
	TimerQueue(size_t poolSize = 128);
	~TimerQueue();

	/*
	* 初始化和销毁
	*/
	int init();
	int destroy();

	/*
	* 创建,修改,删除定时器
	*/
	timer_t createTimer(DWORD timeout, timer_func_t callbackFunc, void* param);
	int changeTimer(timer_t timerPtr, DWORD timeout);
	int deleteTimer(timer_t timerPtr, bool wait);
};