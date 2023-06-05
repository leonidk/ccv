#include <ccv.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>

int main(int argc, char **argv)
{
	assert(argc == 6);
	ccv_rect_t box = ccv_rect(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
	printf("%05d: %d %d %d %d %f\n", 0, box.x, box.y, box.width, box.height, 1.0f);

	AVFormatContext *fmtCtx = avformat_alloc_context();
	AVPacket packet;
	av_init_packet(&packet); 
	if (avformat_open_input(&fmtCtx, argv[1], NULL, NULL) != 0)
	{
		return -1;
	}
	if (avformat_find_stream_info(fmtCtx, NULL) < 0)
	{
		return -1;
	}
	int videoStreamIndex = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (videoStreamIndex < 0)
	{
		return -1;
	}

	AVCodecParameters *pLocalCodecParameters = fmtCtx->streams[videoStreamIndex]->codecpar;
	AVCodec *pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);
	AVCodecContext *pCodecContext = avcodec_alloc_context3(pLocalCodec);
	avcodec_parameters_to_context(pCodecContext, pLocalCodecParameters);
	avcodec_open2(pCodecContext, pLocalCodec, NULL);
	AVFrame *pFrame = av_frame_alloc();
	AVFrame rgb_picture;
	memset(&rgb_picture, 0, sizeof(AVPicture));
	ccv_enable_default_cache();

	ccv_dense_matrix_t *x = 0;
	ccv_dense_matrix_t *y = 0;
	
	rgb_picture.data[0] = (uint8_t*)ccmalloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24,pLocalCodecParameters->width, pLocalCodecParameters->height,16));
	av_image_fill_arrays(rgb_picture.data, rgb_picture.linesize,rgb_picture.data[0], AV_PIX_FMT_RGB24, pLocalCodecParameters->width, pLocalCodecParameters->height,16);
	struct SwsContext* picture_ctx = sws_getContext(pLocalCodecParameters->width, pLocalCodecParameters->height, pCodecContext->pix_fmt, 
												    pCodecContext->width, pCodecContext->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, 0, 0, 0);

	ccv_tld_t* tld=NULL;
	int ret = 0;
	for (int i = 0; ; i++)
	{
		av_read_frame(fmtCtx, &packet);
		ret = avcodec_send_packet(pCodecContext, &packet);
		//if(ret < 0) continue;
		ret = avcodec_receive_frame(pCodecContext, pFrame);
		if(ret == -11) {
			continue;
		} else if(ret == 0) {

		} else  {
			break;
		}

		sws_scale(picture_ctx, pFrame->data, pFrame->linesize, 0, pFrame->height, rgb_picture.data, rgb_picture.linesize);
		ccv_read(rgb_picture.data[0], &y, CCV_IO_RGB_RAW | CCV_IO_GRAY, pCodecContext->height, pCodecContext->width, rgb_picture.linesize[0]);
		if(tld==NULL) {
			tld = ccv_tld_new(y, box, ccv_tld_default_params);
		} else {
			ccv_tld_info_t info;
			ccv_comp_t newbox = ccv_tld_track_object(tld, x, y, &info);
			if (tld->found)
				printf("%05d: %d %d %d %d %f\n", tld->count, newbox.rect.x, newbox.rect.y, newbox.rect.width, newbox.rect.height, newbox.classification.confidence);
			else
				printf("%05d: --------------\n", tld->count);
		}

		ccv_dense_matrix_t* image = 0;
		ccv_read(rgb_picture.data[0], &image, CCV_IO_RGB_RAW | CCV_IO_RGB_COLOR, pCodecContext->height, pCodecContext->width, rgb_picture.linesize[0]);
		char filename[1024];
		sprintf(filename, "tld-out/output-%04d.png", tld->count);
		ccv_write(image, filename, 0, CCV_IO_PNG_FILE, 0);
		ccv_matrix_free(image);
		av_packet_unref(&packet);
		x = y;
		y = 0;
	}
	/*
	struct SwsContext* picture_ctx = sws_getCachedContext(0, video_st->codec->width, video_st->codec->height, video_st->codec->pix_fmt, video_st->codec->width, video_st->codec->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, 0, 0, 0);

	sws_scale(picture_ctx, (const uint8_t* const*)picture->data, picture->linesize, 0, video_st->codec->height, rgb_picture.data, rgb_picture.linesize);
	ccv_read(rgb_picture.data[0], &x, CCV_IO_RGB_RAW | CCV_IO_GRAY, video_st->codec->height, video_st->codec->width, rgb_picture.linesize[0]);
	ccv_tld_t* tld = ccv_tld_new(x, box, ccv_tld_default_params);
	for (;;)
	{
		got_picture = 0;
		int result = av_read_frame(ic, &packet);
		if (result == AVERROR(EAGAIN))
			continue;
		int ret = 0;
		ret = avcodec_send_packet(video_st->codec, &packet);
		ret = avcodec_receive_frame(video_st->codec, picture);
		if (ret < 0)
			break;
		sws_scale(picture_ctx, (const uint8_t* const*)picture->data, picture->linesize, 0, video_st->codec->height, rgb_picture.data, rgb_picture.linesize);
		ccv_read(rgb_picture.data[0], &y, CCV_IO_RGB_RAW | CCV_IO_GRAY, video_st->codec->height, video_st->codec->width, rgb_picture.linesize[0]);

	}
	ccv_matrix_free(x);
	ccv_tld_free(tld);
	ccfree(rgb_picture.data[0]);
	ccv_disable_cache();*/
	return 0;
}
