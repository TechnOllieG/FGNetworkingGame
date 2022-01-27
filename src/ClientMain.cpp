#if CLIENT

#include "Engine.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include "PongDefinitions.h"
#include <mutex>

Vector2 ballPos;
float leftBlockPos;
float rightBlockPos;
uint8_t leftSideScore;
uint8_t rightSideScore;
uint8_t playerId = -1;

std::mutex recieveMutex;

bool connected = false;
std::mutex connectedMutex;
float youTextX[] {ResolutionWidth * 0.25f, ResolutionWidth - ResolutionWidth * 0.25f};

DWORD recvMessages(void* socket)
{
	SOCKET sock = reinterpret_cast<SOCKET>(socket);

	PacketToClient temp;

	while (true)
	{
		int length = recv(sock, (char*) &temp, sizeof(PacketToClient), 0);
		if (length == 0 || length == SOCKET_ERROR)
		{
			engPrint("Server was shut down");
			connectedMutex.lock();
			connected = false;
			connectedMutex.unlock();
			return 0;
		}

		if (temp.messageSent)
		{
			engPrint(temp.message);
		}

		recieveMutex.lock();
		ballPos = temp.ballPos;
		leftBlockPos = temp.leftBlockPos;
		rightBlockPos = temp.rightBlockPos;
		leftSideScore = temp.leftSideScore;
		rightSideScore = temp.rightSideScore;
		playerId = temp.playerId;
		recieveMutex.unlock();
	}

	return 0;
}

int WinMain(HINSTANCE, HINSTANCE, char*, int)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	engInit();

	PacketToServer toServer;
	SOCKET sock;

	while (engBeginFrame())
	{
		engSetColor(0x222244FF);
		engClear();

		connectedMutex.lock();
		bool tempConnect = connected;
		connectedMutex.unlock();

		if (!tempConnect)
		{
			engSetColor(0xFFFFFFFF);
			engText(10, 10, "Press enter to connect (ip is 31.208.245.110:1337)!");

			if (engKeyPressed(Key::Return))
			{
				sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (sock == INVALID_SOCKET)
				{
					return -1;
				}

				uint8_t ipBytes[] = { 31, 208, 245, 110 };

				sockaddr_in connectAddr;
				connectAddr.sin_family = AF_INET;
				connectAddr.sin_addr.s_addr = *(uint32_t*)ipBytes;
				connectAddr.sin_port = htons(Port);

				connect(sock, (sockaddr*)&connectAddr, sizeof(connectAddr));
				if (sock != INVALID_SOCKET)
				{
					connected = true;
					CreateThread(nullptr, 0, recvMessages, (void*)sock, 0, nullptr);
				}
				else
				{
					engPrint("Tried to connect but couldn't");
				}
			}
			
			continue;
		}

		toServer.movingDown = engKeyDown(Key::Down) || engKeyDown(Key::S);
		toServer.movingUp = engKeyDown(Key::Up) || engKeyDown(Key::W);
		send(sock, (char*)&toServer, sizeof(toServer), 0);

		recieveMutex.lock();
		renderPong(ballPos, leftBlockPos, rightBlockPos, leftSideScore, rightSideScore, playerId);
		if(playerId != -1)
			engText(youTextX[playerId], ScoreTopMargin, "(YOU)");
		recieveMutex.unlock();
	}

	return 0;
}
#endif
