#pragma once

struct Point
{
	Point(unsigned int x, unsigned int y) : x{ x }, y{ y } {};
	unsigned int x, y;
};