#include "stdafx.h"

#pragma comment (lib, "avfilter.lib")
#pragma comment (lib, "avformat.lib")
#pragma comment (lib, "avcodec.lib")
#pragma comment (lib, "avutil.lib")

#include <iostream>
#define __STDC_CONSTANT_MACROS

#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

#include <stdint.h>
#include <inttypes.h>
#include "audioconv.h"

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

using namespace std;

class fileAudioDecoder
{
private:
	AVCodec *codec;
	AVCodecParameters *codecParameters;
	AVCodecContext *codecContext;
	AVFormatContext *formatContext;
	AVPacket *packet;
	AVFrame *frame;
	int finished;

	void cleanUp()
	{
		if (&this->formatContext)
		{
			avformat_close_input(&this->formatContext);
			avformat_free_context(this->formatContext);
		}

		if (this->codecContext)
		{
			avcodec_close(this->codecContext);
			avcodec_free_context(&this->codecContext);
		}

		if (this->frame)
		{
			av_frame_free(&this->frame);
		}

		if (this->packet)
		{
			av_packet_free(&this->packet);
		}
	}

	int getPacket()
	{
		int gotFrame = -1;

		gotFrame = av_read_frame(this->formatContext, this->packet);

		if (gotFrame == AVERROR_EOF)
		{
			this->finished = 1;
			return 1; // Finished
		}
		else if (gotFrame != 0)
		{
			cout << "Error reading frame data." << endl;
			return -1;
		}
	}

	int decodeFrame()
	{
		int gotFrame = -1;

		if (!(this->packet = av_packet_alloc()))
		{
			cout << "Error allocating packet memory." << endl;
			return -1;
		}

		if (!this->frame)
		{
			if (!(this->frame = av_frame_alloc()))
			{
				cout << "Error allocating frame memory." << endl;
				return -1;
			}
		}

		do
		{
			gotFrame = avcodec_receive_frame(this->codecContext, this->frame);

			if (gotFrame == 0)
			{
				break;
			}

			if (gotFrame == AVERROR(EAGAIN))
			{
				gotFrame = getPacket();

				if (gotFrame < 0)
				{
					return gotFrame;
				}

				if (avcodec_send_packet(this->codecContext, this->packet) != 0)
				{
					cout << "Error sending packet to decoder." << endl;
					return -1;
				}

				continue;
			}
			else if (gotFrame = AVERROR_EOF)
			{
				this->finished = 1;
				return 1;
			}
			else
			{
#if _DEBUG
				char errbuf[AV_ERROR_MAX_STRING_SIZE];
				char *err = av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, gotFrame);
				cout << err << endl;
#endif

				cout << "Error receiving decoded frame." << endl;
				return -1;
			}
		} while (gotFrame != 0);

		return 0;
	}

public:
	int init(const char *filePath)
	{
		this->finished = 0;
		this->codecParameters = NULL;
		this->codecContext = avcodec_alloc_context3(NULL);
		this->formatContext = avformat_alloc_context();
		this->packet = av_packet_alloc();
		this->frame = av_frame_alloc();

		if (avformat_open_input(&this->formatContext, filePath, NULL, NULL) != 0)
		{
			cout << "Error opening input file." << endl;
			this->cleanUp();
			return -1;
		}

		if (avformat_find_stream_info(this->formatContext, NULL) < 0)
		{
			cout << "Error finding stream information." << endl;
			this->cleanUp();
			return -1;
		}

#ifdef _DEBUG
		av_dump_format(this->formatContext, 0, filePath, 0);
#endif

		int streamID = -1;

		// Find the first audio stream.
		for (unsigned int i = 0; i < this->formatContext->nb_streams; i++)
		{
			if (this->formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				streamID = i;
				break;
			}
		}

		if (streamID == -1)
		{
			cout << "No audio stream found." << endl;
			this->cleanUp();
			return -1;
		}

		this->codecParameters = this->formatContext->streams[streamID]->codecpar;
		this->codec = avcodec_find_decoder(this->codecParameters->codec_id);

		if (!this->codec)
		{
			cout << "Error finding decoder." << endl;
			this->cleanUp();
			return -1;
		}

		this->codecContext = avcodec_alloc_context3(this->codec);
		
		if (avcodec_parameters_to_context(this->codecContext, this->codecParameters) < 0)
		{
			cout << "Error mapping codec parameters to context." << endl;
			this->cleanUp();
			return -1;
		}

		if (avcodec_open2(this->codecContext, this->codec, NULL) < 0)
		{
			cout << "Error opening decoder." << endl;
			this->cleanUp();
			return -1;
		}

		return 0;
	}

	AVCodecParameters *getCodecParameters()
	{
		AVCodecParameters *copiedParameters = avcodec_parameters_alloc();
		avcodec_parameters_copy(copiedParameters, this->codecParameters);
		
		return copiedParameters;
	}

	AVFrame *getDecodedFrame()
	{
		if (!finished)
		{
			int state = this->decodeFrame();

			if (state != 0)
			{
				this->cleanUp();
			}

			if (state >= 0)
			{
				return this->frame;
			}
		}

		return NULL;
	}
};

class audioEncoder
{
	AVCodec *codec;
	AVCodecContext *codecContext;
	AVCodecParameters *codecParameters;
	AVFrame *frame;
	AVPacket *packet;
	AVFilterGraph *filterGraph;
	AVFilterContext *filterSource, *filterSink;
	int finished;

	void cleanUp()
	{
		if (this->codecContext)
		{
			avcodec_close(this->codecContext);
			avcodec_free_context(&this->codecContext);
		}

		if (this->filterGraph)
		{
			avfilter_free(this->filterSource);
			avfilter_free(this->filterSink);
			avfilter_graph_free(&filterGraph);
		}

		if (this->frame)
		{
			av_frame_free(&this->frame);
		}

		if (this->packet)
		{
			av_packet_free(&this->packet);
		}
	}

	int encodeFrame(AVFrame *frame)
	{
		this->frame = frame;
		int gotPacket = -1;
		int gotFrame = -1;

		if (!(this->packet = av_packet_alloc()))
		{
			cout << "Error allocating packet memory." << endl;
			return -1;
		}

		if (this->frame)
		{
			this->frame->format = this->codecParameters->format;
			this->frame->channels = this->codecParameters->channels;
			this->frame->channel_layout = this->codecParameters->channel_layout;
			this->frame->sample_rate = this->codecParameters->sample_rate;

			if (this->filterGraph)
			{
				if (av_buffersrc_add_frame(this->filterSource, this->frame) < 0)
				{
					// Need more frames.
					return 0;
				}

				gotFrame = av_buffersink_get_frame(this->filterSink, this->frame);

				if (gotFrame == AVERROR(EAGAIN))
				{
					// Need more frames.
					return 0;
				}

				if (!avcodec_is_open(this->codecContext))
				{
					this->codecContext->sample_fmt = (AVSampleFormat)this->frame->format;
					this->codecContext->channel_layout = this->frame->channel_layout;
					this->codecContext->channels = this->frame->channels;
					this->codecContext->sample_rate = this->frame->sample_rate;

					if (avcodec_open2(this->codecContext, this->codec, NULL) < 0)
					{
						cout << "Error opening the codec." << endl;
						this->cleanUp();
						return -1;
					}

#ifdef _DEBUG
					char channelLayout[64];
					av_get_channel_layout_string(channelLayout, sizeof(channelLayout), 0, this->codecContext->channel_layout);
					cout << "Negotiated channel layout: " << channelLayout << endl;
					cout << "Negotiated sample format: " << av_get_sample_fmt_name(this->codecContext->sample_fmt) << endl;
					cout << "Negotiated sample rate: " << this->codecContext->sample_rate << endl;
#endif
				}
			}

			gotFrame = avcodec_send_frame(this->codecContext, this->frame);

			if (gotFrame != 0)
			{
				cout << "Error sending frame to encoder." << endl;
				return -1;
			}
		}

		gotPacket = avcodec_receive_packet(this->codecContext, this->packet);

		if (gotPacket == 0)
		{
			return 0;
		}
		else if (gotPacket == AVERROR(EAGAIN))
		{
			// Need more frames.
			return 0;
		}
		else if (gotPacket = AVERROR_EOF)
		{
			this->finished = 1;
			return 1;
		}
		else
		{
#if _DEBUG
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			char *err = av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, gotPacket);
			cout << err << endl;
#endif

			cout << "Error receiving decoded frame." << endl;
			return -1;
		}
	}

public:
	int init(const char *filePath, AVCodecID codecID, AVCodecParameters *parameters)
	{
		this->finished = 0;
		this->codecContext = avcodec_alloc_context3(NULL);
		this->codecParameters = parameters;
		this->codec = avcodec_find_encoder(codecID);
		this->frame = NULL;
		this->packet = av_packet_alloc();
		this->filterGraph = NULL;
		this->filterSource = NULL;
		this->filterSink = NULL;

		if (!codec)
		{
			cout << "Error finding encoder." << endl;
			this->cleanUp();
			return -1;
		}

		this->codecContext = avcodec_alloc_context3(this->codec);
		this->codecContext->bit_rate = parameters->bit_rate;
		this->codecContext->channels = parameters->channels;
		this->codecContext->channel_layout = parameters->channel_layout;
		this->codecContext->sample_fmt = (AVSampleFormat)parameters->format;
		this->codecContext->sample_rate = parameters->sample_rate;

		const enum AVSampleFormat *format = this->codec->sample_fmts;
		AVSampleFormat requestedFormat = (AVSampleFormat)parameters->format;

		while (*format != AV_SAMPLE_FMT_NONE)
		{
			if (*format == requestedFormat)
			{
				this->codecContext->sample_fmt = requestedFormat;
				break;
			}

			format++;
		}

		if (*format == AV_SAMPLE_FMT_NONE)
		{
			avfilter_register_all();

			AVFilterGraph *filterGraph = avfilter_graph_alloc();

			if (!filterGraph)
			{
				cout << "Error allocating memory for the filter graph." << endl;
				this->cleanUp();
				return -1;
			}

			AVFilter *bufferFilter = avfilter_get_by_name("abuffer");
			AVFilter *formatFilter = avfilter_get_by_name("aformat");
			AVFilter *bufferSinkFilter = avfilter_get_by_name("abuffersink");

			if (!bufferFilter || !formatFilter || !bufferSinkFilter)
			{
				cout << "Error retrieving filter." << endl;
				this->cleanUp();
				return -1;
			}

			AVFilterContext *bufferContext = avfilter_graph_alloc_filter(filterGraph, bufferFilter, "abuffer");
			AVFilterContext *formatContext = avfilter_graph_alloc_filter(filterGraph, formatFilter, "aformat");
			AVFilterContext *bufferSinkContext = avfilter_graph_alloc_filter(filterGraph, bufferSinkFilter, "abuffersink");

			if (!bufferContext || !formatContext || !bufferSinkContext)
			{
				cout << "Error allocating format filter context." << endl;
				this->cleanUp();
				return -1;
			}

			char channelLayout[64];
			av_get_channel_layout_string(channelLayout, sizeof(channelLayout), 0, this->codecParameters->channel_layout);

			av_opt_set(bufferContext, "channel_layout", channelLayout, AV_OPT_SEARCH_CHILDREN);
			av_opt_set_sample_fmt(bufferContext, "sample_fmt", (AVSampleFormat)this->codecParameters->format, AV_OPT_SEARCH_CHILDREN);
			av_opt_set_q(bufferContext, "time_base", AVRational{ 1, this->codecParameters->sample_rate }, AV_OPT_SEARCH_CHILDREN);
			av_opt_set_int(bufferContext, "sample_rate", this->codecParameters->sample_rate, AV_OPT_SEARCH_CHILDREN);

			if (avfilter_init_str(bufferContext, NULL))
			{
				cout << "Error initializing the buffer filter." << endl;
				this->cleanUp();
				return -1;
			}

			char channelLayouts[3072] = { };
			const uint64_t *layout = this->codec->channel_layouts;

			if (layout)
			{
				while (*layout)
				{
					av_get_channel_layout_string(channelLayout, sizeof(channelLayout), 0, *layout);
					sprintf(channelLayouts + strlen(channelLayouts), "%s|", channelLayout);

					layout++;
				}

				// Erase the last separator.
				sprintf(channelLayouts + strlen(channelLayouts) - 1, "\0");
			}
			else
			{
				av_get_channel_layout_string(channelLayout, sizeof(channelLayout), 0, this->codecParameters->channel_layout);
				sprintf(channelLayouts, "%s", channelLayout);
			}

#ifdef _DEBUG
			cout << "Negotiated channel layouts: " << channelLayouts << endl;
#endif

			char sampleFormats[3072] = {};
			const enum AVSampleFormat *sampleFormat = this->codec->sample_fmts;

			if (sampleFormat)
			{
				while (*sampleFormat != -1)
				{
					sprintf(sampleFormats + strlen(sampleFormats), "%s|", av_get_sample_fmt_name(*sampleFormat));

					sampleFormat++;
				}

				// Erase the last separator.
				sprintf(sampleFormats + strlen(sampleFormats) - 1, "\0");
			}
			else
			{
				sprintf(sampleFormats, "%s", av_get_sample_fmt_name((AVSampleFormat)this->codecParameters->format));
			}

#ifdef _DEBUG
			cout << "Negotiated sample formats: " << sampleFormats << endl;
#endif

			char sampleRates[3072] = {};
			const int *sampleRate = this->codec->supported_samplerates;

			if (sampleRate)
			{
				while (*sampleRate)
				{
					sprintf(sampleRates + strlen(sampleRates), "%i|", *sampleRate);

					sampleRate++;
				}

				// Erase the last separator.
				sprintf(sampleRates + strlen(sampleRates) - 1, "\0");
			}
			else
			{
				sprintf(sampleRates, "%i", this->codecParameters->sample_rate);
			}

#ifdef _DEBUG
			cout << "Negotiated sample rates: " << sampleRates << endl;
#endif

			av_opt_set(formatContext, "channel_layouts", channelLayouts, AV_OPT_SEARCH_CHILDREN);
			av_opt_set(formatContext, "sample_fmts", sampleFormats, AV_OPT_SEARCH_CHILDREN);
			av_opt_set_q(formatContext, "time_base", AVRational{ 1, this->codecParameters->sample_rate }, AV_OPT_SEARCH_CHILDREN);
			av_opt_set(formatContext, "sample_rates", sampleRates, AV_OPT_SEARCH_CHILDREN);

			if (avfilter_init_str(formatContext, NULL))
			{
				cout << "Error initializing the format filter." << endl;
				this->cleanUp();
				return -1;
			}

			if (avfilter_init_str(bufferSinkContext, NULL))
			{
				cout << "Error initializing the buffer sink filter." << endl;
				this->cleanUp();
				return -1;
			}

			if (avfilter_link(bufferContext, 0, formatContext, 0) < 0
				|| avfilter_link(formatContext, 0, bufferSinkContext, 0) < 0)
			{
				cout << "Error connecting filters." << endl;
				this->cleanUp();
				return -1;
			}

			if (avfilter_graph_config(filterGraph, NULL) < 0)
			{
				cout << "Error configuring filter graph." << endl;
				this->cleanUp();
				return -1;
			}

			this->filterGraph = filterGraph;
			this->filterSource = bufferContext;
			this->filterSink = bufferSinkContext;
		}
		else
		{
			if (avcodec_open2(this->codecContext, this->codec, NULL) < 0)
			{
				cout << "Error opening the codec." << endl;
				this->cleanUp();
				return -1;
			}
		}

		return 0;
	}

	AVPacket *getEncodedPacket(AVFrame *toEncode)
	{
		if (!finished)
		{
			int state = this->encodeFrame(toEncode);

			if (state != 0)
			{
				this->cleanUp();
			}

			if (state >= 0)
			{
				return this->packet;
			}
		}

		return NULL;
	}
};

int convertAudio(const char *input, const char *output, AVCodecID codecID)
{
	av_register_all();

	fileAudioDecoder *decoder = new fileAudioDecoder();
	audioEncoder *encoder = new audioEncoder();
	AVPacket *outputPacket = new AVPacket;
	FILE *outFile = fopen(output, "wb+");

	if (!outFile)
	{
		cout << "Error opening file for writing." << endl;
		return -1;
	}

	if (!decoder->init(input) == 0)
	{
		cout << "Error reading input file." << endl;
		return -1;
	}

	AVCodecParameters *decodingParameters = decoder->getCodecParameters();
	
	encoder->init(output, codecID, decodingParameters);

	float *samples = NULL;
	long int totalSamples = 0;

	AVFrame *outFrame = decoder->getDecodedFrame();

	if (!outFrame)
	{
		cout << "Error fetching a frame." << endl;
		return -1;
	}

	int backedFrames = 0;

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
			fwrite(outputPacket->data, 1, outputPacket->size, outFile);

			outputPacket = encoder->getEncodedPacket(NULL);
		}

		outFrame = decoder->getDecodedFrame();
	}

	fclose(outFile);

	cout << "Done." << endl;
	return 0;
}
