#include <ccv.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>


ccv_tld_param_t cld_params = {
	.win_size = {
		15,
		15,
	},
	.level = 5,
	.min_forward_backward_error = 100,
	.min_eigen = 0.025,
	.min_win = 20,
	.interval = 3,
	.shift = 0.1,
	.top_n = 100,
	.rotation = 0,
	.include_overlap = 0.7,
	.exclude_overlap = 0.2,
	.structs = 40,
	.features = 18,
	.validate_set = 0.5,
	.nnc_same = 0.95,
	.nnc_thres = 0.65,
	.nnc_verify = 0.7,
	.nnc_beyond = 0.8,
	.nnc_collect = 0.5,
	.bad_patches = 100,
	.new_deform = 20,
	.track_deform = 10,
	.new_deform_angle = 20,
	.track_deform_angle = 10,
	.new_deform_scale = 0.02,
	.track_deform_scale = 0.02,
	.new_deform_shift = 0.02,
	.track_deform_shift = 0.02,
	.tld_patch_size = 10,
	.tld_grid_sparsity = 10,
};

int main(int argc, char **argv)
{
	assert(argc >= 6);

	ccv_rect_t box = ccv_rect(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
	for(int i=6; i < argc; i++) {
		double v = atof(argv[i]);
		switch(i) {
			case 6:
				cld_params.win_size = ccv_size((int)round(v),(int)round(v));
				break;
			case 7:
				cld_params.level = (int)round(v);
				break;
			case 8:
				cld_params.min_forward_backward_error = v;
				break;
			case 9:
				cld_params.min_eigen = v;
				break;
			case 10:
				cld_params.min_win = (int)round(v);
				break;
			case 11:
				cld_params.interval = (int)round(v);
				break;
			case 12:
				cld_params.shift = v;
				break;
			case 13:
				cld_params.top_n = (int)round(v);
				break;
			case 14:
				cld_params.include_overlap = v;
				break;
			case 15:
				cld_params.exclude_overlap = v;
				break;
			case 16:
				cld_params.structs =  (int)round(v);
				break;
			case 17:
				cld_params.features = (int)round(v);
				break;
			case 18:
				cld_params.validate_set = v;
				break;
			case 19:
				cld_params.nnc_same = v;
				break;
			case 20:
				cld_params.nnc_thres = v;
				break;
			case 21:
				cld_params.nnc_verify = v;
				break;
			case 22:
				cld_params.nnc_beyond = v;
				break;
			case 23:
				cld_params.nnc_collect = v;
				break;
			case 24:
				cld_params.bad_patches = (int)round(v);
				break;
			case 25:
				cld_params.new_deform = (int)round(v);
				break;
			case 26:
				cld_params.track_deform = (int)round(v);
				break;
			case 27:
				cld_params.new_deform_angle = (int)round(v);
				break;
			case 28:
				cld_params.track_deform_angle = (int)round(v);
				break;
			case 29:
				cld_params.new_deform_scale = v;
				break;
			case 30:
				cld_params.track_deform_scale = v;
				break;
			case 31:
				cld_params.new_deform_shift = v;
				break;
			case 32:
				cld_params.track_deform_shift = v;
				break;
			case 33:
				cld_params.tld_patch_size =  (int)round(v);
				break;
			case 34:
				cld_params.tld_grid_sparsity =  (int)round(v);
				break;
		}
	}
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
		ccv_comp_t newbox;
		sws_scale(picture_ctx, pFrame->data, pFrame->linesize, 0, pFrame->height, rgb_picture.data, rgb_picture.linesize);
		ccv_read(rgb_picture.data[0], &y, CCV_IO_RGB_RAW | CCV_IO_GRAY, pCodecContext->height, pCodecContext->width, rgb_picture.linesize[0]);
		if(tld==NULL) {
			tld = ccv_tld_new(y, box, cld_params);
		} else {
			ccv_tld_info_t info;
			newbox = ccv_tld_track_object(tld, x, y, &info);
			if (tld->found)
				printf("%05d: %d %d %d %d %f\n", tld->count, newbox.rect.x, newbox.rect.y, newbox.rect.width, newbox.rect.height, newbox.classification.confidence);
			else
				printf("%05d: --------------\n", tld->count);
			//printf("%04d: performed learn: %d, performed track: %d, successfully track: %d; %d passed fern detector, %d passed nnc detector, %d merged, %d confident matches, %d close matches\n", tld->count, info.perform_learn, info.perform_track, info.track_success, info.ferns_detects, info.nnc_detects, info.clustered_detects, info.confident_matches, info.close_matches);
		}
		/*
		ccv_dense_matrix_t* image = 0;
		ccv_read(rgb_picture.data[0], &image, CCV_IO_RGB_RAW | CCV_IO_RGB_COLOR, pCodecContext->height, pCodecContext->width, rgb_picture.linesize[0]);
		
		if (tld->found)
		{
			ccv_comp_t* comp = &newbox; // (ccv_comp_t*)ccv_array_get(tld->top, i);
			if (comp->rect.x >= 0 && comp->rect.x + comp->rect.width < image->cols &&
				comp->rect.y >= 0 && comp->rect.y + comp->rect.height < image->rows)
			{
				int x, y;
				for (x = comp->rect.x; x < comp->rect.x + comp->rect.width; x++)
				{
					image->data.u8[image->step * comp->rect.y + x * 3] =
					image->data.u8[image->step * (comp->rect.y + comp->rect.height - 1) + x * 3] = 255;
					image->data.u8[image->step * comp->rect.y + x * 3 + 1] =
					image->data.u8[image->step * (comp->rect.y + comp->rect.height - 1) + x * 3 + 1] =
					image->data.u8[image->step * comp->rect.y + x * 3 + 2] =
					image->data.u8[image->step * (comp->rect.y + comp->rect.height - 1) + x * 3 + 2] = 0;
				}
				for (y = comp->rect.y; y < comp->rect.y + comp->rect.height; y++)
				{
					image->data.u8[image->step * y + comp->rect.x * 3] =
					image->data.u8[image->step * y + (comp->rect.x + comp->rect.width - 1) * 3] = 255;
					image->data.u8[image->step * y + comp->rect.x * 3 + 1] =
					image->data.u8[image->step * y + (comp->rect.x + comp->rect.width - 1) * 3 + 1] =
					image->data.u8[image->step * y + comp->rect.x * 3 + 2] =
					image->data.u8[image->step * y + (comp->rect.x + comp->rect.width - 1) * 3 + 2] = 0;
				}
			}
		}
		char filename[1024];
		sprintf(filename, "tld-out/output-%04d.png", tld->count);
		printf("%d %d \n",tld->count,i);
		ccv_write(image, filename, 0, CCV_IO_PNG_FILE, 0);
		ccv_matrix_free(image);
		*/

		
		av_packet_unref(&packet);
		x = y;
		y = 0;
	}

	return 0;
}
