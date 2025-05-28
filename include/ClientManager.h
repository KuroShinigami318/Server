#pragma once
#include "AsyncScopedHelper.h"
#include "TimerDelayer.h"
#include "ISocket.h"
#include "Log.h"
#include "TransferData.h"
#include "networking/ReceiverHelper.h"
#include "networking/SendRawTransferDataError.h"

class ISocket;
class SenderHelper;

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

	const float k_disconnectTimeout = 1800000;
	utils::MessageSink_mt m_messageQueue;
	std::shared_mutex m_mutex;
	utils::unique_ref<SenderHelper> m_senderHelper;
	utils::AsyncScopedHelper m_asyncScopedHelper;
	ReceiverHelper<CustomTransferData> m_receiverHelper;
};