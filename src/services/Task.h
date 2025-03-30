#pragma once

#include "typedefs.h"
#include "AlgoMessages.pb.h"
#include <google/protobuf/text_format.h>

class TCPSession;

class Task {

public:

	Task(AlgoMsg::MsgAlgoCMD cmd_req, AlgoMsg::MsgAlgoCMD cmd_res):cmd_request(cmd_req)
		, cmd_response(cmd_res){

		printer.SetSingleLineMode(true);
		printerOutPut.reserve(2048);
	}

	static void registerTask(AlgoMsg::MsgAlgoCMD cmd, std::function<std::shared_ptr<Task>()> task) {
		SPDLOG_INFO("--registerTask,cmd:{},{} ", AlgoMsg::MsgAlgoCMD_Name(cmd),(int)cmd);
		cmd2Task.try_emplace(cmd, task);
	}

	static std::shared_ptr<Task> getTask(AlgoMsg::MsgAlgoCMD cmd) {
		if (auto it = cmd2Task.find(cmd); it != cmd2Task.end()) {
			return it->second();
		}
		return nullptr;
	}

	virtual void handleMessage(const std::shared_ptr<TCPSession>& session
		, AlgoMsg::MsgAlgoCMD cmd
		, std::unique_ptr<AlgoMsg::MessagePkg>&& recvPkgPtr) {};

public:

	google::protobuf::TextFormat::Printer printer{};

	std::string	  printerOutPut;

	static std::unordered_map<AlgoMsg::MsgAlgoCMD, std::function<std::shared_ptr<Task>()>> cmd2Task;

	AlgoMsg::MsgAlgoCMD cmd_request;
	AlgoMsg::MsgAlgoCMD cmd_response;

};


#define TASK_CONSTURCTOR_DECLARE(cmd_req,cmd_res,TASK_SUB_CLASS)	\
public:													\
	TASK_SUB_CLASS():Task(cmd_req,cmd_res) {	\
	Task::registerTask(cmd_request,TASK_SUB_CLASS::createTask);		\
}																	\
																	\
static std::shared_ptr<Task> createTask() {							\
	static std::shared_ptr<Task> ptr = std::make_shared<TASK_SUB_CLASS>();	\
	return ptr;																\
}

/*#define TASK_REGISTER(cmd,TASK_SUB_CLASS)	\
auto result = Task::registerTask(cmd,TASK_SUB_CLASS::createTask);	\
}	*/