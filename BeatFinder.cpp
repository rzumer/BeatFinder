// BeatFinder.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "libavcodec/avcodec.h"
#include "audioconv.h"

int main(int argc, char* argv[])
{
	if (argc >= 3)
	{
		convertAudio(argv[1], argv[2], AV_CODEC_ID_PCM_F32BE);
	}

	getchar();
    return 0;
}
