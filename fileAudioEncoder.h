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

class fileAudioEncoder
{
private:
	AVCodec *codec;
	AVCodecContext *codecContext;
	AVCodecParameters *codecParameters;
	AVFrame *frame;
	AVPacket *packet;
	AVFilterGraph *filterGraph;
	AVFilterContext *filterSource, *filterSink;
	AVStream *stream;
	AVFormatContext *formatContext;
	AVIOContext *ioContext;
	AVCodecID codecID;
	int finished;

	void cleanUp();
	int encodeFrame(AVFrame *frame);

public:
	int init(const char *filePath, const std::vector<AVCodecID> *codecIDs, AVCodecParameters *parameters);
	int writeHeader();
	int writeEncodedPacket(AVPacket *toWrite);
	int writeTrailer();
	AVPacket *getEncodedPacket(AVFrame *toEncode);
};
