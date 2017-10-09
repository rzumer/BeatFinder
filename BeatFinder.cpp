// BeatFinder.cpp : Defines the entry point for the console application.
//

#include <vector>
#include <iostream>
#include <iterator>
#include <algorithm>
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
		const int spectrumSize = windowSize / 2 + 1;
		const int windowBytes = windowSize;

		PCMDecoder *decoder = new PCMDecoder;
		char *input = argv[1];

		AVPacket *packet = decoder->decodeAudio(input, AV_CODEC_ID_PCM_U8);
		int numSamples = 0;
		int offset = 0;
		vector<uint8_t> samples(packet->data, packet->data + packet->size);
		vector<complex<float>> spectrum(spectrumSize);
		vector<complex<float>> lastSpectrum(spectrumSize);
		vector<float> spectralFlux;
		vector<int> peakWindows;

		while(packet)
		{
			numSamples += packet->size;
			offset = max(numSamples - windowBytes, 0);

			// Append packet contents to the window buffer
			copy(packet->data, packet->data + packet->size - offset, back_inserter(samples));

			while (numSamples >= windowBytes)
			{
				// Convert data to signed float between -1 and 1
				vector<float> floatSamples(windowSize);

				for (int i = 0; i < windowSize; i++)
				{
					float convSample = (int)samples[i] / (numeric_limits<uint8_t>::max() / 2.0) - 1;
					
					floatSamples.push_back(convSample);
				}

				// Do something with the samples
				Eigen::FFT<float> fft;
				fft.fwd(spectrum, floatSamples);

				int spectralDiff = 0;
				for (int i = 0; i < spectrumSize; i++)
				{
					float diff = spectrum[i].real() - lastSpectrum[i].real();
					spectralDiff += fmax(diff, 0);
				}

				spectralFlux.push_back(spectralDiff);
				lastSpectrum = spectrum;

				// Handle remaining bytes
				if (offset > 0)
				{
					// Add the remaining samples back to the window buffer
					samples = vector<uint8_t>(packet->data + packet->size - offset + 1, packet->data + min(packet->size - offset + 1 + windowBytes, packet->size));
					offset = max(offset - windowBytes, 0);
				}

				numSamples = offset;
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

		for (int j = 0; j < min((int)peakWindows.size(), 10); j++)
		{
			double sec = peakWindows[j] * windowSize / 44100.0;
			cout << "Peak at " << sec << "s" << endl;
		}
	}
	else
	{
		cout << "Usage: BeatFinder <input> <output>" << endl;
	}
	
	cout << "Done." << endl;
	getchar();
    return 0;
}
