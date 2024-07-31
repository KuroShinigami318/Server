#include "stdafx.h"
#include "AsyncScopedHelper.h"

AsyncScopedHelper::AsyncScopedHelper(AsyncScopedHelper&& other) noexcept
{
	asyncList.splice(asyncList.cend(), other.asyncList);
}

AsyncScopedHelper& AsyncScopedHelper::operator=(AsyncScopedHelper&& other) noexcept
{
	if (this != &other)
	{
		asyncList.splice(asyncList.cend(), other.asyncList);
	}
	return *this;
}

AsyncScopedHelper::~AsyncScopedHelper()
{
	std::for_each(asyncList.begin(), asyncList.end(), CancelTask);
}

void AsyncScopedHelper::CancelTask(utils::async_waitable<void>& task)
{
	utils::MessageHandleERR cancelResult = task.Cancel();
	ASSERT_PLAIN_MSG(cancelResult == utils::MessageHandleERR::SUCCESS, "cancel task failed: {}", cancelResult);
}

bool AsyncScopedHelper::HasTaskFinished(const utils::async_waitable<void>& i_task)
{
	return i_task.HasFinished();
}

void AsyncScopedHelper::Update()
{
	std::erase_if(asyncList, HasTaskFinished);
}