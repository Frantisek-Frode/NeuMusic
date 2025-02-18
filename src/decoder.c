#include "decoder.h"
#include <libavutil/channel_layout.h>
#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

#define SAMPLE_RATE 44100

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
			// av_dump_format(pFormatContext, i, args.file, 0);
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

	struct SwrContext* swr_ctx = swr_alloc();
	failif (!swr_ctx, "Decoder ERROR Could not allocate resampler context\n");
#undef FAIL
#define FAIL FREE_RESAMPLER

	av_opt_set_int(swr_ctx, "in_channel_layout", pCodecParameters->channel_layout, 0);
	av_opt_set_int(swr_ctx, "in_sample_rate", pCodecParameters->sample_rate, 0);
	av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", pCodecParameters->format, 0);

	// TODO: pass parameters to player
	av_opt_set_int(swr_ctx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(swr_ctx, "out_sample_rate", SAMPLE_RATE, 0);
	av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

	failif (0 > swr_init(swr_ctx), "Failed to initialize the resampling context\n");

	int dst_nb_samples = av_rescale_rnd(1024, SAMPLE_RATE, pCodecParameters->sample_rate, AV_ROUND_UP);
	int max_dst_nb_samples = dst_nb_samples;
	int dst_nb_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);

	uint8_t** dst_data;
	int dst_linesize;
	int status = av_samples_alloc_array_and_samples(
		&dst_data, &dst_linesize, dst_nb_channels,
		dst_nb_samples, AV_SAMPLE_FMT_S16, 0
	);
	failif (status < 0, "Could not allocate destination samples\n");
#undef FAIL
#define FAIL FREE_DEST

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

			/* compute destination number of samples */
			dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, pCodecParameters->sample_rate) +
					pFrame->nb_samples, SAMPLE_RATE, pCodecParameters->sample_rate, AV_ROUND_UP);
			if (dst_nb_samples > max_dst_nb_samples) {
				av_free(dst_data[0]);
				status = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels,
						dst_nb_samples, AV_SAMPLE_FMT_S16, 0);
				failif (status < 0, "Could not reallocate destination samples\n");

				max_dst_nb_samples = dst_nb_samples;
			}

			// convert to destination format
			int samples_per_ch = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)pFrame->data, pFrame->nb_samples);
			failif (status < 0, "Error while converting\n");
			int dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
				samples_per_ch, AV_SAMPLE_FMT_S16, 1);

			channel_write(args.output, dst_data[0], dst_linesize);
		}

NEXT_PACKET:
		av_packet_unref(pPacket);
	}

FREE_DEST:
	if (dst_data) av_freep(dst_data[0]);
	av_freep(dst_data);
FREE_RESAMPLER:
	swr_free(&swr_ctx);
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
	channel_finish_writing(args.output);
	pthread_exit(NULL);
#undef FAIL
}

