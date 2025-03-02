#pragma once

#include "typedefs.h"
#include "ContextService.h"
#include <sqlite3.h>
#include "AlgoMessages.pb.h"

class StorageService {

public:

	static StorageService& getInstance() {

		static StorageService instance{};

		return instance;
	}

	void storeMessage(const AlgoMsg::MsgAlgoCMD cmd, const std::shared_ptr<google::protobuf::Message> msg) {

		m_runContextPtr->post([cmd,msg,this]() {

			switch (cmd)
			{
				case AlgoMsg::CMD_NOTIFY_ShotPerformance: {
					auto cast_msg = std::dynamic_pointer_cast<AlgoMsg::MsgShotPerformance> (msg);
					storeMessage(cast_msg);
					break;
				}

				case AlgoMsg::CMD_NOTIFY_ShotSignalInfo: {
					auto cast_msg = std::dynamic_pointer_cast<AlgoMsg::MsgShotSignalInfo> (msg);
					storeMessage(cast_msg);
					break;
				}
				case AlgoMsg::CMD_NOTIFY_AlgoExecutionInfo: {
					auto cast_msg = std::dynamic_pointer_cast<AlgoMsg::MsgAlgoPerformance> (msg);
					storeMessage(cast_msg);
					break;
				}

				default:
					break;
			} 
		});
	}

	void createTable(sqlite3* db);

private:

	StorageService();

	~StorageService();

	AsioContextPtr		m_runContextPtr;

	sqlite3*			m_db;

	/*std::mutex			m_mutex;*/

	void storeMessage(const std::shared_ptr<AlgoMsg::MsgShotSignalInfo> msg);

	void storeMessage(const std::shared_ptr<AlgoMsg::MsgShotPerformance> msg);

	void storeMessage(const std::shared_ptr<AlgoMsg::MsgAlgoPerformance> msg);
};