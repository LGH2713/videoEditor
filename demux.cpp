#include "demux.h"

Demux::Demux()
{

}

int Demux::decode_interrupt_cb(void *ctx)
{
    PlayerStat *is = static_cast<PlayerStat *>(ctx);
    return is->abort_request;
}

int Demux::demux_init(PlayerStat *is)
{
    AVFormatContext *p_fmt_ctx = nullptr;
    int err, ret;
    int a_index;
    int v_index;

    p_fmt_ctx = avformat_alloc_context();
    if(!p_fmt_ctx) {
        std::cout << "Could not allocate context.\n" << std::endl;
        ret = AVERROR(ENOMEM);
        fail(p_fmt_ctx, ret);
    }

    // 终端回调机制。为底层I/O层提供一个处理接口，不如终止I/O操作
    p_fmt_ctx->interrupt_callback.callback = decode_interrupt_cb;
    p_fmt_ctx->interrupt_callback.opaque = is;

    // 1.构建AVFormatContext
    // 1.1打开视频文件，读取文件头，将文件格式信息储存在”fmt context“ 中
    err = avformat_open_input(&p_fmt_ctx, is->fileName, nullptr, nullptr);
    if(err < 0) {
        std::cout << "avformat_open_input() failed " << err << std::endl;
        fail(p_fmt_ctx, -1);
    }

    is->p_fmt_ctx = p_fmt_ctx;

    // 1.2 搜索流信息：读取一段视频文件数据，尝试解码，将取到的信息填入p_fmt_ctx->streams
    // ic->streams是一个指针数组，数组大小是pFormatCtx->nb_streams
    err = avformat_find_stream_info(p_fmt_ctx, nullptr);
    if(err < 0) {
        std::cout << "avformat_find_stream_info() failed " << err << std::endl;
        fail(p_fmt_ctx, -1);
    }
    // 2. 查找第一个音频流/视频流
    a_index = -1;
    v_index = -1;
    for(int i = 0; i < static_cast<int>(p_fmt_ctx->nb_streams); i++) {
        if((p_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) &&
                (a_index == -1))
        {
            a_index = i;
            std::cout << "Find an audio stream, index " << a_index << std::endl;
        }
        if((p_fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) &&
                (v_index == -1))
        {
            v_index = i;
            std::cout << "Find a video stream, index " << v_index << std::endl;
        }
        if(a_index != -1 && v_index != -1) {
            break;
        }
    }
    if(a_index == -1 && v_index == -1) {
        std::cout << "Cann't find any audio/video stream" << std::endl;
        fail(p_fmt_ctx, -1);
    }

    is->audio_index = a_index;
    is->video_index = v_index;
    is->p_audio_stream = p_fmt_ctx->streams[a_index];
    is->p_video_stream = p_fmt_ctx->streams[v_index];
    return 0;
}

int Demux::demux_deinit()
{
    return 0;
}

int Demux::stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue)
{
    return stream_id < 0 ||
            queue->abort_request ||
            (st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
            queue->nb_packets > MIN_FRAMES && (!queue->duration || av_q2d(st->time_base) * queue->duration > 1.0);
}

int Demux::demux_thread(void *arg)
{
    PlayerStat *is = static_cast<PlayerStat *>(arg);
    AVFormatContext *p_fmt_ctx = is->p_fmt_ctx;
    int ret;
    AVPacket *pkt;

    SDL_mutex *wait_mutex = SDL_CreateMutex();

    std::cout << "demux_thread running..." << std::endl;

    // 4.解复用处理
    while(1) {

        std::cout << "demux callback" << std::endl;

        if(is->abort_request) {
            std::cout << "demux abort" << std::endl;
            break;
        }

        std::cout << "demux 1 ?: " << is->audio_pkt_queue.size + is->video_pkt_queue.size << std::endl;
        std::cout << "demux 2 ?: " << stream_has_enough_packets(is->p_audio_stream, is->audio_index, &is->audio_pkt_queue) << std::endl;
        std::cout << "demux 3 ?: " <<  stream_has_enough_packets(is->p_video_stream, is->video_index, &is->video_pkt_queue) << std::endl;


        // 如果包队列数据已满，则不需要继续读
        if(is->audio_pkt_queue.size + is->video_pkt_queue.size > MAX_QUEUE_SIZE ||
                (stream_has_enough_packets(is->p_audio_stream, is->audio_index, &is->audio_pkt_queue) &&
                 stream_has_enough_packets(is->p_video_stream, is->video_index, &is->video_pkt_queue)))
        {
            std::cout << "demux lock" << std::endl;
            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }

        // 4.1从输入文件读取一个packet
        ret = av_read_frame(is->p_fmt_ctx, pkt);
        if(ret < 0)
        {
            std::cout << "put null packet in queue" << std::endl;
            if(ret == AVERROR_EOF)
            {
                // 输入文件已读完，则往packet队列中发送NULL packet，以冲洗（flush）解码器，否则解码器中缓存的帧取不出来
                if(is->video_index >= 0)
                {
                    PacketQueue::packet_queue_put_nullpacket(&is->video_pkt_queue, is->video_index);
                }
                if(is->audio_index >= 0)
                {
                    PacketQueue::packet_queue_put_nullpacket(&is->audio_pkt_queue, is->audio_index);
                }
            }

            SDL_LockMutex(wait_mutex);
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10);
            SDL_UnlockMutex(wait_mutex);
            continue;
        }

        // 4.3根据当前packet类型（音频、视频、字幕），将其存入对应的packet队列
        if(pkt->stream_index == is->audio_index)
        {
            std::cout << "put audio packet in queue" << std::endl;
            PacketQueue::packet_queue_put(&is->audio_pkt_queue, pkt);
        }
        else if(pkt->stream_index == is->video_index)
        {
            std::cout << "put video packet in queue" << std::endl;
            PacketQueue::packet_queue_put(&is->video_pkt_queue, pkt);
        }
        else
        {
            av_packet_unref(pkt);
        }

    }

    ret = 0;
    if(ret != 0) {
        SDL_Event event;
        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);
    }

    SDL_DestroyMutex(wait_mutex);
    return 0;
}

int Demux::open_demux(PlayerStat *is)
{
    if(demux_init(is) != 0)
    {
        std::cout << "demux_init() failed" << std::endl;
        return -1;
    }

    is->read_tid = SDL_CreateThread(demux_thread, "demux_thread", is);
    if(is->read_tid == nullptr)
    {
        std::cout << "SDL_CreateThread() failed " << SDL_GetError() << std::endl;
        return -1;
    }

    return 0;
}


int Demux::fail(AVFormatContext *ctx, int ret)
{
    if(ctx != nullptr) {
        avformat_close_input(&ctx);
    }
    return ret;
}
