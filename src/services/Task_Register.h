#pragma once

#include "Task_OnLogin.h"
#include "Task_onStartAlgoInstance.h"
#include "Task_onUpdateAlgoInstance.h"
#include "Task_OnMarektDepthQuery.h"
#include "Task_OnSubscribeMarketDepth.h"
#include "Task_OnSubscribeStockConceptQuote.h"

static int regiester_tasks() {
	
	Task_OnLogin::createTask();

	Task_onStartAlgoInstance::createTask();

	Task_onUpdateAlgoInstance::createTask();

	Task_OnMarektDepthQuery::createTask();

	Task_OnSubscribeStockConceptQuote::createTask();
	
	Task_OnSubscribeMarketDepth::createTask();

	return 0;
}
