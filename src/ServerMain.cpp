#if SERVER

#include "Engine.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include <mutex>
#include "Library.h"
#include "PongDefinitions.h"
#include <SDL/SDL.h>

#define USER_MAX 2

struct User
{
	bool active = false;
	SOCKET sock;
	char username[25];
};

User users[USER_MAX];
std::mutex userMutex;

const float blockMoveSpeed = 20;
const float ballMoveSpeed = 60;
bool gameStarted = true;
std::mutex gameStartMutex;

Vector2 ballPos = Vector2(ResolutionWidth * 0.5f - BallSize * 0.5f, ResolutionHeight * 0.5f - BallSize * 0.5f);
Vector2 ballDirection = Vector2(1, 0);
float leftBlockHeight = ResolutionHeight * 0.5f - BlockHeight * 0.5f;
float rightBlockHeight = ResolutionHeight * 0.5f - BlockHeight * 0.5f;
int8_t leftBlockMoveDirection = 0;
int8_t rightBlockMoveDirection = 0;
uint8_t leftScore = 0;
uint8_t rightScore = 0;

std::mutex packetMutex;
std::mutex moveDirectionMutex;

DWORD recvWorker(void* ptr)
{
	User* user = (User*)ptr;
	PacketToServer packet;
	bool userDisconnected = false;
	char disconnectText[44];

	uint8_t index = -1;
	for (int i = 0; i < USER_MAX; i++)
	{
		if (users[i].sock == user->sock)
		{
			index = i;
			break;
		}
	}

	if (index == -1)
	{
		engPrint("user index was -1, this should never happen");
		return -1;
	}

	while (true)
	{
		int recvSize = recv(user->sock, (char*) &packet, sizeof(packet), 0);
		if (recvSize == SOCKET_ERROR || recvSize == 0)
		{
			sprintf_s(disconnectText, 44, "<%s> disconnected.", user->username);
			engPrint(disconnectText);
			userDisconnected = true;
			userMutex.lock();
			user->active = false;
			userMutex.unlock();

			gameStartMutex.lock();
			gameStarted = false;
			gameStartMutex.unlock();
		}

		int8_t* moveDirection;
		if (index == 0) moveDirection = &leftBlockMoveDirection;
		else moveDirection = &rightBlockMoveDirection;

		int8_t tempDirection = 0;
		tempDirection -= packet.movingUp ? 1 : 0;
		tempDirection += packet.movingDown ? 1 : 0;

		moveDirectionMutex.lock();
		*moveDirection = tempDirection;
		moveDirectionMutex.unlock();

		packetMutex.lock();
		PacketToClient toClient = PacketToClient(ballPos, leftBlockHeight, rightBlockHeight, leftScore, rightScore);
		if (userDisconnected)
		{
			toClient.messageSent = true;
			memcpy(toClient.message, disconnectText, sizeof(char) * 44);
		}
		packetMutex.unlock();

		userMutex.lock();
		for (int i = 0; i < USER_MAX; i++)
		{
			if (!users[i].active)
				continue;

			send(users[i].sock, (char*) &toClient, sizeof(toClient), 0);
		}
		userMutex.unlock();
	}

	return 0;
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

		char connectedText[44];
		sprintf_s(connectedText, 44, "<%s> connected!", user->username);

		packetMutex.lock();
		PacketToClient toClient = PacketToClient(ballPos, leftBlockHeight, rightBlockHeight, leftScore, rightScore);
		toClient.messageSent = true;
		memcpy(toClient.message, connectedText, sizeof(char) * 44);
		packetMutex.unlock();

		int amountOfUsers = 0;
		for (int i = 0; i < USER_MAX; i++)
		{
			if (!users[i].active)
				continue;

			send(users[i].sock, (char*) &toClient, sizeof(toClient), 0);
			++amountOfUsers;
		}
		userMutex.unlock();

		if (amountOfUsers == 2)
		{
			gameStartMutex.lock();
			gameStarted = true;
			gameStartMutex.unlock();
		}

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

void blockCollisionSolve()
{
	SDL_Rect ball {ballPos.x, ballPos.y, BallSize, BallSize};
	SDL_Rect leftBlock {leftBlockX, leftBlockHeight, BlockWidth, BlockHeight};
	SDL_Rect rightBlock {rightBlockX, rightBlockHeight, BlockWidth, BlockHeight};

	if (ball.x <= leftBlock.x + BlockWidth && ball.y + BallSize >= leftBlock.y && ball.y <= leftBlock.y + BlockHeight)
		ballDirection = Vector2::Reflect(ballDirection, Vector2(1, 0));
	else if (ball.x + BallSize >= rightBlock.x && ball.y + BallSize >= rightBlock.y && ball.y <= leftBlock.y + BlockHeight)
		ballDirection = Vector2::Reflect(ballDirection, Vector2(-1, 0));
}

void wallCollisionSolve()
{
	SDL_Rect ball {ballPos.x, ballPos.y, BallSize, BallSize};

	if (ball.x <= 0)
	{
		++rightScore;
		ballPos = Vector2(ResolutionWidth * 0.5f - BallSize * 0.5f, ResolutionHeight * 0.5f - BallSize * 0.5f);
		return;
	}
	if (ball.x + BallSize >= ResolutionWidth)
	{
		++leftScore;
		ballPos = Vector2(ResolutionWidth * 0.5f - BallSize * 0.5f, ResolutionHeight * 0.5f - BallSize * 0.5f);
		return;
	}

	if (ball.y <= 0)
		Vector2::Reflect(ballDirection, Vector2(0, 1));
	else if (ball.y + BallSize >= ResolutionHeight)
		Vector2::Reflect(ballDirection, Vector2(0, -1));
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

		gameStartMutex.lock();
		bool gameHasStarted = gameStarted;
		gameStartMutex.unlock();

		if (gameHasStarted)
		{
			// Physics loop
			packetMutex.lock();

			moveDirectionMutex.lock();
			leftBlockHeight += leftBlockMoveDirection * (blockMoveSpeed * engDeltaTime());
			rightBlockHeight += rightBlockMoveDirection * (blockMoveSpeed * engDeltaTime());
			moveDirectionMutex.unlock();

			clamp(leftBlockHeight, 0, ResolutionHeight - BlockHeight);
			clamp(rightBlockHeight, 0, ResolutionHeight - BlockHeight);

			ballPos += ballDirection * (ballMoveSpeed * engDeltaTime());

			packetMutex.unlock();

			blockCollisionSolve();
			wallCollisionSolve();
		}

		renderPong(ballPos, leftBlockHeight, rightBlockHeight, leftScore, rightScore);
	}

	return 0;
}
#endif