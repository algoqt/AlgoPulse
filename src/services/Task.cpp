#include "Task.h"

std::unordered_map<AlgoMsg::MsgAlgoCMD, std::function<std::shared_ptr<Task>()>> Task::cmd2Task = {};
