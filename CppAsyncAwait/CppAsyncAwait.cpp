#include "pch.h"
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
struct Awaitable
{
	
	struct promise_type
	{
		std::promise<ValueType> _value;

		std::experimental::suspend_never initial_suspend() { return {}; }
		std::experimental::suspend_always final_suspend() { return {}; }

		auto get_return_object()
		{
			return Awaitable(_value.get_future(), HandleType::from_promise(*this));
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
	explicit Awaitable(std::future<ValueType>&& future, HandleType h) :
		_future(std::move(future)),
		_coroutineHandle(h)
	{
	}

	Awaitable(const Awaitable&) = delete;
	Awaitable(Awaitable&& rhs) :
		_future(std::move(rhs._future)),
		_coroutineHandle(rhs._coroutineHandle)
	{
		rhs._coroutineHandle = nullptr;
	}


	~Awaitable()
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


template<class FutureType>
FutureType TestAsync()
{
	std::cout << "TestAsync tid:" << std::this_thread::get_id() << std::endl;
	for (int i = 0; i < 5; i++)
	{
		auto n = co_await std::async(std::launch::async, [i]() -> int
		{
			std::cout << "TestAsync - async tid:" << std::this_thread::get_id() << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
			return i;
		});

		std::cout << "TestAsync - await tid:" << std::this_thread::get_id() << " " << n << std::endl;
	}

	return 0;
}

int main()
{
	std::cout << "main 1 - tid:" << std::this_thread::get_id() << std::endl;

	auto f1 = TestAsync<std::future<int>>();

	std::cout << "main 1 - tid:" << std::this_thread::get_id() << " " << f1.get() << std::endl;

	std::cout << "main 2 - tid:" << std::this_thread::get_id() << std::endl;

	auto f2 = TestAsync<Awaitable<int>>();

	while (!f2.IsCompleted())
	{
		_sync.Main();
	}

	std::cout << "main 2 - tid:" << std::this_thread::get_id() << " " << f2.GetReturnValue() << std::endl;

	return 0;
}



