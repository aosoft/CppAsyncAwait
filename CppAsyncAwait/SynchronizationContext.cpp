#include "pch.h"
#include "SynchronizationContext.h"

SynchronizationContext::SynchronizationContext() :
	_requestedAbort(false)
{
}

void SynchronizationContext::Post(std::function<void(void)> func)
{
	std::lock_guard<std::mutex> lock(_mutex);

	_functions.push_back(func);

	_cv.notify_all();
}

void SynchronizationContext::Main()
{
	std::unique_lock<std::mutex> lock(_mutex);
	std::function<void(void)> func;
	_cv.wait(lock, [&]()
		{
			if (_requestedAbort)
			{
				func = nullptr;
				return true;
			}
			else if (_functions.size() > 0)
			{
				func = *_functions.begin();
				_functions.pop_front();
				return true;
			}
			return false;
		});

	if (func != nullptr)
	{
		func();
	}
}
