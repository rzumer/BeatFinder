// BeatFinder.cpp : Defines the entry point for the console application.
//

#include <vector>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <fstream>
#include <unsupported/Eigen/FFT>
#include "libavcodec/avcodec.h"
#include "audioconv.h"

using namespace std;

int main(int argc, char* argv[])
{
	const vector<AVCodecID> *codecIDs = new vector<AVCodecID> { AV_CODEC_ID_PCM_F32BE, AV_CODEC_ID_PCM_F32LE };

	if (argc >= 3)
	{
		convertAudioFile(argv[1], argv[2], codecIDs);
	}
	else if (argc >= 2)
	{
		const int windowSize = 1024;
		const int windowBytes = windowSize;

		PCMDecoder *decoder = new PCMDecoder;
		char *input = argv[1];

		AVPacket *packet = decoder->decodeAudio(input, AV_CODEC_ID_PCM_U8);
		int numSamples = 0;
		int overflow = 0;
		vector<uint8_t> samples(packet->data, packet->data + packet->size);
		vector<complex<float>> spectrum(windowSize);
		vector<complex<float>> lastSpectrum(windowSize);
		vector<float> spectralFlux;
		vector<int> peakWindows;

		while(packet)
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
				/*if (spectralFlux.size() > 2)
				{
					int peakChecked = spectralFlux.size() - 2;
					if (spectralFlux[peakChecked] > spectralFlux[peakChecked - 1] && spectralFlux[peakChecked] > spectralFlux[peakChecked + 1])
					{
						double sec = peakChecked * windowSize / 44100.0;
						cout << "Found peak at " << sec << "s." << endl;
					}
				}*/

				// Handle remaining bytes
				if (overflow > 0)
				{
					// Add the remaining samples back to the window buffer
					samples = vector<uint8_t>(packet->data + packet->size - overflow, packet->data + min(packet->size - overflow + 1 + windowBytes, packet->size));
					overflow = max(overflow - windowBytes, 0);
				}

				numSamples = max(numSamples - windowBytes, 0);
			}

			packet = decoder->decodeAudio(NULL);
		}

		// Find peak windows from the spectral flux
		for (int i = 0; i < spectralFlux.size(); i++)
		{
			if (i == 0)
			{
				if (spectralFlux[i] > spectralFlux[i + 1])
				{
					peakWindows.push_back(i);
				}
			}
			else if (i == spectralFlux.size() - 1)
			{
				if (spectralFlux[i] > spectralFlux[i - 1])
				{
					peakWindows.push_back(i);
				}
			}
			else
			{
				if (spectralFlux[i] > spectralFlux[i - 1] && spectralFlux[i] > spectralFlux[i + 1])
				{
					peakWindows.push_back(i);
				}
			}
		}

		ofstream stream(strcat(argv[1], "_flux.txt"));
		copy(spectralFlux.begin(), spectralFlux.end(), ostream_iterator<int>(stream, "\n"));

		/*ofstream stream2("C:/out_peaks.txt");
		copy(peakWindows.begin(), peakWindows.end(), ostream_iterator<int>(stream2, "\n"));*/

		/*for (int j = 0; j < min((int)peakWindows.size(), 10); j++)
		{
			double sec = peakWindows[j] * windowSize / 44100.0;
			cout << "Peak at " << sec << "s" << endl;
		}*/

		cout << "Done." << endl;
	}
	else
	{
		cout << "Usage: BeatFinder <input> <output>" << endl;
	}
	
	getchar();
    return 0;
}
