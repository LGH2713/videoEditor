#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_fileSelect_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(
                this,
                tr("open a file"),
                "E:/",
                tr("video files(*.avi *.mp4 *.wmv *.h265 *.mkv);;All files(*.*)"));
    ui->fileName->setText(fileName);
}


void MainWindow::on_playButton_clicked()
{
    AVFormatContext* p_fmt_ctx = nullptr;
    AVCodecContext* p_codec_ctx = nullptr;
    AVCodecParameters* p_codec_par = nullptr;
    AVCodec const * p_codec = nullptr;
    AVFrame* p_frm_raw = nullptr;
    AVFrame* p_frm_yuv = nullptr;
    AVPacket* p_packet = nullptr;
    struct SwsContext* sws_ctx = nullptr;
    int v_idx = -1;
    int ret;
    SDL_Window* screen;
    SDL_Renderer* sdl_renderer;
    SDL_Texture* sdl_texture;
    SDL_Rect sdl_rect;

    std::string fileName = ui->fileName->text().toStdString();
    ret = avformat_open_input(&p_fmt_ctx, fileName.c_str(), nullptr, nullptr);
    if(ret != 0) {
        qDebug() << "avformat_open_input() failed!";
        return;
    }

    ret = avformat_find_stream_info(p_fmt_ctx, nullptr);
    if(ret < 0) {
        qDebug() << "avformat_find_stream_info() failed!";
        return;
    }

    av_dump_format(p_fmt_ctx, 0, fileName.c_str(), 0);

    for(int i = 0; i < p_fmt_ctx->nb_streams; ++i) {
        if(p_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            v_idx = i;
            qDebug() << "Find a audio stream, index " << v_idx;
            break;
        }
    }
    if(v_idx == -1) {
        qDebug() << "Cann't find a video stream";
        return ;
    }

    p_codec_par = p_fmt_ctx->streams[v_idx]->codecpar;
    p_codec = avcodec_find_decoder(p_codec_par->codec_id);
    if(p_codec == nullptr) {
        qDebug() << "Cann't find codec!";
        return ;
    }

    p_codec_ctx = avcodec_alloc_context3(p_codec);

    ret = avcodec_parameters_to_context(p_codec_ctx, p_codec_par);
    if(ret < 0) {
        qDebug() << "avcodec_parameters_to_context() failed " << ret;
        return ;
    }

    ret = avcodec_open2(p_codec_ctx, p_codec, nullptr);
    if(ret < 0) {
        qDebug() << "avcodec_open2() failed" << ret;
        return ;
    }

    p_frm_raw = av_frame_alloc();
    p_frm_yuv = av_frame_alloc();

    int buf_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P,
                                            p_codec_ctx->width,
                                            p_codec_ctx->height,
                                            1
                                            );

    uint8_t* buffer = (uint8_t *)av_malloc(buf_size);
    av_image_fill_arrays(p_frm_yuv->data,
                         p_frm_yuv->linesize,
                         buffer,
                         AV_PIX_FMT_YUV420P,
                         p_codec_ctx->width,
                         p_codec_ctx->height,
                         1
                         );

    sws_ctx = sws_getContext(p_codec_ctx->width,
                             p_codec_ctx->height,
                             p_codec_ctx->pix_fmt,
                             p_codec_ctx->width,
                             p_codec_ctx->height,
                             AV_PIX_FMT_YUV420P,
                             SWS_BICUBIC,
                             nullptr,
                             nullptr,
                             nullptr);

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        qDebug() << "SDL_Init() failed: " << SDL_GetError();
        return ;
    }

//    SDL2嵌入qt
    screen = SDL_CreateWindowFrom((void *)ui->player->winId());

//    播放界面大小调整
    ui->player->resize(p_codec_ctx->width, p_codec_ctx->height);

    if(screen == nullptr) {
        qDebug() << "SDL_CreateWindow() failed: " << SDL_GetError();
        return;
    }

    sdl_renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED);


    sdl_texture = SDL_CreateTexture(sdl_renderer,
                                    SDL_PIXELFORMAT_IYUV,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    p_codec_ctx->width,
                                    p_codec_ctx->height);

    sdl_rect.x = 0;
    sdl_rect.y = 0;;
    sdl_rect.w = p_codec_ctx->width;
    sdl_rect.h = p_codec_ctx->height;

    p_packet = (AVPacket *)av_malloc(sizeof(AVPacket));

    while(av_read_frame(p_fmt_ctx, p_packet) == 0) {
        if(p_packet->stream_index == v_idx) {
            ret = avcodec_send_packet(p_codec_ctx, p_packet);
            if(ret != 0) {
                qDebug() << "avcodec_send_packet() failed" << ret;
                return;
            }

            ret = avcodec_receive_frame(p_codec_ctx, p_frm_raw);
            if(ret != 0) {
                qDebug() << "avcodec_receive_frame() failed" << ret;
                return;
            }

            sws_scale(sws_ctx,
                      (const uint8_t* const *) p_frm_raw->data,
                      p_frm_raw->linesize,
                      0,
                      p_codec_ctx->height,
                      p_frm_yuv->data,
                      p_frm_yuv->linesize);

            SDL_UpdateYUVTexture(sdl_texture,
                                 &sdl_rect,
                                 p_frm_yuv->data[0],
                                 p_frm_yuv->linesize[0],
                                 p_frm_yuv->data[1],
                                 p_frm_yuv->linesize[1],
                                 p_frm_yuv->data[2],
                                 p_frm_yuv->linesize[2]);

            SDL_RenderClear(sdl_renderer);

            SDL_RenderCopy(sdl_renderer, sdl_texture, nullptr, &sdl_rect);

            SDL_RenderPresent(sdl_renderer);

            SDL_Delay(40);
        }
        av_packet_unref(p_packet);
    }
    SDL_Quit();
    sws_freeContext(sws_ctx);
    av_free(buffer);
    av_frame_free(&p_frm_yuv);
    av_frame_free(&p_frm_raw);
    avcodec_close(p_codec_ctx);
    avformat_close_input(&p_fmt_ctx);

    return;
}

