#pragma once

#include "Task.h"
#include "TCPSession.h"
#include "Configs.h"

class Task_OnLogin :public Task {

	TASK_CONSTURCTOR_DECLARE(AlgoMsg::MsgAlgoCMD::CMD_LoginRequest, AlgoMsg::MsgAlgoCMD::CMD_LoginResponse, Task_OnLogin)

	void handleMessage(const std::shared_ptr<TCPSession>& session
		, AlgoMsg::MsgAlgoCMD cmd
		, std::unique_ptr<AlgoMsg::MessagePkg>&& recvPkgPtr) override 
	{
		AlgoMsg::MsgLoginRequest req;

		google::protobuf::io::ArrayInputStream input(recvPkgPtr->body().data(), recvPkgPtr->body().length());
		auto parseOK = req.ParseFromBoundedZeroCopyStream(&input, recvPkgPtr->body().length());

		if (not parseOK) {

			printerOutPut.clear();
			printer.PrintToString(*recvPkgPtr, &printerOutPut);
			SPDLOG_ERROR("parse AlgoMsg::MsgLoginRequest failed,{}", printerOutPut);
			return;
		}

		auto resp = std::make_shared<AlgoMsg::MsgLoginResponse>();

		resp->set_request_id(req.request_id());
		resp->set_acct_type(req.acct_type());
		resp->set_acct(req.acct());
		resp->set_desc(req.user_name());

		auto brokerId = agcommon::Configs::getConfigs().getBrokerIdFromAcct(req.acct_type(), req.acct());
		auto acctKey = AcctKey_t(req.acct_type(), req.acct(), brokerId);

		if (TCPSessionManager::getInstance().login(acctKey, req, session)) {
			resp->set_error_code((int32_t)AlgoErrorCode::ALGO_OK);
			resp->set_error_msg("login success");
		}
		else {
			AlgoMsg::MsgLoginResponse resp;
			resp.set_error_code((int32_t)AlgoErrorCode::ALGO_ACCT_ALREADY_LOGIN);
			resp.set_error_msg(fmt::format("{},{} login failed", req.acct_type(), req.acct()));
		}

		TCPSessionManager::getInstance().sendResp2C(session, cmd_response, resp);

		if (req.resendmessage()) {
			TCPSessionManager::getInstance().reSend(session, acctKey, req);
		}
	}
};
