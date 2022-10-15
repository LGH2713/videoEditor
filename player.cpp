#include "player.h"

Player::Player(std::string input_filename) : input_filename(input_filename), is(nullptr), demux(nullptr),
    video(nullptr), audio(nullptr)
{

}

int Player::playing_running()
{
    if(!input_filename.c_str())
    {
        return -1;
    }

    player_init(input_filename.c_str());
    if(is == nullptr)
    {
        std::cout << "player init failed\n" << std::endl;
        do_exit(is);
    }

    demux->open_demux(is); // 开启解复用线程
    video->open_video(is); // 开启视频线程
    audio->open_audio(is); // 开启音频线程

    // 初始化队列事件
    SDL_Event event;

    // 主线程负责视频&音频播放控制
    while(1)
    {
        // 在设置视频模式的线程执行
        // 泵送事件回路，从输入设备收集事件。更新事件队列的内部输入设备状态
        SDL_PumpEvents();

        // SDL event队列为空，则在while循环中播放视频帧。否则从队列头部取一个event，退出当前函数，在上级函数中处理event
        while (!SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT))
        {
            // 沉睡一段时间
            av_usleep(100000);
            SDL_PumpEvents();
        }

        switch(event.type)
        {
        case SDL_KEYDOWN:
            if(event.key.keysym.sym == SDLK_ESCAPE)
            {
                do_exit(is);
                break;
            }

            switch(event.key.keysym.sym)
            {
            case SDLK_SPACE:
                toggle_pause(is);
                break;
            case SDL_WINDOWEVENT:
                break;
            default:
                break;
            }
            break;

        case SDL_QUIT:
        case FF_QUIT_EVENT:
            do_exit(is);
            break;
        default:
            break;
        }
    }

    return 0;
}

void Player::player_init(const char * p_input_file)
{
//    为全局音视频播放结构体分派内存（需要做类型转换）
    is = static_cast<PlayerStat *>(av_mallocz(sizeof(PlayerStat)));
    if(!is) // 分配失败则直接返回
    {
        return ;
    }

    is->fileName = av_strdup(p_input_file); // 复制一个字符串
    if(is->fileName == nullptr) // 复制失败则销毁初始化
    {
        player_deinit(is);      // 反初始化，销毁音视频结构体
    }

    // 初始化视频帧队列和音频帧队列
    if(FrameQueue::frame_queue_init(&is->video_frm_queue, &is->video_pkt_queue, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0 ||
            FrameQueue::frame_queue_init(&is->audio_frm_queue, &is->audio_pkt_queue, SAMPLE_QUEUE_SIZE, 1) < 0)
    {
        player_deinit(is); // 初始化帧队列失败则反初始化
    }

    // 初始化视频包队列和音频包队列
    if(PacketQueue::packet_queue_init(&is->video_pkt_queue) < 0 ||
            PacketQueue::packet_queue_init(&is->audio_pkt_queue) < 0)
    {
        player_deinit(is); // 初始化包队列失败则反初始化
    }

    // 初始化一个空包
    AVPacket flush_pkt;
    flush_pkt.data = nullptr;
    PacketQueue::packet_queue_put(&is->video_pkt_queue, &flush_pkt); // 视频包队列塞入一个空包初始化
    PacketQueue::packet_queue_put(&is->audio_pkt_queue, &flush_pkt); // 音频包队列塞入一个空包初始化

    if(!(is->continue_read_thread = SDL_CreateCond())) // 给音视频结构体初始化一个条件变量
    {
        av_log(nullptr, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        player_deinit(is); // 条件标量创建失败则反初始化
    }

    PlayerClock::init_clock(&is->video_clk, &is->video_pkt_queue.serial); // 初始化视频时钟
    PlayerClock::init_clock(&is->audio_clk, &is->audio_pkt_queue.serial); // 初始化音频时钟

    is->abort_request = 0; // 终止标识设置为假

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) // 初始化SDL
    {
        av_log(nullptr, AV_LOG_FATAL, "Cann't initialize SDL - %s\n", SDL_GetError());
        av_log(nullptr, AV_LOG_FATAL, "(Did you set the DISPLAY variable?)\n");
        exit(1);
    }
}

int Player::player_deinit(PlayerStat *is)
{
    is->abort_request = 1; // 放弃请求标识设为真
    SDL_WaitThread(is->read_tid, nullptr);

//    if(is->audio_index >= 0) {

//    }
//    if(is->video_index >= 0) {

//    }

    avformat_close_input(&is->p_fmt_ctx); // 关闭音视频上下文输入同时清空内存指针设为null

    PacketQueue::packet_queue_abort(&is->video_pkt_queue); // 终止视频包队列
    PacketQueue::packet_queue_abort(&is->audio_pkt_queue); // 终止音频包队列
    PacketQueue::packet_queue_destory(&is->video_pkt_queue); // 清空视频包队列数据同时销毁队列的互斥量和条件变量
    PacketQueue::packet_queue_destory(&is->audio_pkt_queue); // 清空音频包队列数据同时销毁队列的互斥量和条件变量

    // 释放所有图像内存
    FrameQueue::frame_queue_destory(&is->video_frm_queue); // 释放视频帧队列内存
    FrameQueue::frame_queue_destory(&is->audio_frm_queue); // 释放音频帧队列内存

    SDL_DestroyCond(is->continue_read_thread); // 销毁音视频结构体条件变量
    sws_freeContext(is->img_convert_ctx);      // 释放swsContext上下文
    av_free(is->fileName);                     // 释放文件名内存块
    if(is->sdl_video.texture)                  // 如果音视频结构体有视频纹理则销毁
    {
        SDL_DestroyTexture(is->sdl_video.texture);
    }
    av_free(is);                               // 销毁音视频结构体

    return 0;
}

void Player::stream_toggle_pause(PlayerStat *is)
{
    if(is->paused)
    {
        // 这里表示当前是暂停状态，将切换到继续播放状态，在继续播放之前，先将暂停时期流逝的时间加到frame_timer中
        is->frame_timer += av_gettime_relative() / 1000000.0 - is->video_clk.last_updated;
        PlayerClock::set_clock(&is->video_clk, PlayerClock::get_clock(&is->video_clk), is->video_clk.serial);
    }
    is->paused = is->audio_clk.paused = is->video_clk.paused = !is->paused;
}

void Player::toggle_pause(PlayerStat *is)
{
    stream_toggle_pause(is);
    is->step = 0;
}

void Player::do_exit(PlayerStat *is)
{
    if(is)
        player_deinit(is);

    if(is->sdl_video.renderer)
        SDL_DestroyRenderer(is->sdl_video.renderer);
    if(is->sdl_video.window)
        SDL_DestroyWindow(is->sdl_video.window);

    avformat_network_deinit();

    SDL_Quit();

    exit(0);
}

