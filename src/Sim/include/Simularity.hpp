#pragma once

#include "Collection.hpp"

#include <vector>

class Simularity {
public:
	struct Sim {
		bool equal = false;
		int phashDiff = 0;
		float keyPointDiff = 0;
	};
	void addImage(const ImageData& img ) {}
};
