// Do not modify this file

#pragma once
#include <queue>

namespace utils
{
template <typename T, typename U>
class TaskLooper;

template <typename RunResult, typename Context, typename Callback_t>
class TaskLooper<RunResult(Context::*)(), Callback_t>
{
public:
	using Task_t = RunResult(Context::*)();

	struct Task
	{
		Task_t runnable;
		Callback_t callback;
	};

public:
	TaskLooper(Context& i_ctx, std::mutex& i_mutex, std::condition_variable& i_cv)
		: m_ctx(i_ctx), m_mutex(i_mutex), m_cv(i_cv)
	{
	}

	void Run(std::function<bool()> i_predicate)
	{
		while (true)
		{
			std::unique_lock lk(m_mutex);
			m_cv.wait(lk, [this, i_predicate]() { return !m_taskQueue.empty() || i_predicate(); });
			if (i_predicate() && m_taskQueue.empty()) return;

			Task task = m_taskQueue.front();
			m_taskQueue.pop();
			lk.unlock();
			task.callback((m_ctx.*task.runnable)());
		}
	}

	void Push(Task_t i_runnable, Callback_t i_callback)
	{
		if (m_mutex.try_lock())
		{
			CRASH("You are trying to do something undefined behavior!");
		}
		m_taskQueue.emplace(Task{ i_runnable, i_callback });
	}

private:
	Context& m_ctx;
	std::mutex& m_mutex;
	std::condition_variable& m_cv;
	std::queue<Task> m_taskQueue;
};
}