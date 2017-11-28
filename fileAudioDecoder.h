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

#include <stdint.h>
#include <inttypes.h>
#include <vector>
#include <iostream>

class fileAudioDecoder
{
private:
	AVCodec *codec;
	AVCodecParameters *codecParameters;
	AVCodecContext *codecContext;
	AVFormatContext *formatContext;
	AVPacket *packet;
	AVFrame *frame;
	int streamID;
	int finished;

	void cleanUp();
	int getPacket();
	int decodeFrame();

public:
	int init(const char *filePath);
	double getStreamDuration();
	AVCodecParameters *getCodecParameters();
	AVFrame *getDecodedFrame();
};
