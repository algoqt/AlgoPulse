#pragma once

#include "Task_OnLogin.h"
#include "Task_onStartAlgoInstance.h"
#include "Task_onUpdateAlgoInstance.h"
#include "Task_OnMarektDepthQuery.h"
#include "Task_OnSubscribeMarketDepth.h"
#include "Task_OnSubscribeStockConceptQuote.h"

class Task_Register {

public:

	static int registerAlgoTasks() {

		Task_OnLogin::createTaskInstance();

		Task_onStartAlgoInstance::createTaskInstance();

		Task_onUpdateAlgoInstance::createTaskInstance();

		Task_OnMarektDepthQuery::createTaskInstance();

		Task_OnSubscribeStockConceptQuote::createTaskInstance();

		Task_OnSubscribeMarketDepth::createTaskInstance();

		return 0;
	}
};
