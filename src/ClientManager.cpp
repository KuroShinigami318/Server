#include "stdafx.h"
#include "ClientManager.h"
#include "networking/SenderHelper.h"

const long long k_timeoutMs = 500;

ClientManager::~ClientManager() = default;

ClientManager::ClientManager()
	: m_senderHelper(utils::milisecs(k_timeoutMs))
{
}

ClientManager::ClientData::ClientData(ISocket* i_socket, float disconnectTimeout, utils::TimerDelayer::Mode i_mode)
	: socket(i_socket), checkTimer(disconnectTimeout, i_mode)
{
}

void ClientManager::AddClient(ISocket& i_socket)
{
	std::unique_lock lock(m_mutex);
	utils::unique_ref<ClientData>& clientData = m_clients.emplace_back(&i_socket, k_disconnectTimeout, utils::TimerDelayer::Mode::Interval);
	clientData->connection = clientData->checkTimer.sig_onExpired.Connect(&ClientManager::OnClientDisconnected, this, i_socket);
	clientData->checkTimer.SetThreadId(utils::details::threading::thread_id_t::k_invalid);
	RawTransferData rawData{.msgType = RawTransferData::MsgType::HandShake};
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
	std::unique_lock lock(m_mutex);
	std::erase_if(m_clients, [&i_socket](const utils::unique_ref<ClientData>& i_clientData)
	{
		return i_clientData->socket == &i_socket;
	});
}

void ClientManager::UpdateClientData(ISocket& i_socket, const std::vector<char>& i_bytes)
{
	std::shared_lock sharedLock(m_mutex);
	auto clientIt = std::find_if(m_clients.begin(), m_clients.end(), [&i_socket](const utils::unique_ref<ClientData>& i_clientData)
	{
		return i_clientData->socket == &i_socket;
	});
	if (clientIt == m_clients.end())
	{
		return;
	}
	ClientData& clientData = **clientIt;
	clientData.checkTimer.Reset();

	try
	{
		const bool hasPending = clientData.rawData.totalBytes != 0;
		RawTransferData rawData;
		RawTransferData::MsgType msgType = RawTransferData::MsgType::_COUNT;
		if (hasPending)
		{
			clientData.rawData.msg.insert(clientData.rawData.msg.end(), i_bytes.begin(), i_bytes.end());
			if (clientData.rawData.msg.size() < clientData.rawData.totalBytes)
			{
				return;
			}
			rawData = clientData.rawData;
			msgType = rawData.msgType;
			clientData.rawData = RawTransferData();
		}
		else
		{
			nlohmann::json rawJson = DESERIALIZE_FUNC(i_bytes);
			rawData = rawJson.get<RawTransferData>();
			msgType = rawData.msgType;
			if (rawData.totalBytes > rawData.msg.size())
			{
				clientData.rawData = rawData;
				return;
			}
		}
		switch (msgType)
		{
		case RawTransferData::MsgType::InvalidateSession:
		{
			sharedLock.unlock();
			{
				std::unique_lock lock(m_mutex);
				m_asyncScopedHelper.Push(m_messageQueue, &ClientManager::RemoveClient, this, i_socket);
			}
			sharedLock.lock();
		}
		break;
		default:
		{
			ERROR_LOG("Server Error", "Invalid msg type: {}", msgType);
		}
		break;
		}
	}
	catch (const nlohmann::json::exception& e)
	{
		ERROR_LOG("Server Error", "Deserialize failed! {} - raw message: {}", e.what(), i_bytes.data());
	}
}

void ClientManager::Update(float i_elapsed)
{
	m_messageQueue.dispatch();
	{
		std::unique_lock lock(m_mutex);
		std::erase_if(m_clients, [this](const utils::unique_ref<ClientData>& i_clientData)
		{
			auto disconnectedClientIt = std::find(m_disconnectedClients.begin(), m_disconnectedClients.end(), i_clientData->socket);
			if (disconnectedClientIt != m_disconnectedClients.end())
			{
				(*disconnectedClientIt)->Close();
				return true;
			}
			return false;
		});
		m_disconnectedClients.clear();
		m_asyncScopedHelper.Update();
	}
	std::shared_lock sharedLock(m_mutex);
	std::for_each(m_clients.begin(), m_clients.end(), [i_elapsed](utils::unique_ref<ClientData>& i_clientData)
	{
		i_clientData->checkTimer.Update(i_elapsed);
	});
}

void ClientManager::OnClientDisconnected(ISocket& i_socket)
{
	m_disconnectedClients.push_back(&i_socket);
}