#include "stdafx.h"
#include "ClientManager.h"
#include "system_clock.h"
#include "ThreadControl.h"
#include "Input/SystemInputDevice.h"

struct assert_handler final : default_handler
{
	assert_handler() : shouldAssert(false) {}
	void do_assert(const bool& eval_expr, const char* file, unsigned int line, const char* info) const override
	{
		if (shouldAssert)
		{
			default_handler::do_assert(eval_expr, file, line, info);
		}
	}

	bool shouldAssert;
};

int main(int argc, char** argv)
{
	set_handler(std::make_unique<assert_handler>());
	utils::SystemClock systemClock;
	utils::message_thread updateThread(utils::thread_config("Update Thread"));
	utils::RecursiveYielder yielder(updateThread, updateThread, systemClock);
	SystemInputDevice systemInputDevice(SystemInputDevice::InputMode::Line, updateThread, [](const std::string& input) { return input.find(k_exitString) != std::string::npos; });
	StartServer(systemInputDevice, updateThread, yielder);
	return 0;
}