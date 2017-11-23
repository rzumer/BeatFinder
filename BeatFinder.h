#pragma once

#ifndef BEATFINDER_H
#define BEATFINDER_H

#include <vector>

struct BeatInfo
{
	int windowSize;
	int sampleRate;
	std::vector<float> amplitudeEnvelope;
	std::vector<float> spectralFlux;
	std::vector<int> peaks;
};

BeatInfo *FindBeats(const char *inputFileName);

#endif
