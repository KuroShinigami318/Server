#pragma once

struct AsyncScopedHelper : utils::noncopy
{
public:
	AsyncScopedHelper() = default;
	AsyncScopedHelper(AsyncScopedHelper&&) noexcept;
	AsyncScopedHelper& operator=(AsyncScopedHelper&&) noexcept;
	~AsyncScopedHelper();
	void Update();

	template <typename... Args>
	void Push(utils::IMessageQueue& o_sink, Args&&... args)
	{
		asyncList.push_back(utils::async(o_sink, std::forward<Args>(args)...));
	}

private:
	static void CancelTask(utils::async_waitable<void>&);
	static bool HasTaskFinished(const utils::async_waitable<void>&);

	std::list<utils::async_waitable<void>> asyncList;
};