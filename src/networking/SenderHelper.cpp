#include "stdafx.h"
#include "networking/SenderHelper.h"
#include "ISocket.h"
#include "networking/RawTransferData.h"
#include "nlohmann/json.hpp"

SenderHelper::SenderHelper(const utils::milisecs i_timeoutMs)
	: m_timeoutMs(i_timeoutMs), m_senderThread(utils::thread_config{ "sender thread" })
{
}

Result<void, SendRawTransferDataError> SenderHelper::SendRawTransferData(ISocket& i_socket, RawTransferData& rawData)
{
	ISocket::BytesT bytes;
	try
	{
		if (rawData.totalBytes < DATA_BUFSIZE / 2)
		{
			nlohmann::json sendJson = rawData;
			SERIALIZE_FUNC(sendJson, bytes);
			if (auto writeResult = i_socket.WriteBytesAsync(bytes); writeResult.isErr())
			{
				return make_inner_error<SendRawTransferDataError>(TransferErrorCode::SendFailed, writeResult.unwrapErr());
			}
		}
		else
		{
			ISocket::BytesT msgBytes(rawData.msg.begin(), rawData.msg.end());
			rawData.msg.clear();
			nlohmann::json sendJson = rawData;
			SERIALIZE_FUNC(sendJson, bytes);
			auto waitable = utils::async(m_senderThread, [](ISocket* i_socket, ISocket::BytesT bytes, ISocket::BytesT msgBytes)
				->Result<void, SendRawTransferDataError>
				{
					if (auto writeResult = i_socket->WriteBytes(bytes); writeResult.isErr())
					{
						return make_inner_error<SendRawTransferDataError>(TransferErrorCode::SendFailed, writeResult.unwrapErr());
					}
					if (auto writeResult = i_socket->WriteBytesAsync(msgBytes); writeResult.isErr())
					{
						return make_inner_error<SendRawTransferDataError>(TransferErrorCode::SendFailed, writeResult.unwrapErr());
					}
					return Ok();
				}, &i_socket, bytes, msgBytes);
			auto waitfor = waitable.WaitFor(m_timeoutMs);
			if (waitfor.isErr())
			{
				return make_inner_error<SendRawTransferDataError>(TransferErrorCode::SendFailed, waitfor.unwrapErr());
			}
			if (waitfor.unwrap())
			{
				return *waitable.GetPtrResult().unwrap();
			}
			waitable.Cancel();
			return make_error<SendRawTransferDataError>(TransferErrorCode::Timeout);
		}
	}
	catch (const nlohmann::json::exception& e)
	{
		return make_inner_error<SendRawTransferDataError>(TransferErrorCode::SerializationFailed, SerializationErrorCode::SerializeFailed, "Serialize failed! {}", e.what());
	}

	return Ok();
}