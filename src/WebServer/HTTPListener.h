#pragma once

#include "HTTPLib.h"

/*
 * 侦听套接字执行单元
 * 通常有 80 和 443 两个侦听单元
 *
*/
class HTTPListener : public IOStateMachine
{
private:
	IOAdapter* _newAdp;
	bool step0(IOAdapter* adp, int ev, stm_result_t* res);

public:
	HTTPListener();
	virtual ~HTTPListener();
	IOAdapter* getAdapter();
};

