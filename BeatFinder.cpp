// BeatFinder.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include <iterator>
#include <algorithm>
#include <fstream>
#include <numeric>
#include <unsupported/Eigen/FFT>
#include "BeatFinder.h"
#include "libavcodec/avcodec.h"
#include "audioconv.h"

using namespace std;

BeatInfo *FindBeats(const char *inputFileName)
{
	// const vector<AVCodecID> *codecIDs = new vector<AVCodecID>{ AV_CODEC_ID_PCM_F32BE, AV_CODEC_ID_PCM_F32LE };
	const AVCodecID codecID = AV_CODEC_ID_PCM_U8;

	const int windowSize = 1024;
	const int windowBytes = windowSize;
	const int movingAverageSize = 20;
	const double thresholdMultiplier = 1.5;

	AudioTranscoder *transcoder = new AudioTranscoder(inputFileName, codecID);

	if (transcoder->Init() != 0)
	{
		cout << "Error initializing transcoder." << endl;
		return NULL;
	}

	AVPacket *packet = transcoder->getPacket();

	int numSamples = 0;
	int overflow = 0;
	vector<uint8_t> samples;
	vector<complex<float>> spectrum(windowSize);
	vector<complex<float>> lastSpectrum(windowSize);
	vector<float> amplitudeEnvelope;
	vector<float> spectralFlux;
	vector<int> peaks;
	int lastSum = 0;
	int curSum = 0;

	while (packet)
	{
		numSamples += packet->size;
		overflow = max(numSamples - windowBytes, 0);

		// Append packet contents to the window buffer
		copy(packet->data, packet->data + packet->size - overflow, back_inserter(samples));

		while (numSamples >= windowBytes)
		{
			// Convert data to signed float between -1 and 1
			vector<float> floatSamples;

			for (int i = 0; i < windowSize; i++)
			{
				float convSample = (int)samples[i] / (numeric_limits<uint8_t>::max() / 2.0) - 1;
				float hammingCoefficient = 0.54 - 0.46 * cos(2 * M_PI * i / (double)windowSize);

				floatSamples.push_back(convSample * hammingCoefficient);
			}

			vector<float> absFloatSamples = floatSamples;
			for (auto& f : absFloatSamples) { f = f < 0 ? -f : f; }

			amplitudeEnvelope.push_back(1.0 * std::accumulate(absFloatSamples.begin(), absFloatSamples.end(), 0.0f) / absFloatSamples.size());

			Eigen::FFT<float> fft;
			fft.fwd(spectrum, floatSamples);

			int spectralDiff = 0;

			for (int i = 0; i < spectrum.size(); i++)
			{
				float diff = abs(spectrum[i]) - abs(lastSpectrum[i]);
				spectralDiff += fmax(diff, 0);
			}

			spectralFlux.push_back(spectralDiff);
			lastSpectrum = spectrum;

			// Check for a peak
			// TODO get the same results in this method and below.
			float peak = 0;

			int currentWindow = (int)spectralFlux.size() - 1 - movingAverageSize;
			int startWindow = max(0, currentWindow - movingAverageSize);
			int endWindow = min((int)spectralFlux.size() - 1, currentWindow + movingAverageSize);

			if (currentWindow >= 0)
			{
				float sum = 0;
				float threshold = 0;

				for (int j = startWindow; j <= endWindow; j++)
				{
					sum += spectralFlux.at(j);
				}

				threshold = sum / (endWindow - startWindow + 1) * thresholdMultiplier;

				if (threshold <= spectralFlux.at(currentWindow))
				{
					peak = spectralFlux.at(currentWindow) - threshold;

					// Ensure that only peaks are selected.
					if (peaks.size() > 0)
					{
						float lastPeak = peaks.back();

						if (peak < lastPeak)
						{
							peak = 0;
						}
						else
						{
							peaks.pop_back();
							peaks.push_back((float)0);
						}
					}
				}

				peaks.push_back(peak);
			}

			// Handle remaining bytes
			if (overflow > 0)
			{
				// Add the remaining samples back to the window buffer
				samples = vector<uint8_t>(packet->data + packet->size - overflow, packet->data + min(packet->size - overflow + 1 + windowBytes, packet->size));
				overflow = max(overflow - windowBytes, 0);
			}
			else
			{
				samples.clear();
				floatSamples.clear();
			}

			numSamples = max(numSamples - windowBytes, 0);
		}

		packet = transcoder->getPacket();
	}

	// Pad peak vector to the same size as the spectral flux.
	for (int i = 0; i < movingAverageSize; i++)
	{
		peaks.push_back(0);
	}

	// Find peak windows from the spectral flux
	// TODO get the same results in this method and above.
	/*vector<float> threshold;
	for (int i = 0; i < spectralFlux.size(); i++)
	{
		int start = max(0, i - movingAverageSize);
		int end = min((int)spectralFlux.size() - 1, i + movingAverageSize);
		float mean = 0;
		for (int j = start; j <= end; j++)
			mean += spectralFlux.at(j);
		mean /= (end - start);
		threshold.push_back(mean * thresholdMultiplier);
	}

	vector<float> prunedSpectralFlux;
	for (int i = 0; i < threshold.size(); i++)
	{
		if (threshold.at(i) <= spectralFlux.at(i))
		{
			prunedSpectralFlux.push_back(spectralFlux.at(i) - threshold.at(i));
		}
		else
			prunedSpectralFlux.push_back((float)0);
	}

	vector<float> peaks;
	for (int i = 1; i < prunedSpectralFlux.size() - 1; i++)
	{
		if (prunedSpectralFlux.at(i) > prunedSpectralFlux.at(i + 1) && prunedSpectralFlux.at(i) > prunedSpectralFlux.at(i - 1))
		{
			peaks.push_back(prunedSpectralFlux.at(i));
		}
		else
			peaks.push_back((float)0);
	}*/

	/*ofstream stream(strcat(input, "_flux.txt"));
	copy(spectralFlux.begin(), spectralFlux.end(), ostream_iterator<int>(stream, "\n"));

	ofstream stream2(strcat(input, "_peaks.txt"));
	copy(peaks.begin(), peaks.end(), ostream_iterator<float>(stream2, "\n"));*/

	BeatInfo *beatInfo = new BeatInfo;
	beatInfo->windowSize = windowSize;
	beatInfo->sampleRate = transcoder->decodingParameters->sample_rate;
	beatInfo->amplitudeEnvelope = amplitudeEnvelope;
	beatInfo->spectralFlux = spectralFlux;
	beatInfo->peaks = peaks;
	beatInfo->duration = transcoder->duration;

	//cout << "Done." << endl;

	return beatInfo;
}

/*int main(int argc, char* argv[])
{
	if (argc >= 2)
	{
		BeatInfo *beatInfo = FindBeats(argv[1]);

		ofstream stream(strcat(argv[1], "_flux.txt"));
		copy(beatInfo->spectralFlux.begin(), beatInfo->spectralFlux.end(), ostream_iterator<int>(stream, "\n"));

		ofstream stream2(strcat(argv[1], "_peaks.txt"));
		copy(beatInfo->peaks.begin(), beatInfo->peaks.end(), ostream_iterator<float>(stream2, "\n"));
	}
	else
	{
		cout << "Usage: BeatFinder <input>" << endl;
	}

	getchar();
	return 0;
}*/
