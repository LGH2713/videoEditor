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

    p_fmt_ctx = avformat_alloc_context(); // 为音视频格式上下文分配空间
    if(!p_fmt_ctx)
    {
        std::cout << "Could not allocate context.\n" << std::endl;
        ret = AVERROR(ENOMEM);
        fail(p_fmt_ctx, ret);
    }

    // 终端回调机制。为底层I/O层提供一个处理接口，不如终止I/O操作
    p_fmt_ctx->interrupt_callback.callback = decode_interrupt_cb; // 设置回调函数
    p_fmt_ctx->interrupt_callback.opaque = is; // 设置回调函数的参数

    // 1.构建AVFormatContext
    // 1.1打开视频文件，读取文件头，将文件格式信息储存在”fmt context"中
    err = avformat_open_input(&p_fmt_ctx, is->fileName, nullptr, nullptr);
    if(err < 0)
    {
        std::cout << "avformat_open_input() failed " << err << std::endl;
        fail(p_fmt_ctx, -1);
    }

    is->p_fmt_ctx = p_fmt_ctx; // 为音视频结构体格式上下文赋值为刚刚创建的格式上下文

    // 1.2 搜索流信息：读取一段视频文件数据，尝试解码，将取到的信息填入p_fmt_ctx->streams
    // ic->streams是一个指针数组，数组大小是pFormatCtx->nb_streams
    err = avformat_find_stream_info(p_fmt_ctx, nullptr);
    if(err < 0)
    {
        std::cout << "avformat_find_stream_info() failed " << err << std::endl;
        fail(p_fmt_ctx, -1);
    }
    // 2. 查找第一个音频流/视频流
    a_index = -1;
    v_index = -1;
    for(int i = 0; i < static_cast<int>(p_fmt_ctx->nb_streams); i++)
    {
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
        if(a_index != -1 && v_index != -1)
        {
            break;
        }
    }
    if(a_index == -1 && v_index == -1) {
        std::cout << "Cann't find any audio/video stream" << std::endl;
        fail(p_fmt_ctx, -1);
    }

    is->audio_index = a_index; // 赋值音频索引
    is->video_index = v_index; // 赋值视频索引
    is->p_audio_stream = p_fmt_ctx->streams[a_index]; // 赋值音频流
    is->p_video_stream = p_fmt_ctx->streams[v_index]; // 赋值视频流
    return 0;
}

int Demux::demux_deinit()
{
    return 0;
}

int Demux::stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue)
{
    return stream_id < 0 || // 音频/视频索引小于0
            queue->abort_request || // 终止标识为真
            (st->disposition & AV_DISPOSITION_ATTACHED_PIC) || // 流配置按位与
            queue->nb_packets > MIN_FRAMES && // 队列包数大于最小帧数
            !queue->duration ||  // 队列中所有包的时长为0
            av_q2d(st->time_base) * queue->duration > 1.0; // 时间基（pts单位时间）与队列总时长相乘 大于 1.0
}

// 解复用器线程回调函数
int Demux::demux_thread(void *arg)
{
    PlayerStat *is = static_cast<PlayerStat *>(arg); // 音视频结构体格式转换
    AVFormatContext *p_fmt_ctx = is->p_fmt_ctx;      // 获取音视频格式上下文
    int ret;
    AVPacket *pkt = av_packet_alloc(); // 一定要先分配空间

    SDL_mutex *wait_mutex = SDL_CreateMutex(); // 创建线程互斥量

    std::cout << "demux_thread running..." << std::endl;

    // 4.解复用处理
    while(1)
    {
        if(is->abort_request) // 如果终止标识为真则直接跳出循环
        {
            std::cout << "demux abort" << std::endl;
            break;
        }


        // 如果包队列数据已满，则不需要继续读
        if(is->audio_pkt_queue.size + is->video_pkt_queue.size > MAX_QUEUE_SIZE || // 视频队列大小和音频队列大小大于最大队列数
                (stream_has_enough_packets(is->p_audio_stream, is->audio_index, &is->audio_pkt_queue) && // 音频包队列有足够数据
                 stream_has_enough_packets(is->p_video_stream, is->video_index, &is->video_pkt_queue)))  // 视频包队列有足够数据
        {
            // 等待 10 毫秒
            SDL_LockMutex(wait_mutex); // 锁住互斥量
            SDL_CondWaitTimeout(is->continue_read_thread, wait_mutex, 10); // 条件变量10毫秒后解开
            SDL_UnlockMutex(wait_mutex); // 解锁互斥量
            continue;
        }

        // 4.1从输入文件读取一个packet
        ret = av_read_frame(is->p_fmt_ctx, pkt);
        if(ret < 0)
        {
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

    is->read_tid = SDL_CreateThread(demux_thread, "demux_thread", is); // 创建解复用器线程，获取线程id
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
        avformat_close_input(&ctx); // 关闭格式上下文输入
    }
    return ret;
}
