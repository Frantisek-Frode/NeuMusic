#include "decoder.h"
#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>


#define failif(cond, ...) if (cond) { fprintf(stderr, __VA_ARGS__); goto FAIL; }

void* decode(void* _args) {
#define FAIL END
	DecoderArgs args = *(DecoderArgs*)_args;

	AVFormatContext *pFormatContext = avformat_alloc_context();
	failif (NULL == pFormatContext, "Decoder ERROR could not allocate memory for Format Context.\n");
#undef FAIL
#define FAIL FREE_FORMAT

	failif (0 > avformat_open_input(&pFormatContext, args.file, NULL, NULL), "Decoder ERROR could not open the file.\n");
#undef FAIL
#define FAIL CLOSE_FILE

	failif (0 > avformat_find_stream_info(pFormatContext, NULL), "Decoder ERROR could not get the stream info.\n");

	const AVCodec *pCodec = NULL;
	AVCodecParameters *pCodecParameters =  NULL;
	int audio_stream_index = -1;

	for (int i = 0; i < pFormatContext->nb_streams; i++)
	{
		AVCodecParameters *pLocalCodecParameters =  NULL;
		pLocalCodecParameters = pFormatContext->streams[i]->codecpar;

		const AVCodec *pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);
		failif (NULL == pLocalCodec, "Decoder ERROR unsupported codec.\n");

		if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
			audio_stream_index = i;
			av_dump_format(pFormatContext, i, args.file, 0);
			pCodec = pLocalCodec;
			pCodecParameters = pLocalCodecParameters;
			break;
		}
	}

	failif (audio_stream_index < 0, "Decoder ERROR file %s does not contain an audio stream.\n", args.file);

	AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
	failif (NULL == pCodecContext, "Decoder ERROR failed to allocated memory for AVCodecContext\n");
#undef FAIL
#define FAIL FREE_CODEC

	failif (0 < avcodec_parameters_to_context(pCodecContext, pCodecParameters),
		"Decoder ERROR failed to copy codec params to codec context\n");
	failif (0 < avcodec_open2(pCodecContext, pCodec, NULL),
		"Decoder ERROR failed to open codec through avcodec_open2\n");

	AVFrame *pFrame = av_frame_alloc();
	failif (NULL == pFrame, "Decoder ERROR failed to allocate memory for AVFrame\n");
#undef FAIL
#define FAIL FREE_FRAME

	AVPacket *pPacket = av_packet_alloc();
	failif (NULL == pPacket, "Decoder ERROR failed to allocate memory for AVPacket\n");
#undef FAIL
#define FAIL FREE_PACKET

	while (0 <= av_read_frame(pFormatContext, pPacket)) {
		if (pPacket->stream_index != audio_stream_index) goto NEXT_PACKET;

		int status = avcodec_send_packet(pCodecContext, pPacket);
		if (status < 0) {
			fprintf(stderr, "Error while sending a packet to the decoder: %s\n", av_err2str(status));
			goto NEXT_PACKET;
		}

		for (;;) {
			int status = avcodec_receive_frame(pCodecContext, pFrame);
			if (status == AVERROR(EAGAIN) || status == AVERROR_EOF) {
				break;
			} else if (status < 0) {
				fprintf(stderr, "Error while receiving a frame from the decoder: %s\n", av_err2str(status));
				goto NEXT_PACKET;
			}

			// printf("Samples %d\n", pFrame->nb_samples);
		}

NEXT_PACKET:
		av_packet_unref(pPacket);
	}

	for (double t = 0; t < args.duration; t += 1 / (double)args.sample_rate) {
		double val = sin(6.28 * t * args.freq) * 0xfff;
		int16_t values[] = {
			(int16_t)val,
			(int16_t)val
		};
		channel_write(args.output, &values, sizeof(values));
	}

	channel_finish_writing(args.output);

FREE_PACKET:
	av_packet_free(&pPacket);
FREE_FRAME:
	av_frame_free(&pFrame);
FREE_CODEC:
	avcodec_free_context(&pCodecContext);
CLOSE_FILE:
	avformat_close_input(&pFormatContext);
FREE_FORMAT:
	avformat_free_context(pFormatContext);
END:
	pthread_exit(NULL);
#undef FAIL
}

