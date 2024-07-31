#pragma once

#include "networking/SendRawTransferDataError.h"

class ISocket;
struct RawTransferData;

class SenderHelper
{
public:
	SenderHelper(const utils::milisecs i_timeoutMs);
	Result<void, SendRawTransferDataError> SendRawTransferData(ISocket& i_socket, RawTransferData& rawData);

private:
	const utils::milisecs m_timeoutMs;
	utils::message_thread m_senderThread;
};