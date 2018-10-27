#include "pch.h"
#include <iostream>
#include <windows.h>
#include "SynchronizationContext.h"

SynchronizationContext _sync;


template<typename ValueType>
struct Generator
{
	struct promise_type
	{
	public:
		ValueType _value;

		std::experimental::suspend_always initial_suspend() { return {}; }
		std::experimental::suspend_always final_suspend() { return {}; }

		auto get_return_object()
		{
			return Generator(HandleType::from_promise(*this));
		}

		void unhandled_exception()
		{
			std::terminate();
		}

		std::experimental::suspend_always yield_value(ValueType value)
		{
			_value = value;
			return {};
		}

		std::experimental::suspend_always return_value(ValueType value)
		{
			_value = value;
			return {};
		}
	};

private:
	using HandleType = std::experimental::coroutine_handle<promise_type>;

	HandleType _coroutineHandle;

public:
	explicit Generator(HandleType h) : _coroutineHandle(h)
	{
	}

	Generator(const Generator& src) = delete;

	Generator(Generator&& rhs) :
		_coroutineHandle(rhs._coroutineHandle)
	{
		rhs._coroutineHandle = nullptr;
	}

	~Generator()
	{
		if (_coroutineHandle != nullptr)
		{
			_coroutineHandle.destroy();
		}
	}

	ValueType GetCurrentValue() const
	{
		return _coroutineHandle.promise()._value;
	}

	bool MoveNext()
	{
		_coroutineHandle.resume();
		return !_coroutineHandle.done();
	}
};


template<typename ValueType>
struct AwaitableFuture
{
	
	struct promise_type
	{
		std::promise<ValueType> _value;

		std::experimental::suspend_never initial_suspend() { return {}; }
		std::experimental::suspend_always final_suspend() { return {}; }

		auto get_return_object()
		{
			return AwaitableFuture(_value.get_future(), HandleType::from_promise(*this));
		}

		void unhandled_exception()
		{
			std::terminate();
		}

		std::experimental::suspend_never return_value(ValueType value)
		{
			_value.set_value(value);
			return {};
		}

		auto await_transform(std::future<ValueType> f)
		{
			struct Awaiter
			{
				std::future<ValueType> _future;

				bool await_ready() const
				{
					return _future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
				}

				void await_suspend(HandleType h)
				{
					std::thread([this, h]()
					{
						_future.wait();
						_sync.Post([h]()
						{
							h.resume();
						});
					}).detach();
				}

				ValueType await_resume()
				{
					return _future.get();
				}
			};


			return Awaiter{ std::move(f) };
		}
	};

private:
	using HandleType = std::experimental::coroutine_handle<promise_type>;
	std::future<ValueType> _future;
	HandleType _coroutineHandle;

public:
	explicit AwaitableFuture(std::future<ValueType>&& future, HandleType h) :
		_future(std::move(future)),
		_coroutineHandle(h)
	{
	}

	AwaitableFuture(const AwaitableFuture&) = delete;
	AwaitableFuture(AwaitableFuture&& rhs) :
		_future(std::move(rhs._future)),
		_coroutineHandle(rhs._coroutineHandle)
	{
		rhs._coroutineHandle = nullptr;
	}


	~AwaitableFuture()
	{
		if (_coroutineHandle != nullptr)
		{
			_coroutineHandle.destroy();
		}
		_coroutineHandle = nullptr;
	}

	ValueType GetReturnValue()
	{
		return _future.get();
	}

	bool IsCompleted() const
	{
		return _future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
	}
};



AwaitableFuture<int> TestAsync()
{
	for (int i = 0; i < 5; i++)
	{
		auto n = co_await std::async(std::launch::async, [i]() -> int
		{
			std::this_thread::sleep_for(std::chrono::seconds(2));
			return i;
		});

		printf("%d\n", n);
	}

	return 0;
}

int main()
{
	auto f = TestAsync();

	while (!f.IsCompleted())
	{
		_sync.Main();
	}

	printf("%d\n", f.GetReturnValue());

	return 0;
}



