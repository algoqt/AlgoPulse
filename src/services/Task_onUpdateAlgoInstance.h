#pragma once

#include "Task.h"
#include "TCPSession.h"
#include "StorageService.h"
#include "AlgoTrader.h"
#include "AlgoService.h"
#include "Configs.h"

class Task_onUpdateAlgoInstance :public Task {

	TASK_CONSTURCTOR_DECLARE(AlgoMsg::MsgAlgoCMD::CMD_AlgoInstanceUpdateRequest
		, AlgoMsg::MsgAlgoCMD::CMD_AlgoInstanceUpdateResponse
		, Task_onUpdateAlgoInstance)

	void handleMessage(const std::shared_ptr<TCPSession>& session
		, AlgoMsg::MsgAlgoCMD cmd
		, std::unique_ptr<AlgoMsg::MessagePkg>&& recvPkgPtr) override 
	{
		auto& algoSerive = AlgoService::getInstance();

		auto req = std::make_shared<AlgoMsg::MsgAlgoInstanceUpdateRequest>();
		auto resp = std::make_shared<AlgoMsg::MsgAlgoInstanceUpdateResponse>();

		google::protobuf::io::ArrayInputStream input(recvPkgPtr->body().data(), recvPkgPtr->body().length());

		auto parseOK = req->ParseFromBoundedZeroCopyStream(&input, recvPkgPtr->body().length());

		if (not parseOK) {
			printerOutPut.clear();
			printer.PrintToString(*recvPkgPtr, &printerOutPut);
			SPDLOG_ERROR("parse AlgoMsg::MsgAlgoInstanceUpdateRequest failed,{}", printerOutPut);

			resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);
			resp->set_error_msg("parse AlgoMsg::MsgAlgoInstanceUpdateRequest failed");
			TCPSessionManager::getInstance().sendResp2C(session, cmd_response, resp);
			return;
		}

		printerOutPut.clear();
		printer.PrintToString(*req, &printerOutPut);
		SPDLOG_INFO("recv update request:{}", printerOutPut);

		resp->set_request_id(req->request_id());
		resp->set_acct_type(req->acct_type());
		resp->set_acct(req->acct());
		resp->set_algo_order_id(req->algo_order_id());

		auto brokerId = agcommon::Configs::getConfigs().getBrokerIdFromAcct(req->acct_type(), req->acct());
		auto acctKey = AcctKey_t(req->acct_type(), req->acct(), brokerId);

		AlgoErrorMessage_t algoErrMessage;

		if (TCPSessionManager::getInstance().isLogin(acctKey) == false) {

			SPDLOG_ERROR("[{}][{}]acct not login,{}", req->request_id(), req->algo_order_id(), acctKey);

			resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ACCT_NOT_LOGIN);

			resp->set_error_msg(fmt::format("{} acct not login.", acctKey));
		}
		else {
			auto algoInstanceId = req->algo_order_id();

			auto action = req->action();

			if (action == AlgoMsg::MsgAlgoAction::ACTION_STOP) {

				auto it = algoSerive.algoOrderId2AlgoTrader.find(algoInstanceId);

				if (it == algoSerive.algoOrderId2AlgoTrader.end()) {

					resp->set_error_code((int32_t)AlgoErrorCode::ALGO_NOT_EXISTS);
					algoErrMessage = fmt::format("algoOrderId not exists:{}", algoInstanceId);
					resp->set_error_msg(algoErrMessage);
					SPDLOG_ERROR(algoErrMessage);
				}
				else {

					auto& trader = it->second;

					auto algoOrder = trader->getAlgoOrder();

					if (algoOrder->acct != req->acct() or algoOrder->acctType != req->acct_type()) {

						resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);
						algoErrMessage = fmt::format("algoOrderId:{} acct/acctType NOT Match:{},{}", algoInstanceId, req->acct(), req->acct_type());
						resp->set_error_msg(algoErrMessage);
						SPDLOG_ERROR(algoErrMessage);
					}
					else {
						if (trader->isStopped()) {

							resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);
							algoErrMessage = fmt::format("algoOrderId:{} already stopped", algoInstanceId);
							resp->set_error_msg(algoErrMessage);
							SPDLOG_ERROR(algoErrMessage);
						}

						else {

							resp->set_error_code((int32_t)AlgoErrorCode::ALGO_OK);
							asio::post(*trader->contextPtr, [trader]() {trader->cancel(); });
						}
					}
				}
			}
			else {

				resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);
				algoErrMessage = fmt::format("algoOrderId{} not support Update Action:{}", algoInstanceId, AlgoMsg::MsgAlgoAction_Name(action));
				resp->set_error_msg(algoErrMessage);
				SPDLOG_ERROR(algoErrMessage);
			}
			//resp.PrintDebugString();
		}

		TCPSessionManager::getInstance().sendResp2C(session, cmd_response, resp);
		StorageService::getInstance().storeMessage(cmd, req, std::move(recvPkgPtr));

	}

};
