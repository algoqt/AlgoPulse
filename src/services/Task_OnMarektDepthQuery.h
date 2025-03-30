#pragma once

#include "Task.h"
#include "TCPSession.h"
#include "StockDataManager.h"
#include "ContextService.h"

class Task_OnMarektDepthQuery :public Task {

	TASK_CONSTURCTOR_DECLARE(AlgoMsg::MsgAlgoCMD::CMD_MarketDepthQueryRequest
		, AlgoMsg::MsgAlgoCMD::CMD_MarketDepthQueryResponse
		, Task_OnMarektDepthQuery)

	void handleMessage(const std::shared_ptr<TCPSession>& session
		, AlgoMsg::MsgAlgoCMD cmd
		, std::unique_ptr<AlgoMsg::MessagePkg>&& recvPkgPtr) override 
	{

		auto req = std::make_shared<AlgoMsg::MsgMarketDepthQueryRequest>();

		google::protobuf::io::ArrayInputStream input(recvPkgPtr->body().data(), recvPkgPtr->body().length());
		auto parseOK = req->ParseFromBoundedZeroCopyStream(&input, recvPkgPtr->body().length());

		auto resp = std::make_shared<AlgoMsg::MsgMarketDepthQueryResponse>();

		resp->set_request_id(req->request_id());
		resp->set_error_code((int32_t)AlgoErrorCode::ALGO_OK);
		resp->set_is_last(false);

		if (not parseOK) {
			printerOutPut.clear();
			printer.PrintToString(*recvPkgPtr, &printerOutPut);
			SPDLOG_ERROR("parse AlgoMsg::MsgMarketDepthQueryRequest failed,{}", printerOutPut);

			resp->set_error_code((int32_t)AlgoErrorCode::ALGO_ERROR);
			resp->set_error_msg("parse AlgoMsg::MsgMarketDepthQueryRequest failed");
			TCPSessionManager::getInstance().sendResp2C(session, cmd_response, resp);
			return;
		}

		printerOutPut.clear();
		printer.PrintToString(*req, &printerOutPut);
		SPDLOG_INFO("recv MsgMarketDepthQueryRequest:{}", printerOutPut);


		auto mayCostTimeTask = [this, session, req, resp , pkg = std::move(recvPkgPtr)]() mutable {

			std::map<QuoteTime_t, UnorderMarketDepthPtrMap> qt2Symobl2MdPtr;

			std::unordered_set symbols{ req->symbol() };

			auto lines = StockDataManager::getInstance().cacheFromH5Tick(symbols
				, *agcommon::parseDateTimeInteger(req->start_time())
				, *agcommon::parseDateTimeInteger(req->end_time())
				, qt2Symobl2MdPtr);

			const int pkgMds = 4800;
			int mdCount = 0;

			for (const auto& [qt, symbol2md] : qt2Symobl2MdPtr) {

				for (const auto& [symbol, mdPtr] : symbol2md) {
					auto x = resp->add_md_array();
					x->CopyFrom(*mdPtr->encode2AlgoMessage());
					mdCount++;
					if (mdCount >= pkgMds) {

						TCPSessionManager::getInstance().sendResp2C(session, cmd_response, resp);

						resp = std::make_shared<AlgoMsg::MsgMarketDepthQueryResponse>();
						resp->set_request_id(req->request_id());
						resp->set_error_code((int32_t)AlgoErrorCode::ALGO_OK);
						resp->set_is_last(false);
						mdCount = 0;
					}
				}

			}

			resp->set_is_last(true);
			TCPSessionManager::getInstance().sendResp2C(session, cmd_response, resp);
		};

		auto contextPtr = ContextService::getInstance().getRandomWorkerContext();

		asio::post(*contextPtr, std::move(mayCostTimeTask));
	}

};
