#include "Input/TextEvent.h"

TextEvent::TextEvent(const std::string& i_text) : m_text(i_text) {}

const std::string& TextEvent::GetText() const
{
	return m_text;
}

utils::steady_clock::time_point TextEvent::GetTimestamp() const
{
	return m_timestamp;
}