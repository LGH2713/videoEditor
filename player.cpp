#include "player.h"

Player::Player(std::string input_filename) : input_filename(input_filename)
{

}

int Player::playing_running()
{
    PlayerStat *is = nullptr;

    if(!input_filename.c_str()) {
        return -1;
    }

    is = player_init(input_filename.c_str());
    if(is == nullptr) {
        std::cout << "player init failed\n" << std::endl;
        do_exit(is);
    }

    Demux::open_demux(is);
    Video::open_video(is);
    Audio::open_audio(is);

    // 初始化队列事件
    SDL_Event event;

    while(1) {
        // 在设置视频模式的线程执行
        // 泵送事件回路，从输入设备收集事件。更新事件队列的内部输入设备状态
        SDL_PumpEvents();

        // SDL event队列为空，则在while循环中播放视频帧。否则从队列头部取一个event，退出当前函数，在上级函数中处理event
        while (!SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) {
            // 沉睡一段时间
            av_usleep(100000);
            SDL_PumpEvents();
        }

        switch(event.type) {
        case SDL_KEYDOWN:
            if(event.key.keysym.sym == SDLK_ESCAPE) {
                do_exit(is);
                break;
            }

            switch(event.key.keysym.sym) {
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

PlayerStat *Player::player_init(const char * p_input_file)
{
    PlayerStat *is;

    is = static_cast<PlayerStat *>(av_mallocz(sizeof(PlayerStat)));
    if(!is) {
        return nullptr;
    }

    is->fileName = av_strdup(p_input_file);
    if(is->fileName == nullptr) {
        player_deinit(is);
    }

    if(FrameQueue::frame_queue_init(&is->video_frm_queue, &is->video_pkt_queue, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0 ||
            FrameQueue::frame_queue_init(&is->audio_frm_queue, &is->audio_pkt_queue, SAMPLE_QUEUE_SIZE, 1) < 0) {
        player_deinit(is);
    }

    if(PacketQueue::packet_queue_init(&is->video_pkt_queue) < 0 ||
            PacketQueue::packet_queue_init(&is->audio_pkt_queue) < 0) {
        player_deinit(is);
    }

    PlayerClock::init_clock(&is->video_clk, &is->video_pkt_queue.serial);
    PlayerClock::init_clock(&is->audio_clk, &is->audio_pkt_queue.serial);

    is->abort_request = 0;

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        av_log(nullptr, AV_LOG_FATAL, "Cann't initialize SDL - %s\n", SDL_GetError());
        av_log(nullptr, AV_LOG_FATAL, "(Did you set the DISPLAY variable?)\n");
        exit(1);
    }

    return is;
}

int Player::player_deinit(PlayerStat *is)
{
    is->abort_request = 1;
    SDL_WaitThread(is->read_tid, nullptr);

    if(is->audio_index >= 0) {

    }
    if(is->video_index >= 0) {

    }

    avformat_close_input(&is->p_fmt_ctx);

    PacketQueue::packet_queue_abort(&is->video_pkt_queue);
    PacketQueue::packet_queue_abort(&is->audio_pkt_queue);
    PacketQueue::packet_queue_destory(&is->video_pkt_queue);
    PacketQueue::packet_queue_destory(&is->audio_pkt_queue);

    FrameQueue::frame_queue_destory(&is->video_frm_queue);
    FrameQueue::frame_queue_destory(&is->audio_frm_queue);

    SDL_DestroyCond(is->continue_read_thread);
    sws_freeContext(is->img_convert_ctx);
    av_free(is->fileName);
    if(is->sdl_video.texture) {
        SDL_DestroyTexture(is->sdl_video.texture);
    }
    av_free(is);

    return 0;
}

void Player::stream_toggle_pause(PlayerStat *is)
{
    if(is->paused) {
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

