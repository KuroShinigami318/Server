#include "stdafx.h"

#include "Input/SystemInputDevice.h"
#include "Input/TextEvent.h"
#include "raw_input.h"
#include "thread_control_interface.h"

SystemInputDevice::SystemInputDevice(InputMode i_inputMode, utils::IMessageQueue& i_inputQueue, const utils::CallableBound<bool(const std::string&)>& i_exitPredicate, std::istream& i_inputStream)
    : IInputDevice(i_inputQueue)
    , m_inputMode(i_inputMode)
	, m_exitPredicate(i_exitPredicate)
	, m_inputStream(i_inputStream)
    , m_inputVar()
    , m_inputThread({ .thread_name = "input thread", .thread_prologue = {&SystemInputDevice::FirstRun, this}, .thread_epilogue = {&SystemInputDevice::OnCancel, this} })
{
    utils::async(m_inputThread, &SystemInputDevice::OnRun, this);
}

std::string SystemInputDevice::GetTextReceived(utils::IYielder* i_yielder) const
{
    m_isInputRequested = true;
    while (!m_inputRequest.has_value())
    {
        if (i_yielder)
        {
            i_yielder->DoYieldWithResult(utils::IYielder::Mode::Forced).ignoreResult();
        }
        else
        {
            std::unique_lock lk(m_mutex);
            m_cv.wait(lk, [this]() {return m_inputRequest.has_value(); });
        }
    }
    m_isInputRequested = false;
    std::string returnRequest = std::move(*m_inputRequest);
    m_inputRequest.reset();
    return returnRequest;
}

utils::IMessageQueue& SystemInputDevice::GetIntraInputQueue()
{
    return m_inputThread;
}

void SystemInputDevice::OnRun()
{
    utils::async_waitable<void> waitable = utils::async(m_inputThread, &SystemInputDevice::OnRun, this);
    if (int c = GetChar(); c != EOF && c != '\n')
    {
        m_inputVar += c;
        if (m_inputMode == InputMode::Line)
        {
            return;
        }
    }
    if (m_inputVar.empty())
    {
		return;
    }
    if (m_isInputRequested)
    {
        m_inputRequest = m_inputVar;
        m_cv.notify_one();
    }
    if (m_exitPredicate(m_inputVar))
    {
        waitable.Cancel();
		utils::async(m_inputQueue, &utils::timer_message_thread::shutdown, &m_inputThread);
    }
    else
    {
		EmitEvent(utils::make_unique<TextEvent>(m_inputVar));
		m_inputVar.clear();
    }
}

void SystemInputDevice::OnCancel()
{
    EmitEvent(utils::make_unique<TextEvent>(m_inputVar));
}

void SystemInputDevice::FirstRun()
{
}

int SystemInputDevice::GetChar()
{
    return m_inputMode == InputMode::Char ? utils::raw_getch() : m_inputStream.get();
}