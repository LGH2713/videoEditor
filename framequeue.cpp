#include "framequeue.h"

FrameQueue::FrameQueue()
{

}

FrameQueue::~FrameQueue()
{

}

int FrameQueue::frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last)
{
    memset(f, 0, sizeof(FrameQueue));

    // 创建SDL互斥锁
    if(!(f->mutex = SDL_CreateMutex())) {
        av_log(nullptr, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }

    // 创建条件变量
    if(!(f->cond = SDL_CreateCond())) {
        av_log(nullptr, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }

    // 设置帧队列初始化属性
    f->pktq = pktq;
    f->max_size = max_size;
    f->keep_last = keep_last;

    // 帧内存分配错误
    for(int i = 0; i < f->max_size; i++) {
        if(!(f->queue[i].frame = av_frame_alloc()))
            return AVERROR(ENOMEM);
    }

    return 0;
}

// 销毁所有帧
void FrameQueue::frame_queue_destory(FrameQueue *f)
{
    for(int i = 0; i < f->max_size; i++) {
        Frame *vp = &f->queue[i];
        Frame::frame_queue_unref_item(vp);
        av_frame_unref(vp->frame);
    }
    SDL_DestroyMutex(f->mutex);
    SDL_DestroyCond(f->cond);
}

void FrameQueue::frame_queue_signal(FrameQueue *f)
{
    SDL_LockMutex(f->mutex);
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

Frame *FrameQueue::frame_queue_peek(FrameQueue *f)
{
    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

Frame *FrameQueue::frame_queue_peek_next(FrameQueue *f)
{
    return &f->queue[(f->rindex + f->rindex_shown + 1) % f->max_size];
}

// 取出此帧进行播放，只读取不删除，不删除是因为此帧需要缓存
Frame *FrameQueue::frame_queue_peek_last(FrameQueue *f)
{
    return &f->queue[f->rindex];
}

//向队列尾部申请一个可写空间，若无空间可写，则等待
Frame *FrameQueue::frame_queue_peek_writable(FrameQueue *f)
{
    SDL_LockMutex(f->mutex);
    while(f->size >= f->max_size && !f->pktq->abort_request) {
        SDL_CondWait(f->cond, f->mutex);
    }
    SDL_UnlockMutex(f->mutex);

    if(f->pktq->abort_request)
        return nullptr;

    return &f->queue[f->windex];
}

//从队列头部读取一帧，只读取不删除，若无帧可读则等待
Frame *FrameQueue::frame_queue_peek_readable(FrameQueue *f)
{
    SDL_LockMutex(f->mutex);
    while(f->size - f->rindex_shown <= 0 && !f->pktq->abort_request) {
        SDL_CondWait(f->cond, f->mutex);
    }
    SDL_UnlockMutex(f->mutex);
    if(f->pktq->abort_request)
        return nullptr;

    return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

//向队列尾部压入一帧，只更新计数和写指针，因此调用此函数前应将帧数据写入队列相应位置
void FrameQueue::frame_queue_push(FrameQueue *f)
{
    if(++f->windex == f->max_size)
        f->windex = 0;
    SDL_LockMutex(f->mutex);
    f->size ++;
    SDL_CondSignal(f->cond); // 重启所有因条件变量而阻塞的线程
    SDL_UnlockMutex(f->mutex); // 解锁互斥量
}

//读指针（rindex）指向的帧已显示，删除此帧，注意不读取直接删除。读指针+1
void FrameQueue::frame_queue_next(FrameQueue *f)
{
    if(f->keep_last && !f->rindex_shown) {
        f->rindex_shown = 1;
        return ;
    }
    Frame::frame_queue_unref_item(&f->queue[f->rindex]);
    if(++f->rindex == f->max_size)
        f->rindex = 0;
    f->size --;
    SDL_CondSignal(f->cond);
    SDL_UnlockMutex(f->mutex);
}

// 帧队列中未显示的帧数
int FrameQueue::frame_queue_nb_remaining(FrameQueue *f)
{
    return f->size - f->rindex_shown;
}

// 返回读帧最后一次显示的位置
int64_t FrameQueue::frame_queue_last_pos(FrameQueue *f)
{
    Frame *fp = &f->queue[f->rindex];
    if(f->rindex_shown && fp->serial == f->pktq->serial)
        return fp->pos;
    else
        return -1;
}


