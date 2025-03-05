#pragma once

#include "Task.h"
#include "TCPSession.h"
#include "StockDataManager.h"

class Task_OnSubscribeMarketDepth :public Task {

	TASK_CONSTURCTOR_DECLARE(AlgoMsg::MsgAlgoCMD::CMD_SubscribeMarketDepthRequest
		, AlgoMsg::MsgAlgoCMD::CMD_SubscribeMarketDepthResponse
		, Task_OnSubscribeMarketDepth)

	void handleMessage(const std::shared_ptr<TCPSession>& session
		, AlgoMsg::MsgAlgoCMD cmd
		, std::unique_ptr<AlgoMsg::MessagePkg>&& recvPkgPtr) override 
	{
		auto req = std::make_shared<AlgoMsg::MsgMarketDepthSubcribeRequest>();

		google::protobuf::io::ArrayInputStream input(recvPkgPtr->body().data(), recvPkgPtr->body().length());
		auto parseOK = req->ParseFromBoundedZeroCopyStream(&input, recvPkgPtr->body().length());

		auto resp = std::make_shared<AlgoMsg::MsgMarketDepthSubcribeResponse>();
		resp->set_request_id(req->request_id());
		resp->set_error_code((int32_t)AlgoErrorCode::ALGO_OK);

		if (not parseOK) {
			printerOutPut.clear();
			printer.PrintToString(*recvPkgPtr, &printerOutPut);
			SPDLOG_ERROR("parse AlgoMsg::MsgMarketDepthSubcribeRequest failed,{}", printerOutPut);

			resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);
			resp->set_error_msg("parse AlgoMsg::MsgMarketDepthSubcribeRequest failed");
			TCPSessionManager::getInstance().sendResp2C(session, cmd_response, resp);
			return;
		}

		printerOutPut.clear();
		printer.PrintToString(*req, &printerOutPut);
		SPDLOG_INFO("recv SubscribeMarketDepthRequest:{}", printerOutPut);

		auto result = TCPSessionManager::getInstance().addMarketDepthSubscribe(session, req.get());
		if (not result.empty()) {
			resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);
			resp->set_error_msg(result);
		}
		TCPSessionManager::getInstance().sendResp2C(session, cmd_response, resp);
	}
};
