#pragma once

#include "pch.h"

class SynchronizationContext
{
private:
	std::mutex _mutex;
	std::condition_variable _cv;
	bool _requestedAbort;
	std::deque<std::function<void(void)>> _functions;

public:
	SynchronizationContext();
	void Post(std::function<void(void)> func);
	void Main();

	void Abort()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_requestedAbort = true;
		_cv.notify_all();
	}
};

