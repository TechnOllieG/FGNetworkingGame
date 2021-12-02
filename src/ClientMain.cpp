#if CLIENT

#include "Engine.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include "PongDefinitions.h"

DWORD recvMessages(void* socket)
{
	SOCKET sock = reinterpret_cast<SOCKET>(socket);

	while (true)
	{
		char buffer[1060];
		int recvSize = recv(sock, buffer, 1060, 0);
		engPrint("%.*s", recvSize, buffer);
	}

	return 0;
}

int WinMain(HINSTANCE, HINSTANCE, char*, int)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	uint8_t ipBytes[] = { 127, 0, 0, 1 };

	sockaddr_in connectAddr;
	connectAddr.sin_family = AF_INET;
	connectAddr.sin_addr.s_addr = *(uint32_t*)ipBytes;
	connectAddr.sin_port = htons(666);

	engInit();

	bool connected = false;

	while (engBeginFrame())
	{
		engSetColor(0x222244FF);
		engClear();

		if (!connected)
		{
			engSetColor(0xFFFFFFFF);
			engText(10, 10, "Press enter to connect!");

			if (engKeyPressed(Key::Return))
			{
				connect(sock, (sockaddr*)&connectAddr, sizeof(connectAddr));
				if (sock != INVALID_SOCKET)
				{
					connected = true;
					CreateThread(nullptr, 0, recvMessages, (void*)sock, 0, nullptr);
				}
			}
		}

		if (engKeyPressed(Key::Space))
		{
			const char* msg = "I hit space! Woasswwwowoww";
			send(sock, msg, strlen(msg), 0);
		}
	}

	return 0;
}
#endif
