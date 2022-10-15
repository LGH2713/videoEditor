#include "video.h"

Video::Video()
{

}

int Video::queue_picture(PlayerStat *is, AVFrame *src_frame, double pts, double duration, int64_t pos)
{
    Frame *vp;
    if(!(vp = FrameQueue::frame_queue_peek_writable(&is->video_frm_queue)))
        return -1;

    vp->sar = src_frame->sample_aspect_ratio;
    vp->uploaded = 0;

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
    vp->pos = pos;

    // 将AVFrame拷入队列相应位置
    av_frame_move_ref(vp->frame, src_frame);
    // 更新队列计数及写索引
    FrameQueue::frame_queue_push(&is->video_frm_queue);

    return 0;
}

int Video::video_decode_frame(AVCodecContext *p_codec_ctx, PacketQueue *p_pkt_queue, AVFrame *frame)
{
    int ret;

    while(1)
    {
        std::cout << "video_decode_frame first loop" << std::endl;
        AVPacket pkt;
        while(1)
        {
            std::cout << "video_decode_frame second loop" << std::endl;
            // 3.从解码器接收frame
            // 3.1 一个视频packet含一个视频frame
            //      解码器缓存一定数量的packet后，才有解码后的frame输出
            //      frame输出顺序是按pts的顺序，如IBBPBBBP
            //      frame->pst_pos变量是此frame对应的packet在视频文件中的偏移地址，值同pkt.pos
            ret = avcodec_receive_frame(p_codec_ctx, frame);
            if(ret < 0)
            {
                if(ret == AVERROR_EOF)
                {
                    av_log(nullptr, AV_LOG_INFO, "video avcodec_receive_frame(): the decoder has been fully flushed\n");
                    avcodec_flush_buffers(p_codec_ctx); // 重置内部编解码器状态/刷新内部缓冲区
                    return 0;
                }
                else if(ret == AVERROR(EAGAIN))
                {
                    av_log(nullptr, AV_LOG_INFO, "video avcodec_receive_frame(): output is not available in this state - user must try to send new input\n");
                    break;
                }
                else
                {
                    av_log(nullptr, AV_LOG_ERROR, "video avcodec_receive_frame(): other errors\n");
                    continue;
                }
            }
            else
            {
                frame->pts = frame->best_effort_timestamp; // 帧时间戳估计使用各种启发式，在流时间基

                return 1; // 成功解码一个视频帧或音频帧则返回
            }
        }

        // 1.取出一个packet。使用pkt对应的serial赋值给d->pkt_serial
        if(PacketQueue::packet_queue_get(p_pkt_queue, &pkt, true) < 0)
        {
            return -1;
        }

        if(pkt.data == nullptr) // 取出的数据包数据不为空
        {
            std::cout << "video_decode_frame flush buffer" << std::endl;
            avcodec_flush_buffers(p_codec_ctx); // 重置内部编解码器状态/刷新内部缓冲区
        }
        else
        {
            // 2.将packet发送给解码器
            //      发送packet的顺序是按dts递增的顺序，如IPBBPBB
            //      pkt,pos变量可以标识当前packet在视频文件中的地址偏移
            if(avcodec_send_packet(p_codec_ctx, &pkt) == AVERROR(EAGAIN))
            {
                av_log(nullptr, AV_LOG_ERROR, "receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
            }

            std::cout << "send packet" << std::endl;

            av_packet_unref(&pkt);
        }
    }
}

int Video::video_decode_thread(void *arg)
{
    PlayerStat *is = static_cast<PlayerStat *>(arg);
    AVFrame *p_frame = av_frame_alloc();
    double pts;
    double duration;
    int ret;
    int got_picture;
    AVRational tb = is->p_video_stream->time_base;
    AVRational frame_rate = av_guess_frame_rate(is->p_fmt_ctx, is->p_video_stream, nullptr);

    if(p_frame == nullptr) // 帧分配空间失败
    {
        av_log(nullptr, AV_LOG_ERROR, "av_frame_alloc() for p_frame failed\n");
        return AVERROR(ENOMEM);
    }

    while(1)
    {
        got_picture = video_decode_frame(is->p_vcodec_ctx, &is->video_pkt_queue, p_frame); // 解析视频帧
        if(got_picture < 0)
        {
            av_frame_unref(p_frame);
            return 0;
        }

        duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){frame_rate.den, frame_rate.num}) : 0); // 当前帧播放时长
        pts = (p_frame->pts == AV_NOPTS_VALUE) ? NAN : p_frame->pts * av_q2d(tb); // 当前帧显示时间戳
        ret = queue_picture(is, p_frame, pts, duration, p_frame->pkt_pos); // 将当前帧压入frame_queue
        av_frame_unref(p_frame);

        if(ret < 0)
        {
            av_frame_unref(p_frame);
            return 0;
        }
    }
}

// 职责：
// 1. 返回达成同步条件的视频帧播放延迟时间
double Video::compute_target_delay(double delay, PlayerStat *is)
{
    double sync_threshold /*同步域*/, diff = 0;

    // 视频时钟与同步时钟（如音频时钟）的差异，时钟值是上一帧pts值（实为： 上一帧pts + 上一帧至今流逝的时间差）
    diff = PlayerClock::get_clock(&is->video_clk) - PlayerClock::get_clock(&is->audio_clk); // diff可能是正值也可以是负值
    // delay是上一帧播放时长：当前帧（待播放的帧）播放时间与上一帧播放时间的理论差值
    // diff是视频时钟与同步时钟的差值

    // delay < AV_SYNC_THRESHOLD_MIN，则同步域为AV_SYNC_THRESHOLD_MIN
    // delay > AV_SYNC_THRESHOLD_MAX，则同步域为AV_SYNC_THRESHOLD_MAX
    // AV_SYNC_THRESHOLD_MIN < delay < AV_SYNC_THRESHOLD_MAX，则同步域为delay
    sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay)); // 依据上面公式求同步域
    if(!isnan(diff)) {
        if(diff <= -sync_threshold)     // 视频时钟落后于同步时钟，且超过同步域值
            delay = FFMAX(0, delay + diff); // 当前帧播放时刻落后于同步时钟（delay + diff < 0）则delay = 0（视频追赶，立即播放），否则delay = delay + diff
        else if(diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD) // 视频时钟超前于同步时钟，且超过同步域值，但上一帧播放时长超长
            delay = delay + diff;               // 仅仅校正为delay = delay + diff，主要是AV_SYNC_FRAMEUP_THRESHOLD参数的作用
        else if(diff >= sync_threshold)         // 视频时钟超前于同步时钟，且超过同步域值
            delay = 2 * delay;                  // 视频播放要放慢脚步，delay扩大两倍
    }

    av_log(nullptr, AV_LOG_TRACE, "video: delay = %0.3f A-V=%f\n", delay, -diff);

    return delay;
}

// 职责：
// 1. 计算上一帧需要播放的时间
// ps：若上一帧的播放时间早于下一帧的播放时间，则为正常情况，故返回两帧之间的时间差值作为上一帧的播放时长
//     若上一帧的播放时间晚于下一帧的播放时间，则为非常情况，故返回上一帧原本的播放时长
double Video::vp_duration(PlayerStat *is, Frame *vp, Frame *nextvp)
{
    if(vp->serial == nextvp->serial) // 当前帧与下一帧的序列相同
    {
        double duration = nextvp->pts - vp->pts; // 计算出两帧的时间差
        if(isnan(duration) || duration <= 0) // 计算值为无理数或下一帧的播放时间早于上一帧的播放时间
            return vp->duration; // 返回当前帧的持续时间
        else
            return duration; // 返回两帧差值
    }
    else
    {
        return 0.0; // 序列不同则返回0.0
    }
}

void Video::update_video_pts(PlayerStat *is, double pts, int64_t pos, int serial)
{
    PlayerClock::set_clock(&is->video_clk, pts, serial);        // 更新video clock
}

// 职责：
// 1. 读取一个帧
// 2. 对读取帧进行图像转换
// 3. 使用yuv格式更新图像
// 4. 以指定颜色清空渲染目标
// 5. 使用转换后的图像更新渲染目标
// 6. 执行渲染
void Video::video_display(PlayerStat *is)
{
    Frame *vp;
    vp = FrameQueue::frame_queue_peek_last(&is->video_frm_queue); // 返回当前读索引指向的帧

    // 图像转换：p_frm_raw->data ==> p_frm_yuv->data
    // 将源图像中一片连续的区域经过处理后更新到目标体香对应区域，处理的图像区域必须逐行连续
    // plane：如YUV有Y、U、V三个plane，RGB有R、G、B三个plane
    // slice：图像中一片连续的行、必须是连续的，顺序由顶部到底部或由底部到顶部
    // stride/pitch: 一行图像所占的字节数，Strid = BytesPrePixel * width + padding，注意对齐
    // AVFrame.*data[]: 每个数组元素表示对应plane中一行图像所占的字节数
    sws_scale(is->img_convert_ctx,                                      //sws context
              static_cast<const uint8_t * const *>(vp->frame->data),    // src slice
              vp->frame->linesize,                                      // src sride
              0,                                                        // src slice y
              is->p_vcodec_ctx->height,                                 // src slice height
              is->p_frm_yuv->data,                                      // dst planes
              is->p_frm_yuv->linesize                                   // dst stride
              );


    // 使用新的YUV像素数据更新SDL_Rect
    SDL_UpdateYUVTexture(is->sdl_video.texture,     // sdl texture
                         &is->sdl_video.rect,       // sdl rect
                         is->p_frm_yuv->data[0],    // y plane
            is->p_frm_yuv->linesize[0],             // y pitch
            is->p_frm_yuv->data[1],                 // u plane
            is->p_frm_yuv->linesize[1],             // u pitch
            is->p_frm_yuv->data[2],                 // v plane
            is->p_frm_yuv->linesize[2]              // v pitch
            );

    // 使用特定颜色清空当前渲染目标
    SDL_RenderClear(is->sdl_video.renderer);
    // 使用部分图像数据（texture）更新当前渲染目标
    SDL_RenderCopy(is->sdl_video.renderer,          // sdl renderer
                   is->sdl_video.texture,           // sdl texture
                   nullptr,                         // src rect, if null copy texture
                   &is->sdl_video.rect              // dst rect
                   );

    // 执行渲染(直接渲染该帧)
    SDL_RenderPresent(is->sdl_video.renderer);
}

// 职责：
// 1. 循环处理前后帧
void Video::video_refresh(void *opaque, double *remaining_time)
{
    PlayerStat *is = static_cast<PlayerStat *>(opaque);
    double time;
    static bool first_frame = true;

    while(1)
    {
        if(FrameQueue::frame_queue_nb_remaining(&is->video_frm_queue) == 0) // 所有帧已显示
        {
            return ;
        }

        double last_duration, duration, delay;
        Frame *vp, *lastvp;

        lastvp = FrameQueue::frame_queue_peek_last(&is->video_frm_queue); // 上一帧：上次显示的帧
        vp = FrameQueue::frame_queue_peek(&is->video_frm_queue);          // 当前帧：当前待显示的帧

        // lastvp和vp不是同一播放序列（一个seek会开始一个新播放序列），将frame_timer更新为当前时间
        if(first_frame)
        {
            is->frame_timer = av_gettime_relative() / 1000000.0;
            first_frame = false;
        }

        // 暂停处理：不停播放上一帧
        if(is->paused)
        {
            video_display(is);
        }

        last_duration = vp_duration(is, lastvp, vp);        // 上一帧播放时长：vp->pts - lastvp->pts
        delay = compute_target_delay(last_duration, is);    // 根据视频时钟和同步时钟的差值，计算delay值

        time = av_gettime_relative() / 1000000.0; // 获取系统时间
        // 当前帧播放时刻（is->frame_timer + delay）大于当前时刻（time），表示播放时刻未到
        if(time < is->frame_timer + delay)
        {
            // 播放时刻未到，则更新刷新时间remaining_time为当前时刻到下一播放时刻的时间差
            *remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
            // 播放时刻未到，则不播放，直接返回
            return;
        }

        // 更新frame_timer值
        is->frame_timer += delay;
        // 校正frame_timer值：若frame_timer落后于当前系统时间太久（超过最大同步域值），则更新为当前系统时间
        if(delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX)
        {
            is->frame_timer = time;
        }

        SDL_LockMutex(is->video_frm_queue.mutex);
        if(!isnan(vp->pts))
        {
            update_video_pts(is, vp->pts, vp->pos, vp->serial); // 更新视频时钟：时间戳、时钟时间
        }
        SDL_UnlockMutex(is->video_frm_queue.mutex);

        // 是否丢弃未能及时播放的帧
        if(FrameQueue::frame_queue_nb_remaining(&is->video_frm_queue) > 1) // 队列中未显示帧数 > 1(只有一帧则不考虑丢帧)
        {
            Frame *nextvp = FrameQueue::frame_queue_peek_next(&is->video_frm_queue); // 下一帧：下一待显示的帧
            duration = vp_duration(is, vp, nextvp); // 当前帧vp播放时长 = nextvp->pts - vp->pts
            if(time > is->frame_timer + duration)   // 当前帧未能及时播放，即下一帧播放时刻（is->frame_timer + duration）小于当前系统时刻（time）
            {
                FrameQueue::frame_queue_next(&is->video_frm_queue); // 删除上一帧已显示帧，即删除lastvp，读指针加1（从lastvp更新到vp）
                continue;
            }
        }

        video_display(is);

        // 删除当前读指针元素，读指针+1.若未丢帧，则读指针从lastvp更新到vp；若有丢帧，读指针从vp更新到nextvp
        FrameQueue::frame_queue_next(&is->video_frm_queue);
    }
}

// 职责：
// 1. 视频播放线程（刷新前后帧播放）
int Video::video_playing_thread(void *arg)
{
    PlayerStat *is = static_cast<PlayerStat *>(arg);
    double remaining_time = 0.0;

    while(1)
    {
        if(remaining_time > 0.0)
        {
            av_usleep(static_cast<unsigned>(remaining_time * 1000000.0));
        }
        remaining_time = REFRESH_RATE; // 视频刷新率
        video_refresh(is, &remaining_time); // 刷新前后帧
    }

    return 0;
}

// 职责：
// 1. 创建视频窗口
// 2. 对解析出的视频帧进行格式转换
// 3. 开启视频播放帧
int Video::open_video_playing(void *arg)
{
    PlayerStat *is = static_cast<PlayerStat *>(arg);
    int ret;
    int buf_size;
    uint8_t *buffer = nullptr;

    is->p_frm_yuv = av_frame_alloc();
    if(is->p_frm_yuv == nullptr)
    {
        std::cout << "av_frame_alloc() for p_frm_raw failed\n" << std::endl;
        return -1;
    }

    // 为AVFrame.*data[]手工分配缓冲区，用于储存sws_scale()中目的帧视频数据
    buf_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P,
                                        is->p_vcodec_ctx->width,
                                        is->p_vcodec_ctx->height,
                                        1);

    // buffer将作为p_frm_yuv的视频数据缓冲区
    buffer = static_cast<uint8_t *>(av_malloc(buf_size));
    if(buffer == nullptr)
    {
        std::cout << "av_malloc() for buffer failed" <<std::endl;
        return -1;
    }

    // 使用给定参数设定p_frm_yuv->data和p_frm_yuv->linesize

    ret = av_image_fill_arrays(is->p_frm_yuv->data,
                               is->p_frm_yuv->linesize,
                               buffer,
                               AV_PIX_FMT_YUV420P,
                               is->p_vcodec_ctx->width,
                               is->p_vcodec_ctx->height,
                               1);
    if(ret < 0)
    {
        std::cout << "av_image_fill_arrays() failed " << ret << std::endl;
        return -1;
    }

    // A2. 初始化SWS context，用于后续图像转换
    //     此处第6个参数使用的是FFFmpeg中的像素格式
    //     FFmpeg中的像素格式AV_PIX_FMT_YUV420P对应SDL中的像素格式是SDL_PIXELFORMAT_IYUV
    //     如果解码后得到的图像不被SDL支持，不进行图像转换的话，SDL是无法正常显示图像的
    //     如果解码后得到的图像能被SDL支持，则不必进行图像转换
    //     这里为了编码方便，统一转换为SDL支持的格式AV_PIX_FMT_YUV420P ==> SDL_PIXELFORMAT_IYUV
    is->img_convert_ctx = sws_getContext(is->p_vcodec_ctx->width,
                                         is->p_vcodec_ctx->height,
                                         is->p_vcodec_ctx->pix_fmt,
                                         is->p_vcodec_ctx->width,
                                         is->p_vcodec_ctx->height,
                                         AV_PIX_FMT_YUV420P,
                                         SWS_BICUBIC,
                                         nullptr,
                                         nullptr,
                                         nullptr
                                         );

    if(is->img_convert_ctx == nullptr)
    {
        std::cout << "sws_getContext() failed" << std::endl;
        return -1;
    }

    is->sdl_video.rect.x = 0;
    is->sdl_video.rect.y = 0;
    is->sdl_video.rect.w = is->p_vcodec_ctx->width;
    is->sdl_video.rect.h = is->p_vcodec_ctx->height;

    // 1. 创建SDL窗口，SDL2.0支持多窗口
    //      SDL_Window即运行程序后弹出的视频窗口
    is->sdl_video.window = SDL_CreateWindow("simple ffplayer",
                                            SDL_WINDOWPOS_UNDEFINED, // 不关心窗口的X坐标
                                            SDL_WINDOWPOS_UNDEFINED, // 不关心窗口的Y坐标
                                            is->sdl_video.rect.w,
                                            is->sdl_video.rect.h,
                                            SDL_WINDOW_OPENGL
                                            );

    // 2. 创建SDL_Renderer
    //      SDL_Renderer: 渲染器
    if(is->sdl_video.window == nullptr)
    {
        std::cout << "SDL_CreateWindow() failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    is->sdl_video.renderer = SDL_CreateRenderer(is->sdl_video.window, -1, 0);
    if(is->sdl_video.renderer == nullptr)
    {
        std::cout << "SDL_CreateRenderer() failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    // 3. 创建SDL_Texture
    //      一个SDL_Texture对应一帧YUV数据
    is->sdl_video.texture = SDL_CreateTexture(is->sdl_video.renderer,
                                              SDL_PIXELFORMAT_IYUV,
                                              SDL_TEXTUREACCESS_STREAMING,
                                              is->sdl_video.rect.w,
                                              is->sdl_video.rect.h
                                              );

    if(is->sdl_video.texture == nullptr)
    {
        std::cout << "SDL_CreateTexture() failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    SDL_CreateThread(video_playing_thread, "video playing thread", is);

    return 0;
}

// 职责：
// 1. 寻找解码器
// 2. 分配解码器上下文
// 3. 打开解码器上下文
// 4. 创建并打开解码线程
int Video::open_video_stream(PlayerStat *is)
{
    AVCodecParameters *p_codec_par = nullptr;
    AVCodec const* p_codec = nullptr;
    AVCodecContext *p_codec_ctx = nullptr;
    AVStream *p_stream = is->p_video_stream;
    int ret;

    // 1.为视频流构建解码器AVCodecContext
    // 1.1获取解码器参数AVCodecParameters
    p_codec_par = p_stream->codecpar;

    // 1.2获取解码器
    p_codec = avcodec_find_decoder(p_codec_par->codec_id);
    if(p_codec == nullptr)
    {
        std::cout << "Cann't find codec!" << std::endl;
        return -1;
    }

    // 1.3构建解码器AVCodecContext
    // 1.3.1 p_codec_ctx初始化：分配结构体，使用p_codec初始化相应成员为默认值
    p_codec_ctx = avcodec_alloc_context3(p_codec);
    if(p_codec_ctx == nullptr)
    {
        std::cout << "avcodec_alloc_context3() failed" << std::endl;
        return -1;
    }

    // 1.3.2 p_codec_ctx初始化：p_codec_par ==> p_codec_ctx，初始化相应成员
    ret = avcodec_parameters_to_context(p_codec_ctx, p_codec_par);
    if(ret < 0)
    {
        std::cout << "avcodec_parameters_to_context() failed" << std::endl;
        return -1;
    }

    // 1.3.3 p_codec_ctx初始化：使用p_codec初始化p_codec_ctx，初始化完成
    ret = avcodec_open2(p_codec_ctx, p_codec, nullptr);
    if(ret < 0)
    {
        std::cout << "avcodec_open2() " << ret << std::endl;
        return -1;
    }

    is->p_vcodec_ctx = p_codec_ctx;

    // 2.创建视频解码线程
    SDL_CreateThread(video_decode_thread, "video decode thread", is);

    return 0;
}

int Video::open_video(PlayerStat *is)
{
    open_video_stream(is); // 开启视频流
    open_video_playing(is); // 开启视频播放

    return 0;
}
