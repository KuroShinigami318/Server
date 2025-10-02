#pragma once

class IEvent
{
public:
	virtual ~IEvent() = default;
	virtual utils::steady_clock::time_point GetTimestamp() const = 0;
};