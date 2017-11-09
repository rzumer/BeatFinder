#pragma once

#ifndef BEATFINDER_H
#define BEATFINDER_H

#include <vector>

struct BeatInfo
{
	const int windowSize = 1024;
	std::vector<float> spectralFlux;
	std::vector<int> peaks;
};

BeatInfo *FindBeats(const char *inputFileName);

#endif
