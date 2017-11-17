#pragma comment (lib, "avfilter.lib")
#pragma comment (lib, "avformat.lib")
#pragma comment (lib, "avcodec.lib")
#pragma comment (lib, "avutil.lib")

#include "fileAudioEncoder.h";

using namespace std;

void fileAudioEncoder::cleanUp()
{
	if (this->codecContext)
	{
		avcodec_close(this->codecContext);
		avcodec_free_context(&this->codecContext);
	}

	// Causes access exceptions
	/*if (this->ioContext)
	{
		avio_close(this->ioContext);
		avio_context_free(&this->ioContext);
	}

	if (this->formatContext)
	{
		avformat_free_context(this->formatContext);
	}*/

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

int fileAudioEncoder::encodeFrame(AVFrame *frame)
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

int fileAudioEncoder::init(const char *filePath, const vector<AVCodecID> *codecIDs, AVCodecParameters *parameters)
{
	this->codecID = AV_CODEC_ID_NONE;
	this->finished = 0;
	this->codecParameters = parameters;
	this->frame = NULL;
	this->packet = av_packet_alloc();
	this->filterGraph = NULL;
	this->filterSource = NULL;
	this->filterSink = NULL;

	// Initialize output format context if an output file name was provided.
	if (filePath)
	{
		if (avio_open2(&this->ioContext, filePath, AVIO_FLAG_WRITE, NULL, NULL) < 0)
		{
			cout << "Error opening file for writing." << endl;
			this->cleanUp();
			return -1;
		}

		const size_t fileExtensionLength = 16;
		const size_t fileNameLength = 1024 - fileExtensionLength;
		char *fileName = new char[fileNameLength + fileExtensionLength];
		char *fileExtension = new char[fileExtensionLength];

		_splitpath_s(filePath, NULL, 0, NULL, 0, fileName, fileNameLength, fileExtension, fileExtensionLength);
		sprintf(fileName + strlen(fileName), "%s", fileExtension);

		if (avformat_alloc_output_context2(&this->formatContext, NULL, NULL, fileName) < 0)
		{
			cout << "Error allocating output format context." << endl;
			this->cleanUp();
			return -1;
		}

		this->formatContext->pb = ioContext;

		for (int i = 0; i < codecIDs->size(); i++)
		{
			if (avformat_query_codec(this->formatContext->oformat, codecIDs->at(i), FF_COMPLIANCE_NORMAL))
			{
#ifdef _DEBUG
				cout << "Negotiated codec: " << avcodec_get_name(codecIDs->at(i)) << endl;
#endif
				this->codecID = codecIDs->at(i);
				break;
			}
		}

		if (this->codecID == AV_CODEC_ID_NONE)
		{
			cout << "Unsupported codecs offered for the given output format." << endl;
			this->cleanUp();
			return -1;
		}

		this->formatContext->oformat->audio_codec = this->codecID;
		this->codec = avcodec_find_encoder(this->codecID);
		this->codecContext = avcodec_alloc_context3(this->codec);

		if (this->formatContext->oformat->flags & AVFMT_GLOBALHEADER)
		{
			this->formatContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}

		if (!(this->stream = avformat_new_stream(this->formatContext, this->codec)))
		{
			cout << "Error allocating output stream." << endl;
			this->cleanUp();
			return -1;
		}

		this->codecContext = avcodec_alloc_context3(this->codec);
		this->codecContext->bit_rate = parameters->bit_rate;
		this->codecContext->channels = parameters->channels;
		this->codecContext->channel_layout = parameters->channel_layout;
		this->codecContext->sample_fmt = (AVSampleFormat)parameters->format;
		this->codecContext->sample_rate = parameters->sample_rate;

		avcodec_parameters_from_context(this->stream->codecpar, this->codecContext);

		if (avformat_init_output(this->formatContext, NULL) < 0)
		{
			cout << "Error initializing output format context." << endl;
			this->cleanUp();
			return -1;
		}
	}
	else
	{
		this->codecID = codecIDs->at(0);
		this->codec = avcodec_find_encoder(this->codecID);
		this->codecContext = avcodec_alloc_context3(this->codec);
		this->codecContext->bit_rate = parameters->bit_rate;
		this->codecContext->channels = parameters->channels;
		this->codecContext->channel_layout = parameters->channel_layout;
		this->codecContext->sample_fmt = (AVSampleFormat)parameters->format;
		this->codecContext->sample_rate = parameters->sample_rate;
	}

	if (!codec)
	{
		cout << "Error finding encoder." << endl;
		this->cleanUp();
		return -1;
	}

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

	// If no viable codec was found for encoding, set up a filtergraph for format conversion.
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

		char channelLayouts[3072] = {};
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

int fileAudioEncoder::writeHeader()
{
	if (!avcodec_is_open(this->codecContext))
	{
		cout << "Codec context is closed." << endl;
		return -1;
	}

	if (avformat_write_header(this->formatContext, NULL) < 0)
	{
		cout << "Error writing output file header." << endl;
		return -1;
	}

	return 0;
}

int fileAudioEncoder::writeEncodedPacket(AVPacket *toWrite)
{
	if (!avcodec_is_open(this->codecContext))
	{
		cout << "Codec context is closed." << endl;
		return -1;
	}

	int result = av_interleaved_write_frame(this->formatContext, toWrite);

	if (result < 0)
	{
		cout << "Error writing encoded packet." << endl;
		return -1;
	}

	return result;
}

int fileAudioEncoder::writeTrailer()
{
	if (!avcodec_is_open(this->codecContext))
	{
		cout << "Codec context is closed." << endl;
		return -1;
	}

	if (av_write_trailer(this->formatContext) < 0)
	{
		cout << "Error writing output file trailer." << endl;
		return -1;
	}

	return 0;
}

AVPacket *fileAudioEncoder::getEncodedPacket(AVFrame *toEncode)
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
