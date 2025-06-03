#include "stdafx.h"
#include "ClientManager.h"
#include "networking/SenderHelper.h"

const long long k_timeoutMs = 500;

ClientManager::~ClientManager() = default;

ClientManager::ClientManager()
	: m_senderHelper(utils::make_unique<SenderHelper>(utils::milisecs(k_timeoutMs)))
{
}

void ClientManager::AddClient(ISocket& i_socket)
{
	std::unique_lock lock(m_mutex);
	TransferData rawData{};
	rawData.msgType = TransferData::MsgType::HandShake;
	bool shouldRemainConnection = true;
	try
	{
		nlohmann::json dataJson = shouldRemainConnection;
		SERIALIZE_FUNC(dataJson, rawData.msg);
		rawData.totalBytes = rawData.msg.size();
		HandleError(m_senderHelper->SendRawTransferData(i_socket, rawData));
	}
	catch (const nlohmann::json::exception& e)
	{
		ERROR_LOG("Server Error", "Serialize failed! {}", e.what());
	}
}

void ClientManager::RemoveClient(ISocket& i_socket)
{
}

void ClientManager::UpdateClientData(ISocket& i_socket, const std::vector<char>& i_bytes)
{
	std::shared_lock sharedLock(m_mutex);
	ReceiveResult<CustomTransferData> receiveResult = m_receiverHelper.ReceiveRawTransferData(i_socket, i_bytes);
	if (receiveResult.isErr())
	{
		const bool evaluate = receiveResult.storage().get<ReceiveRawTransferDataError>().Equals(ReceiveTransferErrorCode::PendingData);
		ASSERT_PLAIN_MSG(evaluate, "{}", receiveResult.unwrapErr());
		return;
	}

	TransferData rawData = receiveResult.unwrap();
	switch (rawData.msgType)
	{
	case TransferData::MsgType::InvalidateSession:
	{
		sharedLock.unlock();
		{
			std::unique_lock lock(m_mutex);
			m_asyncScopedHelper.StartOptionalTask(m_messageQueue, &ClientManager::RemoveClient, this, i_socket);
		}
		sharedLock.lock();
	}
	break;
	case TransferData::MsgType::Logging:
	{
		sharedLock.unlock();
		{
			std::string logMessage{ rawData.msg.begin(), rawData.msg.end() };
			std::unique_lock lock(m_mutex);
			std::cout << "Logging: " << logMessage << std::endl;
		}
		sharedLock.lock();
	}
	break;
	default:
	{
		ERROR_LOG("Server Error", "Invalid msg type: {}", rawData.msgType);
	}
	break;
	}
}

void ClientManager::Update(float i_elapsed)
{
	m_messageQueue.dispatch();
	{
		std::unique_lock lock(m_mutex);
	}
	std::shared_lock sharedLock(m_mutex);
}