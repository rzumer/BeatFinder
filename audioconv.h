#include <vector>

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

#pragma once
int convertAudio(const char *input, const char *output, const std::vector<AVCodecID> *codecIDs);
