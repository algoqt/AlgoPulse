#pragma once

#include <coroutine>
#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <boost/coroutine2/all.hpp>
#include "typedefs.h"

struct Wait2Resume {

	Wait2Resume(asio::io_context& io,int64_t id) : id(id), marketDepthTimer(io) {}

	asio::steady_timer marketDepthTimer;

	int64_t id;

	inline asio::awaitable<void> wait() {
		
		if (marketDepthTimer.expires_at() > std::chrono::steady_clock::now()) {
			marketDepthTimer.cancel();
		}

		marketDepthTimer.expires_at(boost::asio::steady_timer::time_point::max());
		auto t = co_await marketDepthTimer.async_wait(boost::asio::as_tuple(boost::asio::use_awaitable));
		SPDLOG_DEBUG("[aId:{}] co_await done ", id);
	}

	inline void clear() {
		
		if (marketDepthTimer.expires_at() > std::chrono::steady_clock::now()) {
			marketDepthTimer.cancel();
		}
		
	}
};
