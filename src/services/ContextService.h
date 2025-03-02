#pragma once

#include <vector>
#include <boost/asio.hpp>
#include <thread>

namespace asio = boost::asio;

using AsioContextPtr =  std::shared_ptr<asio::io_context> ;

class ContextService {

public:
	static ContextService& getInstance() {

		static ContextService instance{};

		return instance;
	}

	AsioContextPtr createContext(const std::string& applicationKey, int numOfThread = 1);

	void stopContext(const std::string& applicationKey);

	std::vector<AsioContextPtr> getWorkerContext(std::size_t n = 0);

	AsioContextPtr getRandomWorkerContext();

	AsioContextPtr getHashedWorkerContext(const std::string& str2hash);

	~ContextService();

private:

	ContextService() = default;

	std::mutex			m_mutex;

	std::unordered_map<std::string, AsioContextPtr> appKey2contextPtr{};

	std::vector<std::thread>						m_threads{};

	std::unordered_map<std::string, asio::executor_work_guard<asio::io_context::executor_type>> appKey2workGuard{};

	std::vector<AsioContextPtr>						m_workerContexts;
};