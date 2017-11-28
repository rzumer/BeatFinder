#pragma comment (lib, "avfilter.lib")
#pragma comment (lib, "avformat.lib")
#pragma comment (lib, "avcodec.lib")
#pragma comment (lib, "avutil.lib")

#include "fileAudioDecoder.h"

using namespace std;

void fileAudioDecoder::cleanUp()
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

int fileAudioDecoder::getPacket()
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

	return 0;
}

int fileAudioDecoder::decodeFrame()
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

		int skippedPackets = 0;

		if (gotFrame == AVERROR(EAGAIN))
		{
			gotFrame = getPacket();

			if (gotFrame < 0)
			{
				return gotFrame;
			}

			if (avcodec_send_packet(this->codecContext, this->packet) != 0)
			{
				// Skip a number of packets in case of a misread header (e.g. metadata).
				if (++skippedPackets > 10)
				{
					cout << "Error sending packet to decoder." << endl;
					return -1;
				}
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

int fileAudioDecoder::init(const char *filePath)
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

	this->streamID = -1;

	// Find the first audio stream.
	for (unsigned int i = 0; i < this->formatContext->nb_streams; i++)
	{
		if (this->formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			this->streamID = i;
			break;
		}
	}

	if (this->streamID == -1)
	{
		cout << "No audio stream found." << endl;
		this->cleanUp();
		return -1;
	}

	this->codecParameters = this->formatContext->streams[this->streamID]->codecpar;
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

double fileAudioDecoder::getStreamDuration()
{
	if (this->formatContext)
	{
		AVStream *stream = this->formatContext->streams[this->streamID];

		return stream->duration * stream->time_base.num / ((double)stream->time_base.den / 1000);
	}

	return 0.0;
}

AVCodecParameters *fileAudioDecoder::getCodecParameters()
{
	AVCodecParameters *copiedParameters = avcodec_parameters_alloc();
	avcodec_parameters_copy(copiedParameters, this->codecParameters);

	return copiedParameters;
}

AVFrame *fileAudioDecoder::getDecodedFrame()
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
