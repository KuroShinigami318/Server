#pragma once
#include "IEvent.h"

class TextEvent : public IEvent
{
public:
	TextEvent(const std::string& i_text);
	const std::string& GetText() const;
	utils::steady_clock::time_point GetTimestamp() const override;

private:
	std::string m_text;
	utils::steady_clock::time_point m_timestamp = utils::steady_clock::now();
};