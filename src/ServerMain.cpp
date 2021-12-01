#if SERVER

#include "Engine.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include <mutex>

#define USER_MAX 128
struct User
{
	bool active = false;
	SOCKET sock;
	char username[25];
};
User users[USER_MAX];
std::mutex userMutex;

DWORD recvWorker(void* ptr)
{
	User* user = (User*)ptr;
	char inBuffer[1024];
	char outBuffer[1060];

	while (true)
	{
		int recvSize = recv(user->sock, inBuffer, 1024, 0);
		if (recvSize == SOCKET_ERROR || recvSize == 0)
		{
			char disconnectText[50];
			sprintf_s(disconnectText, 50, "<%s> disconnected.", user->username);
			engPrint(disconnectText);

			userMutex.lock();
			user->active = false;
			for (int i = 0; i < USER_MAX; i++)
			{
				if (!users[i].active)
					continue;

				send(users[i].sock, disconnectText, strlen(disconnectText), 0);
			}
			userMutex.unlock();
			return 0;
		}

		sprintf_s(outBuffer, 1060, "<%s>: %.*s", user->username, recvSize, inBuffer);

		engPrint("%s", outBuffer);

		userMutex.lock();
		for (int i = 0; i < USER_MAX; i++)
		{
			if (!users[i].active)
				continue;

			send(users[i].sock, outBuffer, recvSize + 30, 0);
		}
		userMutex.unlock();
	}
}

DWORD acceptWorker(void* ptr)
{
	SOCKET listenSock = reinterpret_cast<SOCKET>(ptr);
	while (true)
	{
		sockaddr_in acceptAddr;
		int acceptAddrLen = sizeof(acceptAddr);

		SOCKET newUserSock = accept(listenSock, (sockaddr*)&acceptAddr, &acceptAddrLen);

		int user_idx = -1;
		for (int i = 0; i < USER_MAX; ++i)
		{
			if (users[i].active)
				continue;

			user_idx = i;
			break;
		}

		if (user_idx == -1)
		{
			// Ran out of user slots :( Server is full.
			closesocket(newUserSock);
			continue;
		}

		userMutex.lock();
		User* user = &users[user_idx];
		user->active = true;
		user->sock = newUserSock;
		sprintf_s(user->username, 25, "%d.%d.%d.%d:%d",
			acceptAddr.sin_addr.s_net,
			acceptAddr.sin_addr.s_host,
			acceptAddr.sin_addr.s_lh,
			acceptAddr.sin_addr.s_impno,
			ntohs(acceptAddr.sin_port));

		char connectedText[50];
		sprintf_s(connectedText, 50, "<%s> connected!", user->username);

		for (int i = 0; i < USER_MAX; i++)
		{
			if (!users[i].active)
				continue;

			
			send(users[i].sock, connectedText, strlen(connectedText), 0);
		}
		userMutex.unlock();

		engPrint(connectedText);

		// Spin up recvWorker for this user
		CreateThread(
			nullptr,
			0,
			recvWorker,
			user,
			0,
			nullptr
		);
	}
}

int WinMain(HINSTANCE, HINSTANCE, char*, int)
{
	engInit();

	// Create and bind a socket for listening
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSock == INVALID_SOCKET)
	{
		return 1;
	}

	sockaddr_in bindAddr;
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_addr.s_addr = INADDR_ANY;
	bindAddr.sin_port = htons(666);

	if (bind(listenSock, (sockaddr*)&bindAddr, sizeof(bindAddr)))
	{
		return 1;
	}

	listen(listenSock, 10);
	CreateThread(
		nullptr,
		0,
		acceptWorker,
		(void*)listenSock,
		0,
		nullptr
	);

	while (engBeginFrame())
	{
		engSetColor(0x442222FF);
		engClear();
	}
	closesocket(listenSock);

	return 0;
}
#endif
