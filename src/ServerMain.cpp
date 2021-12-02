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

const float fixedTimeStep = 0.016f;
float accumulator = 0;
const float blockMoveSpeed = 300;
const float ballMoveSpeed = 450;
bool gameStarted = false;
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

	while (!userDisconnected)
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

			toClient.playerId = i;
			send(users[i].sock, (char*) &toClient, sizeof(toClient), 0);
		}
		userMutex.unlock();
	}

	closesocket(user->sock);
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

			toClient.playerId = i;
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

	Vector2 ballCenter = Vector2(ballPos.x + BallSize * 0.5f, ballPos.y + BallSize * 0.5f);
	Vector2 leftBlockCenter = Vector2(leftBlock.x + BlockWidth * 0.5f, leftBlock.y + BlockHeight * 0.5f);
	Vector2 rightBlockCenter = Vector2(rightBlock.x + BlockWidth * 0.5f, rightBlock.y + BlockHeight * 0.5f);

	if (ball.x <= leftBlock.x + BlockWidth && ball.y + BallSize >= leftBlock.y && ball.y <= leftBlock.y + BlockHeight)
		ballDirection = (ballCenter - leftBlockCenter).normalized();
	else if (ball.x + BallSize >= rightBlock.x && ball.y + BallSize >= rightBlock.y && ball.y <= leftBlock.y + BlockHeight)
		ballDirection = (ballCenter - rightBlockCenter).normalized();
}

void wallCollisionSolve()
{
	SDL_Rect ball {ballPos.x, ballPos.y, BallSize, BallSize};

	if (ball.x <= 0)
	{
		++rightScore;
		ballPos = Vector2(ResolutionWidth * 0.5f - BallSize * 0.5f, ResolutionHeight * 0.5f - BallSize * 0.5f);
		ballDirection = Vector2(-1, 0);
		return;
	}
	if (ball.x + BallSize >= ResolutionWidth)
	{
		++leftScore;
		ballPos = Vector2(ResolutionWidth * 0.5f - BallSize * 0.5f, ResolutionHeight * 0.5f - BallSize * 0.5f);
		ballDirection = Vector2(1, 0);
		return;
	}

	if (ball.y <= 0)
		ballDirection = Vector2::Reflect(ballDirection, Vector2(0, 1));
	if (ball.y + BallSize >= ResolutionHeight)
		ballDirection = Vector2::Reflect(ballDirection, Vector2(0, -1));
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
	bindAddr.sin_port = htons(Port);

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
			accumulator += engDeltaTime();

			while (accumulator >= fixedTimeStep)
			{
				// Physics loop
				packetMutex.lock();

				moveDirectionMutex.lock();
				leftBlockHeight += leftBlockMoveDirection * (blockMoveSpeed * fixedTimeStep);
				rightBlockHeight += rightBlockMoveDirection * (blockMoveSpeed * fixedTimeStep);
				moveDirectionMutex.unlock();

				clamp(leftBlockHeight, 0, ResolutionHeight - BlockHeight);
				clamp(rightBlockHeight, 0, ResolutionHeight - BlockHeight);

				ballPos += ballDirection * (ballMoveSpeed * fixedTimeStep);

				packetMutex.unlock();

				blockCollisionSolve();
				wallCollisionSolve();

				gameStartMutex.lock();
				gameHasStarted = gameStarted;
				gameStartMutex.unlock();

				accumulator -= fixedTimeStep;
			}
		}

		renderPong(ballPos, leftBlockHeight, rightBlockHeight, leftScore, rightScore);
	}

	return 0;
}
#endif