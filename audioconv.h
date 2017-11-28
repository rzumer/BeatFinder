#define __STDC_CONSTANT_MACROS

extern "C"
{
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
#include "libavutil/error.h"
#include "libavutil/avutil.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"
#include "libavutil/frame.h"
#include "libavutil/opt.h"
}

#include "fileAudioEncoder.h"
#include "fileAudioDecoder.h"

#pragma once
class AudioTranscoder
{
private:
	fileAudioDecoder *decoder;
	fileAudioEncoder *encoder;
	const char *inputFilePath;
	AVCodecID codecID;
	int firstFrame;

public:
	AVCodecParameters *decodingParameters;
	double duration;
	AudioTranscoder(const char *input, AVCodecID codecID = AV_CODEC_ID_NONE);
	int Init();
	AVPacket *getPacket();
};

int convertAudioFile(const char *input, const char *output, const std::vector<AVCodecID> *codecIDs);
