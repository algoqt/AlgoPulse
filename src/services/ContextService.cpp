
#include "ContextService.h"
#include <spdlog/spdlog.h>

AsioContextPtr ContextService::createContext(const std::string& applicationKey, int numOfThread /*= 1*/) {

	std::scoped_lock lock(m_mutex);

	auto it = appKey2contextPtr.find(applicationKey);
	if (it != appKey2contextPtr.end()) {
		return it->second;
	}

	auto contextPtr = std::make_shared<asio::io_context>();
	auto workGuard  = asio::make_work_guard(contextPtr->get_executor());

	appKey2contextPtr.insert({ applicationKey,contextPtr });
	appKey2workGuard.insert({ applicationKey, std::move(workGuard) });

	for (int i = 0; i < numOfThread; i++) {
		m_threads.push_back(std::thread([this, applicationKey, numOfThread,i,contextPtr]()
			{
				SPDLOG_INFO("create aiso context[{}] run in {} threads ,this is {}th thread", applicationKey, numOfThread, i + 1);
				contextPtr->run();
			})
		);
	}
	if (applicationKey.starts_with("worker")) {
		m_workerContexts.push_back(contextPtr);
	}
	return contextPtr;
}

void ContextService::stopContext(const std::string& applicationKey) {

	std::scoped_lock lock(m_mutex)
		;
	auto it = appKey2workGuard.find(applicationKey);
	if (it != appKey2workGuard.end()) {
		auto& wg = it->second;
		wg.reset();
		appKey2workGuard.erase(it);
	}

	auto cit = appKey2contextPtr.find(applicationKey);
	if (cit != appKey2contextPtr.end()) {
		auto& contextPtr = cit->second;
		contextPtr->stop();
		appKey2contextPtr.erase(cit);
	}
}

ContextService::~ContextService() {

	for (auto& [key, wg] : appKey2workGuard) {
		wg.reset();
	}
	for (auto& [key, contextPtr] : appKey2contextPtr) {
		contextPtr->stop();
	}
	for (auto& th : m_threads) {
		if (th.joinable()) {
			th.join();
		}
	}
	appKey2contextPtr.clear();

	m_threads.clear();

	appKey2workGuard.clear();
}

std::vector<AsioContextPtr> ContextService::getWorkerContext(std::size_t n /*= 0*/) {

	std::scoped_lock lock(m_mutex);

	std::vector<AsioContextPtr> results{};
	
	if(n==0)
		return m_workerContexts;

	else {
		n = std::min(n, m_workerContexts.size());
		for (auto i = 0; i < n; i++) {
			results.push_back(m_workerContexts.at(i));
		}
		return results;
	}
}

AsioContextPtr ContextService::getRandomWorkerContext() {

	std::unique_lock lock(m_mutex);

	if (m_workerContexts.size() == 0) {
		lock.unlock();
		createContext("worker_random", 1);
		lock.lock();
	}

	auto index = std::rand() % m_workerContexts.size();

	return m_workerContexts.at(index);
}

AsioContextPtr ContextService::getHashedWorkerContext(const std::string& str2hash) {

	constexpr std::hash<std::string> hasher;

	std::unique_lock lock(m_mutex);

	if (m_workerContexts.size() == 0) {
		lock.unlock();
		createContext("worker_hash", 1);
		lock.lock();
	}

	auto index = hasher(str2hash) % m_workerContexts.size();

	return m_workerContexts.at(index);
}