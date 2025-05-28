#include "stdafx.h"
#include "Log.h"
#include "IReadEventHandler.h"
#include "IWriteEventHandler.h"
#include "IAcceptEventHandler.h"
#include "ICloseConnectionEventHandler.h"
#include "Socket_reactor.h"
#include "ClientManager.h"

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
		INFO_LOG("server info", "disconnected with {}", socket.GetNativeSocket());
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

class WriteEventHandler : public IWriteEventHandler
{
	bool HandleWriteEvent(const size_t& i_bytesSent) override
	{
		INFO_LOG("server info", "bytes sent to client {}", i_bytesSent);
		return true;
	}
};

void ProcessShutdownInput(const std::string& input, SocketReactor& socketReactor)
{
	if (input.find("exit") != std::string::npos)
	{
		socketReactor.Shutdown();
	}
}

int main(int argc, char** argv)
{
	utils::Log::RegisterWriter<utils::Log::DefaultConsoleWriter>();
	ClientManager clientManager;
	utils::message_thread inputThread(utils::thread_config("Input Thread"));
	utils::message_thread updateThread(utils::thread_config("Update Thread"));
	utils::async_waitable<void> inputWaitable;

	utils::Epilogue autoStopInput([&inputThread]() { inputThread.terminate(); });

	SocketReactor socketReactor(SocketReactor::InitType::Bind, BS_AF_INET, "", 8081);
	std::atomic_bool shuttingDown = false;
	utils::async(updateThread, [&socketReactor, &shuttingDown, &clientManager, &inputWaitable, &inputThread]()
	{
		utils::steady_clock::time_point tp = utils::steady_clock::now();
		while (!shuttingDown)
		{
			utils::steady_clock::duration elapsed = utils::steady_clock::now() - tp;
			tp = utils::steady_clock::now();
			socketReactor.Update(utils::duration<float>(elapsed).count());
			clientManager.Update(utils::duration<float>(elapsed).count());
			std::this_thread::sleep_for(utils::milisecs(1));
			if (!inputWaitable.IsInitialized() || inputWaitable.HasFinished())
			{
				inputWaitable = utils::async(inputThread, [&socketReactor]()
				{
					std::string input;
					std::getline(std::cin, input);
					std::transform(input.cbegin(), input.cend(), input.begin(), [](char c) {return std::tolower(c); });
					ProcessShutdownInput(input, socketReactor);
				});
			}
		}
	});
	socketReactor.RegisterEventHandler(SocketEvent::AcceptConnection, std::make_unique<AcceptEventHandler>(clientManager)).ignoreResult();
	socketReactor.RegisterEventHandler(SocketEvent::CloseConnection, std::make_unique<CloseConnectionEventHandler>(clientManager)).ignoreResult();
	socketReactor.RegisterEventHandler(SocketEvent::ReadStream, std::make_unique<ReadEventHandler>(clientManager)).ignoreResult();
	socketReactor.RegisterEventHandler(SocketEvent::WriteStream, std::make_unique<WriteEventHandler>()).ignoreResult();
	auto result = socketReactor.Run();
	if (result.isErr())
	{
		ERROR_LOG("ERROR", "{}", result.unwrapErr());
	}
	shuttingDown = true;
	return 0;
}