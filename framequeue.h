#ifndef FRAMEQUEUE_H
#define FRAMEQUEUE_H

#include "tool.h"
#include "frame.h"
#include "packetqueue.h"


class FrameQueue
{
public:
    FrameQueue();
    ~FrameQueue();
    int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last);
    void frame_queue_destory(FrameQueue *f);
    void frame_queue_signal(FrameQueue *f);
    Frame *frame_queue_peek(FrameQueue *f);
    Frame *frame_queue_peek_next(FrameQueue *f);
    Frame *frame_queue_peek_last(FrameQueue *f);
    static Frame *frame_queue_peek_writable(FrameQueue *f);
    static Frame *frame_queue_peek_readable(FrameQueue *f);
    static void frame_queue_push(FrameQueue *f);
    static void frame_queue_next(FrameQueue *f);
    static int frame_queue_nb_remaining(FrameQueue *f);
    int64_t frame_queue_last_pos(FrameQueue *f);

    Frame queue[FRAME_QUEUE_SIZE];
    int rindex; // 读索引。待播放时读取此帧进行播放，播放后 此帧成为上一帧
    int windex; //写索引
    int  size; // 总帧数
    int max_size; // 队列可储存最大帧数
    int keep_last;
    int rindex_shown; // 当前是否有帧在显示
    SDL_mutex *mutex;
    SDL_cond *cond;
    PacketQueue *pktq;
};

#endif // FRAMEQUEUE_H
