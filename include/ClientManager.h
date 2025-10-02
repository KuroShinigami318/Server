#pragma once
#include "AsyncScopedHelper.h"

const std::string k_exitString = "exit";

struct CustomTransferData;
class IInputDevice;
class IYielder;
class ISocket;
class SenderHelper;
template <typename CustomTransferData>
class ReceiverHelper;

class ClientManager
{
public:
	ClientManager();
	~ClientManager();
	void Update(float);
	void AddClient(ISocket&);
	void RemoveClient(ISocket&);
	void UpdateClientData(ISocket&, const std::vector<char>& i_bytes);

private:
	const float k_disconnectTimeout = 1800000;
	utils::MessageSink_mt m_messageQueue;
	std::shared_mutex m_mutex;
	utils::unique_ref<SenderHelper> m_senderHelper;
	utils::AsyncScopedHelper m_asyncScopedHelper;
	utils::unique_ref<ReceiverHelper<CustomTransferData>> m_receiverHelper;
};

void StartServer(const IInputDevice& i_inputDevice, utils::IMessageQueue& i_updateQueue, utils::IYielder& i_yielder, utils::async_waitable<void>* o_asyncWaitForServerStarted = nullptr, std::ostream& i_ostream = std::cout);