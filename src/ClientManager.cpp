#include "stdafx.h"
#include "ClientManager.h"
#include "hashing/hashable.h"
#include "IAcceptEventHandler.h"
#include "ICloseConnectionEventHandler.h"
#include "IReadEventHandler.h"
#include "IWriteEventHandler.h"
#include "Log.h"
#include "networking/ReceiverHelper.h"
#include "networking/SenderHelper.h"
#include "networking/SendRawTransferDataError.h"
#include "Socket_reactor.h"
#include "TransferData.h"
#include "thread_control_interface.h"
#include "Input/IInputDevice.h"
#include "Input/TextEvent.h"

namespace
{
constexpr const long long k_timeoutMs = 500;
constexpr const size_t k_expectedHash = 13379945649500584976ull;

template <typename R> requires utils::concept_n::result<std::decay_t<R>>
bool HandleError(R&& o_result)
{
	const std::decay_t<R>& result = std::forward<R>(o_result);
	if (result.isErr())
	{
		ERROR_LOG("Server Error", "{}", result.unwrapErr());
		return true;
	}
	return false;
}

class AcceptEventHandler : public IAcceptEventHandler
{
public:
	AcceptEventHandler(ClientManager& i_clientManager)
		: m_clientManager(i_clientManager)
	{
	}

public:
	bool HandleAcceptEvent(ISocket& socket) override
	{
		m_clientManager.AddClient(socket);
		INFO_LOG("server info", "connected with {}", socket.GetIPAddress());
		return true;
	}

private:
	ClientManager& m_clientManager;
};

class CloseConnectionEventHandler : public ICloseConnectionEventHandler
{
public:
	CloseConnectionEventHandler(ClientManager& i_clientManager)
		: m_clientManager(i_clientManager)
	{
	}

public:
	bool HandleCloseEvent(ISocket& socket) override
	{
		m_clientManager.RemoveClient(socket);
		INFO_LOG("server info", "disconnected with {}", socket.GetIPAddress());
		return true;
	}

private:
	ClientManager& m_clientManager;
};

class ReadEventHandler : public IReadEventHandler
{
public:
	ReadEventHandler(ClientManager& i_clientManager)
		: m_clientManager(i_clientManager)
	{
	}

public:
	bool HandleReadEvent(const std::vector<char>& i_bytes, ISocket& i_socket) override
	{
		m_clientManager.UpdateClientData(i_socket, i_bytes);
		return true;
	}

private:
	ClientManager& m_clientManager;
};

void ProcessShutdownEvent(SocketReactor& socketReactor, const IEvent& event)
{
	if (static_cast<const TextEvent&>(event).GetText().find(k_exitString) != std::string::npos)
	{
		socketReactor.Shutdown();
	}
}
}

ClientManager::~ClientManager() = default;

ClientManager::ClientManager()
	: m_senderHelper(utils::make_unique<SenderHelper>(utils::milisecs(k_timeoutMs)))
	, m_receiverHelper(utils::make_unique<ReceiverHelper<CustomTransferData>>())
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
		ERROR_LOG(i_socket.GetIPAddress(), "Serialize failed! {}", e.what());
	}
}

void ClientManager::RemoveClient(ISocket& i_socket)
{
	std::unique_lock lock(m_mutex);
}

void ClientManager::UpdateClientData(ISocket& i_socket, const std::vector<char>& i_bytes)
{
	std::shared_lock sharedLock(m_mutex);
	ReceiveResult<CustomTransferData> receiveResult = m_receiverHelper->ReceiveRawTransferData(i_socket, i_bytes);
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
		m_asyncScopedHelper.StartOptionalTask(m_messageQueue, &ClientManager::RemoveClient, this, i_socket);
	}
	break;
	case TransferData::MsgType::Logging:
	{
		sharedLock.unlock();
		{
			std::string logMessage{ rawData.msg.begin(), rawData.msg.end() };
			std::unique_lock lock(m_mutex);
			INFO_LOG(i_socket.GetIPAddress(), "Logging: {}", logMessage);
		}
	}
	break;
	case TransferData::MsgType::VerifyValidation:
	{
		TransferData sendData;
		std::string rawFileValidate{ rawData.msg.begin(), rawData.msg.end() };
		size_t actualHash = 0ull;
		hash_combine(actualHash, rawFileValidate.c_str(), hashing::hashable<const char*>());
		if (actualHash != k_expectedHash)
		{
			sendData.msgType = TransferData::MsgType::InvalidateSession;
			m_senderHelper->SendRawTransferData(i_socket, sendData).assertSuccess();
			ERROR_LOG("Cheat detector", "Found cheater: {}", i_socket.GetIPAddress());
			return;
		}
		sendData.msgType = TransferData::MsgType::VerifyValidation;
		m_senderHelper->SendRawTransferData(i_socket, sendData).assertSuccess();
	}
	break;
	default:
	{
		ERROR_LOG(i_socket.GetIPAddress(), "Invalid msg type: {}", rawData.msgType);
	}
	break;
	}
}

void ClientManager::Update(float i_elapsed)
{
	m_messageQueue.dispatch();
}

void StartServer(const IInputDevice& i_inputDevice, utils::IMessageQueue& i_updateQueue, utils::IYielder& i_yielder)
{
	ClientManager clientManager;
	SocketReactor socketReactor(SocketReactor::InitType::Bind, BS_AF_INET, "", 8081);

	std::atomic_bool shuttingDown = false;
	utils::async(i_updateQueue, [&socketReactor, &shuttingDown, &clientManager, &i_yielder](utils::IMessageQueue& inputQueue, const IInputDevice& i_inputDevice)
	{
		utils::steady_clock::time_point tp = utils::steady_clock::now();
		i_inputDevice.sig_onEventReceived.Connect(ProcessShutdownEvent, socketReactor).Detach();
		while (!shuttingDown)
		{
			utils::steady_clock::duration elapsed = utils::steady_clock::now() - tp;
			tp = utils::steady_clock::now();
			socketReactor.Update(utils::duration<float>(elapsed).count());
			clientManager.Update(utils::duration<float>(elapsed).count());
			i_yielder.DoYieldWithResult(utils::IYielder::Mode::Forced).ignoreResult();
		}
	}, i_updateQueue, i_inputDevice);
	socketReactor.RegisterEventHandler(SocketEvent::AcceptConnection, std::make_unique<AcceptEventHandler>(clientManager)).ignoreResult();
	socketReactor.RegisterEventHandler(SocketEvent::CloseConnection, std::make_unique<CloseConnectionEventHandler>(clientManager)).ignoreResult();
	socketReactor.RegisterEventHandler(SocketEvent::ReadStream, std::make_unique<ReadEventHandler>(clientManager)).ignoreResult();
	auto result = socketReactor.Run();
	if (result.isErr())
	{
		ERROR_LOG("ERROR", "{}", result.unwrapErr());
	}
	shuttingDown = true;
}