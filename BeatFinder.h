#pragma once

#include <vector>

struct BeatInfo
{
	const int windowSize = 1024;
	std::vector<float> spectralFlux;
	std::vector<int> peaks;
};

BeatInfo FindBeats(char *inputFileName);
