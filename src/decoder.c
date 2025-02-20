#include "decoder.h"
#include "comain.h"
#include "channel.h"
#include "mpris.h"
#include <libavutil/channel_layout.h>
#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>


int decoder_init(const char* path, DecoderContext* result) {
#define FAIL END

	DecoderContext res;

	AVFormatContext* format_ctx = avformat_alloc_context();
	failif (NULL == format_ctx, "Decoder ERROR could not allocate memory for Format Context.\n");
	res.format_ctx = format_ctx;
#undef FAIL
#define FAIL FREE_FORMAT

	failif (0 > avformat_open_input(&format_ctx, path, NULL, NULL), "Decoder ERROR could not open the file.\n");
#undef FAIL
#define FAIL CLOSE_FILE

	failif (0 > avformat_find_stream_info(format_ctx, NULL), "Decoder ERROR could not get the stream info.\n");

	res.audio_stream = -1;
	for (int i = 0; i < format_ctx->nb_streams; i++)
	{
		AVCodecParameters* codec_params = format_ctx->streams[i]->codecpar;
		if (codec_params->codec_type != AVMEDIA_TYPE_AUDIO) continue;

		res.codec = avcodec_find_decoder(codec_params->codec_id);
		failif (NULL == res.codec, "Decoder ERROR unsupported codec.\n");

		res.codec_params = codec_params;
		res.audio_stream = i;
		break;
	}
	failif (res.audio_stream < 0, "Decoder ERROR file %s does not contain an audio stream.\n", path);

	*result = res;
	return 0;

CLOSE_FILE:
	avformat_close_input(&format_ctx);
FREE_FORMAT:
	avformat_free_context(format_ctx);
END:
	return -1;
#undef FAIL
}

void decoder_free(DecoderContext* ctx) {
	avformat_close_input(&ctx->format_ctx);
	avformat_free_context(ctx->format_ctx);
}

void* decode(void* _args) {
#define FAIL END
	DecoderContext args = *(DecoderContext*)_args;

	// setup codec
	AVCodecContext *codec_ctx = avcodec_alloc_context3(args.codec);
	failif (NULL == codec_ctx, "Decoder ERROR failed to allocated memory for AVCodecContext\n");
#undef FAIL
#define FAIL FREE_CODEC

	failif (0 < avcodec_parameters_to_context(codec_ctx, args.codec_params),
		"Decoder ERROR failed to copy codec params to codec context\n");
	failif (0 < avcodec_open2(codec_ctx, args.codec, NULL),
		"Decoder ERROR failed to open codec through avcodec_open2\n");

	AVFrame *frame = av_frame_alloc();
	failif (NULL == frame, "Decoder ERROR failed to allocate memory for AVFrame\n");
#undef FAIL
#define FAIL FREE_FRAME

	AVPacket *packet = av_packet_alloc();
	failif (NULL == packet, "Decoder ERROR failed to allocate memory for AVPacket\n");
#undef FAIL
#define FAIL FREE_PACKET

	// setup resampler
	struct SwrContext* swr_ctx = swr_alloc();
	failif (!swr_ctx, "Decoder ERROR Could not allocate resampler context\n");
#undef FAIL
#define FAIL FREE_RESAMPLER

	av_opt_set_int(swr_ctx, "in_channel_layout", args.codec_params->channel_layout, 0);
	av_opt_set_int(swr_ctx, "in_sample_rate", args.codec_params->sample_rate, 0);
	av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", args.codec_params->format, 0);

	av_opt_set_int(swr_ctx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(swr_ctx, "out_sample_rate", args.codec_params->sample_rate, 0);
	av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

	failif (0 > swr_init(swr_ctx), "Failed to initialize the resampling context\n");

	// alloc resampling buffer
	int dst_nb_samples = av_rescale_rnd(1024, args.codec_params->sample_rate, args.codec_params->sample_rate, AV_ROUND_UP);
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

	// decode
	while (0 <= av_read_frame(args.format_ctx, packet)) {
		if (packet->stream_index != args.audio_stream) goto NEXT_PACKET;

		int status = avcodec_send_packet(codec_ctx, packet);
		if (status < 0) {
			fprintf(stderr, "Error while sending a packet to the decoder: %s\n", av_err2str(status));
			goto NEXT_PACKET;
		}

		for (;;) {
			int status = avcodec_receive_frame(codec_ctx, frame);
			if (status == AVERROR(EAGAIN) || status == AVERROR_EOF) {
				break;
			} else if (status < 0) {
				fprintf(stderr, "Error while receiving a frame from the decoder: %s\n", av_err2str(status));
				goto NEXT_PACKET;
			}

			// realloc resampling buffer, if needed
			dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, args.codec_params->sample_rate) +
					frame->nb_samples, args.codec_params->sample_rate, args.codec_params->sample_rate, AV_ROUND_UP);
			if (dst_nb_samples > max_dst_nb_samples) {
				av_free(dst_data[0]);
				status = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels,
						dst_nb_samples, AV_SAMPLE_FMT_S16, 0);
				failif (status < 0, "Could not reallocate destination samples\n");

				max_dst_nb_samples = dst_nb_samples;
			}

			// resample
			int samples_per_ch = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)frame->data, frame->nb_samples);
			failif (status < 0, "Error while converting\n");
			av_samples_get_buffer_size(&dst_linesize, dst_nb_channels, samples_per_ch, AV_SAMPLE_FMT_S16, 1);

			// play
			channel_write(args.output, dst_data[0], dst_linesize);

			// check for events
			while (sizeof(PlayerAction) <= channel_available(args.events)) {
				PlayerAction action;
				channel_read(args.events, sizeof(PlayerAction), &action);

				switch (action) {
				case ACTION_STOP:
				case ACTION_NEXT:
				case ACTION_PREV:
					// TODO: rename macro?
					goto FAIL;
				default: break;
				}
			}
		}

NEXT_PACKET:
		av_packet_unref(packet);
	}

	channel_finish_writing(args.output);
	// cleanup
	if (dst_data) av_freep(dst_data[0]);
FREE_DEST:
	av_freep(dst_data);
FREE_RESAMPLER:
	swr_free(&swr_ctx);
FREE_PACKET:
	av_packet_free(&packet);
FREE_FRAME:
	av_frame_free(&frame);
FREE_CODEC:
	avcodec_free_context(&codec_ctx);
END:
	pthread_exit(NULL);
#undef FAIL
}

