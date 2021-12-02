#pragma once
#include "Library.h"
#include "Engine.h"

#define Port 2456

#define ResolutionWidth 800
#define ResolutionHeight 600
#define BlockWidth 10
#define BlockHeight 60
#define BlockWidthMargin 10
#define BallSize 10
#define ScoreCenterMargin 10
#define ScoreTopMargin 5
#define TextWidth 10

float rightBlockX = ResolutionWidth - BlockWidthMargin - BlockWidth;
float leftBlockX = BlockWidthMargin;

struct PacketToServer
{
	bool movingDown;
	bool movingUp;
};

struct PacketToClient
{
	char message[44] { 0 }; // 44 bytes
	Vector2 ballPos; // 8 bytes
	float leftBlockPos; // 4 bytes
	float rightBlockPos; // 4 bytes
	uint8_t leftSideScore; // 1 byte
	uint8_t rightSideScore; // 1 byte
	bool messageSent = false; // 1 byte
	uint8_t playerId = -1;

	PacketToClient() {}

	PacketToClient(Vector2 inBallPos, float inLeftBlockPos, float inRightBlockPos, uint8_t inLeftSideScore, uint8_t inRightSideScore)
	{
		ballPos = inBallPos;
		leftBlockPos = inLeftBlockPos;
		rightBlockPos = inRightBlockPos;
		leftSideScore = inLeftSideScore;
		rightSideScore = inRightSideScore;
	}
};

void renderPong(Vector2 ballPos, float leftBlockPos, float rightBlockPos, uint8_t leftSideScore, uint8_t rightSideScore, uint8_t playerId = -1)
{
	engSetColor(0xFFFFFFFF);
	engFillRect(ballPos.x, ballPos.y, BallSize, BallSize);

	if (playerId == 0)
		engSetColor(0x00FF00FF);
	engFillRect(leftBlockX, leftBlockPos, BlockWidth, BlockHeight);
	engSetColor(0xFFFFFFFF);

	if(playerId == 1)
		engSetColor(0x00FF00FF);
	engFillRect(rightBlockX, rightBlockPos, BlockWidth, BlockHeight);
	engSetColor(0xFFFFFFFF);

	engTextf(ResolutionWidth * 0.5f - TextWidth, ScoreTopMargin, "%i", leftSideScore);
	engTextf(ResolutionWidth * 0.5f, ScoreTopMargin, "%i", rightSideScore);
}