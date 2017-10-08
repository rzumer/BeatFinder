// BeatFinder.cpp : Defines the entry point for the console application.
//

#include <vector>
#include <iostream>
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
	else
	{
		cout << "Usage: BeatFinder <input> <output>" << endl;
	}

	getchar();
    return 0;
}
