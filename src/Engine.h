#pragma once
#include "Key.h"

// Initializes the engine
// call this first
void engInit();

// Shuts down the engine
void engShutdown();

// Starts a frame, used like `while(engBeginFrame())`
bool engBeginFrame();

// *** RENDERING *** //

// Sets color to hex color in the form 0xRRGGBBAA
void engSetColor(unsigned int clr);

// Sets color with each color provided
void engSetColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

// Clears the entire screen to the currently set color
void engClear();

// Renders a hollow rectangle at (x, y) with size (width, height)
void engRect(int x, int y, int width, int height);

// Fills a rectangle at (x, y) with size (width, height)
void engFillRect(int x, int y, int width, int height);

// Renders text (str) at location (x, y)
void engText(int x, int y, const char* str);
void engTextf(int x, int y, const char* format, ...);

// *** DEBUGGING *** //
void engPrint(const char* format, ...);

// *** INPUT *** //

// Returns if given key is currently pressed
bool engKeyDown(Key key);

// Returns if given key was pressed this frame
bool engKeyPressed(Key key);

// Returns if given key was released this frame
bool engKeyReleased(Key key);

// *** TIME *** //
float engDeltaTime();
float engElapsedTime();