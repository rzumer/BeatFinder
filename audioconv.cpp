#pragma comment (lib, "avutil.lib")

#include <iostream>
#include "audioconv.h"

using namespace std;

AudioTranscoder::AudioTranscoder(const char *input, AVCodecID codecID)
{
	av_register_all();

	decoder = new fileAudioDecoder;
	encoder = new fileAudioEncoder;
	this->inputFilePath = input;
	this->codecID = codecID;
	this->duration = 0.0;
}

int AudioTranscoder::Init()
{
	if (!decoder->init(inputFilePath) == 0)
	{
		cout << "Error reading input file." << endl;
		return -1;
	}

	duration = decoder->getStreamDuration();

	decodingParameters = decoder->getCodecParameters();
	decodingParameters->channel_layout = AV_CH_LAYOUT_MONO;
	decodingParameters->channels = 1;

	if (!encoder->init(NULL, new vector<AVCodecID>{ codecID }, decodingParameters) == 0)
	{
		cout << "Error initializing encoder." << endl;
		return -1;
	}

	this->firstFrame = 1;

	return 0;
}

AVPacket *AudioTranscoder::getPacket()
{
	AVPacket *outputPacket = new AVPacket;
	AVFrame *outFrame = NULL;

	if (!decoder || !encoder)
	{
		cout << "Uninitialized input." << endl;
		return NULL;
	}

	if (firstFrame)
	{
		outFrame = decoder->getDecodedFrame();

		if (!outFrame)
		{
			cout << "Error fetching a frame." << endl;
			return NULL;
		}

		this->firstFrame = 0;
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
