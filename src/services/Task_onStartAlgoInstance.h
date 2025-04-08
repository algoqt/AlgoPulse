#pragma once

#include "Task.h"
#include "TCPSession.h"
#include "StorageService.h"
#include "AlgoOrder.h"
#include "AlgoTrader.h"
#include "ShotTrader.h"
#include "AlgoService.h"
#include "Configs.h"

class Task_onStartAlgoInstance :public Task {

	TASK_CONSTURCTOR_DECLARE(AlgoMsg::MsgAlgoCMD::CMD_AlgoInstanceCreateRequest
		, AlgoMsg::MsgAlgoCMD::CMD_AlgoInstanceCreateResponse
		, Task_onStartAlgoInstance)

	void handleMessage(const std::shared_ptr<TCPSession>& session
		, AlgoMsg::MsgAlgoCMD cmd
		, std::unique_ptr<AlgoMsg::MessagePkg>&& recvPkgPtr) override 
	{

		auto req = std::make_shared<AlgoMsg::MsgAlgoInstanceCreateRequest>();

		google::protobuf::io::ArrayInputStream input(recvPkgPtr->body().data(), recvPkgPtr->body().length());

		auto parseOK = req->ParseFromBoundedZeroCopyStream(&input, recvPkgPtr->body().length());

		auto resp = std::make_shared<AlgoMsg::MsgAlgoInstanceCreateResponse>();

		if (not parseOK) {
			printerOutPut.clear();
			printer.PrintToString(*recvPkgPtr, &printerOutPut);
			SPDLOG_ERROR("parse AlgoMsg::MsgAlgoInstanceCreateRequest failed,{}", printerOutPut);

			resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);
			resp->set_error_msg("parse AlgoMsg::MsgAlgoInstanceCreateRequest failed");
			TCPSessionManager::getInstance().sendResp2C(session, cmd_response, resp);
			return;
		}

		printerOutPut.clear();
		printer.PrintToString(*req, &printerOutPut);
		SPDLOG_INFO("recv start request:{}", printerOutPut);

		resp->set_request_id(req->request_id());
		resp->set_acct_type(req->acct_type());
		resp->set_acct(req->acct());

		auto requestId = req->request_id();
		auto brokerId = agcommon::Configs::getConfigs().getBrokerIdFromAcct(req->acct_type(), req->acct());
		auto acctKey = AcctKey_t(req->acct_type(), req->acct(), brokerId);

		if (TCPSessionManager::getInstance().isLogin(acctKey) == false) {

			SPDLOG_ERROR("[{}][{}]acct not login,{}", requestId, req->algo_order().client_algo_order_id(), acctKey);

			resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ACCT_NOT_LOGIN);

			resp->set_error_msg(fmt::format("acct not login,{}", acctKey.acct));
		}
		else {
			AlgoErrorMessage_t algoErrMessage_all;

			auto algoOrders = createAlgoOrder(*req, algoErrMessage_all);

			resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);

			if (algoErrMessage_all.empty()) {

				for (auto& algoOrder : algoOrders) {

					SPDLOG_INFO("[{}]create algoOrder:{}", requestId, algoOrder->to_string());

					AlgoErrorMessage_t algoErrMessage;
					int32_t errorCode = (int32_t)AlgoErrorCode::ALGO_ERROR;

					errorCode = (int32_t)dispatchAlgoOrder(algoOrder, algoErrMessage);

					if (errorCode == (int32_t)AlgoErrorCode::ALGO_OK) {
						resp->set_error_code(errorCode);
						resp->add_success_algo_order_id(algoOrder->algoOrderId);
					}
					else {
						resp->add_failed_algo_order_id(algoOrder->algoOrderId);
						algoErrMessage_all = algoErrMessage_all + "|" + std::format("{}:{}", algoOrder->algoOrderId, algoErrMessage);
					}
				}

				if (resp->failed_algo_order_id().size() > 0) {

					resp->set_error_msg(fmt::format("[{}]success:{},failed:{}.{}", requestId, resp->success_algo_order_id().size(), resp->failed_algo_order_id().size(), algoErrMessage_all));
				}
				else {
					resp->set_error_msg("");
				}
			}
			else {
				resp->set_error_msg(algoErrMessage_all);
			}

			SPDLOG_INFO("[{}]create algoOrders SIZE:{}.{}", requestId, algoOrders.size(), algoErrMessage_all);
		}

		TCPSessionManager::getInstance().sendResp2C(session, cmd_response, resp);

		StorageService::getInstance().storeMessage(cmd, req, std::move(recvPkgPtr));
	}

	std::vector<std::shared_ptr<AlgoOrder>> createAlgoOrder(
		const AlgoMsg::MsgAlgoInstanceCreateRequest& req
		, AlgoErrorMessage_t& algoErrorMessage);

	AlgoErrorCode dispatchAlgoOrder(
		const std::shared_ptr<AlgoOrder>& algoOrder
		, AlgoErrorMessage_t& algoErrMessage);

	void onAlgoInstanceFinished(
		const std::shared_ptr<Trader>& traderPtr
		, const std::exception_ptr& ex);
};
