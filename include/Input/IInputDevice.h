#pragma once
#include "IEvent.h"

namespace utils
{
class IYielder;
}

class IInputDevice
{
protected:
    struct AccessKey;
    IInputDevice(utils::IMessageQueue& i_inputQueue)
        : m_inputQueue(i_inputQueue)
    {
        utils::async(m_inputQueue, [this]()
        {
            sig_onEventReceived.set_thread_id(utils::GetCurrentThreadID());
		});
    }

    void EmitEvent(utils::unique_ref<const IEvent> i_event)
    {
        utils::async(m_inputQueue, [this](utils::unique_ref<const IEvent> i_event)
        {
            utils::Access<AccessKey>(sig_onEventReceived).Emit(*i_event);
        }, std::move(i_event));
	}

public:
    virtual ~IInputDevice()
    {
		sig_onEventReceived.set_thread_id(utils::GetCurrentThreadID());
    }
	utils::IMessageQueue& m_inputQueue;
    utils::Signal_st_da<void(const IEvent&), AccessKey> sig_onEventReceived;
};