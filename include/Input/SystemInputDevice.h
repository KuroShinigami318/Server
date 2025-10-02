#pragma once
#include "IInputDevice.h"

class SystemInputDevice : public IInputDevice
{
public:
    DeclareScopedEnum(InputMode, uint8_t, Char, Line)

public:
    SystemInputDevice(InputMode i_inputMode, utils::IMessageQueue& i_inputQueue, const utils::CallableBound<bool(const std::string&)>& i_exitPredicate, std::istream& i_inputStream = std::cin);
    std::string GetTextReceived(utils::IYielder* i_yielder = nullptr) const;
    utils::IMessageQueue& GetIntraInputQueue();

private:
    void OnRun();
    void OnCancel();
    void FirstRun();
    int GetChar();

    InputMode m_inputMode;
    const utils::CallableBound<bool(const std::string&)> m_exitPredicate;
    std::istream& m_inputStream;
    std::string m_inputVar;
    mutable std::optional<std::string> m_inputRequest;
    mutable bool m_isInputRequested = false;
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_cv;
    utils::timer_message_thread m_inputThread;
};
DefineScopeEnumOperatorImpl(InputMode, SystemInputDevice)