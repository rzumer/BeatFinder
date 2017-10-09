#pragma comment (lib, "avutil.lib")

#include <iostream>
#include "audioconv.h"

using namespace std;

AVPacket *PCMDecoder::decodeAudio(const char *input, AVCodecID codecID)
{
	AVPacket *outputPacket = new AVPacket;
	AVFrame *outFrame = NULL;

	if (!input)
	{
		if (!decoder || !encoder)
		{
			cout << "Uninitialized input." << endl;
			return NULL;
		}

		do
		{
			outputPacket = encoder->getEncodedPacket(outFrame);

			if (outputPacket == NULL)
			{
				cout << "Error getting encoded packet." << endl;
				return NULL;
			}

			if (outputPacket->size > 0)
			{
				return outputPacket;
			}

			outFrame = decoder->getDecodedFrame();
		} while (outFrame);

		return NULL;
	}

	av_register_all();

	decoder = new fileAudioDecoder;
	encoder = new fileAudioEncoder;

	if (!decoder->init(input) == 0)
	{
		cout << "Error reading input file." << endl;
		return NULL;
	}

	AVCodecParameters *decodingParameters = decoder->getCodecParameters();
	decodingParameters->channel_layout = AV_CH_LAYOUT_MONO;
	decodingParameters->channels = 1;

	if (!encoder->init(NULL, new vector<AVCodecID>{ codecID }, decodingParameters) == 0)
	{
		cout << "Error initializing encoder." << endl;
		return NULL;
	}

	outFrame = decoder->getDecodedFrame();

	if (!outFrame)
	{
		cout << "Error fetching a frame." << endl;
		return NULL;
	}

	while (outFrame != NULL)
	{
		outputPacket = encoder->getEncodedPacket(outFrame);

		if (outputPacket == NULL)
		{
			cout << "Error getting encoded packet." << endl;
			return NULL;
		}

		if (outputPacket->size > 0)
		{
			return outputPacket;
		}

		outFrame = decoder->getDecodedFrame();
	}

	return NULL;
}

int convertAudioFile(const char *input, const char *output, const vector<AVCodecID> *codecIDs)
{
	av_register_all();

	fileAudioDecoder *decoder = new fileAudioDecoder();
	fileAudioEncoder *encoder = new fileAudioEncoder();
	AVPacket *outputPacket = new AVPacket;

	if (!decoder->init(input) == 0)
	{
		cout << "Error reading input file." << endl;
		return -1;
	}

	AVCodecParameters *decodingParameters = decoder->getCodecParameters();
	
	if (!encoder->init(output, codecIDs, decodingParameters) == 0)
	{
		cout << "Error initializing encoder." << endl;
		return -1;
	}

	AVFrame *outFrame = decoder->getDecodedFrame();

	if (!outFrame)
	{
		cout << "Error fetching a frame." << endl;
		return -1;
	}

	int headerWritten = 0;

	while (outFrame != NULL)
	{
		outputPacket = encoder->getEncodedPacket(outFrame);

		if (outputPacket == NULL)
		{
			cout << "Error getting encoded packet." << endl;
			return -1;
		}

		while (outputPacket->size > 0)
		{
			if (!headerWritten)
			{
				if (encoder->writeHeader() < 0)
				{
					cout << "Error writing to output file." << endl;
					return -1;
				}

				headerWritten = 1;
			}

			if (encoder->writeEncodedPacket(outputPacket) < 0)
			{
				cout << "Error writing to output file." << endl;
				return -1;
			}

			outputPacket = encoder->getEncodedPacket(NULL);
		}

		outFrame = decoder->getDecodedFrame();
	}

	if (encoder->writeTrailer() < 0)
	{
		cout << "Error writing to output file." << endl;
		return -1;
	}

	cout << "Done." << endl;
	return 0;
}
