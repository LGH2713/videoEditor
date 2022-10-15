#include "packetqueue.h"

PacketQueue::PacketQueue()
{

}

int PacketQueue::packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    if(!q->mutex)
    {
        std::cout << "SDL_CreateMutex(): " << SDL_GetError() << std::endl;
        return AVERROR(ENOMEM);
    }
    q->cond = SDL_CreateCond();
    if(!q->cond)
    {
        std::cout << "SDL_CreateCond(): " << SDL_GetError() << std::endl;
        return AVERROR(ENOMEM);
    }
    q->abort_request = 0;
    return 0;
}

//写队列尾部。pkt是一包还未解码的音频数据
int PacketQueue::packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    PacketList *pkt_node; // 初始化一个包列表节点

    if(av_packet_make_refcounted(pkt) < 0) // 确保给定数据包所描述的数据被引用计数。
    {
        std::cout << "[pkt] is not refrence counted" << std::endl;
        return -1;
    }

    pkt_node = static_cast<PacketList *>(av_malloc(sizeof(PacketList))); // 为包列表节点分配空间
    if(!pkt_node) // 分配空间失败则返回-1
        return -1;

    pkt_node->pkt = *pkt; // 当前包列表节点的包为传入的数据包
    pkt_node->next = nullptr; // 下一个包节点为空

    SDL_LockMutex(q->mutex); // 锁住包队列的互斥量，为后面包队列内容修改做准备

    if(!q->last_pkt) //尾节点为空，则包队列的头节点数据为空包的节点pkt_node
    {
        q->first_pkt = pkt_node;
    }
    else // 若队列不为空，则队列的尾节点的下一个节点为传入的数据包
    {
        q->last_pkt->next = pkt_node;
    }
    q->last_pkt = pkt_node; // 尾节点新增的数据包
    q->nb_packets++; // 包计数+1
    q->size += pkt_node->pkt.size; // 包队列空间大小加一个包节点大小

    // 发送条件变量的信号，重启等待q->cond条件变量的一个线程
    SDL_CondSignal(q->cond);
    SDL_UnlockMutex(q->mutex); // 解锁互斥量
    return 0;
}

int PacketQueue::packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
    PacketList *p_pkt_node;
    int ret;

    SDL_LockMutex(q->mutex);

    while(1)
    {
        p_pkt_node = q->first_pkt;
        if(p_pkt_node) // 队列非空，取一个出来
        {
            q->first_pkt = q->first_pkt->next;
            if(!q->first_pkt) // 已取出最后一包数据
            {
                q->last_pkt = nullptr; // 队列尾包设置为nullptr
            }
            q->nb_packets --; // 队列计数-1
            q->size -= p_pkt_node->pkt.size; // 更新队列容量大小
            *pkt = p_pkt_node->pkt; // pkt装载着队列取出的数据（返回数据位置）
            av_free(p_pkt_node); // 释放p_pkt_node内存
            ret = 1;
            break;
        }
        else if(!block) // 队列空且阻塞标志无效，则立即退出
        {
            ret = 0;
            break;
        }
        else // 队列空且阻塞标志有效，则等待
        {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}

// 压入空包
int PacketQueue::packet_queue_put_nullpacket(PacketQueue *q, int stream_index)
{
    AVPacket *pkt = av_packet_alloc();
    pkt->data = nullptr;
    pkt->size = 0;
    pkt->stream_index = stream_index;
    return packet_queue_put(q, pkt);
}

// 刷新包队列
void PacketQueue::packet_queue_flush(PacketQueue *q)
{
    PacketList *pkt, *pkt1;

    SDL_LockMutex(q->mutex);
    for(pkt = q->first_pkt; pkt; pkt = pkt1) // 循环解绑所有数据包
    {
        pkt1 = pkt->next;
        av_packet_unref(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = nullptr;
    q->first_pkt = nullptr;
    q->nb_packets = 0;
    q->size = 0;
    q->duration = 0;
    SDL_UnlockMutex(q->mutex);
}

// 用析构函数重写 ************************
void PacketQueue::packet_queue_destory(PacketQueue *q)
{
    packet_queue_flush(q); // 清空队列数据
    SDL_DestroyMutex(q->mutex); // 销毁互斥量
    SDL_DestroyCond(q->cond); // 销毁条件变量
}

//队列终止
void PacketQueue::packet_queue_abort(PacketQueue *q)
{
    SDL_LockMutex(q->mutex); // 锁住队列互斥

    q->abort_request = 1; // 设置终止标志

    SDL_CondSignal(q->cond); // 发送条件变量，唤醒一个条件变量阻塞线程

    SDL_UnlockMutex(q->mutex); // 解锁队列互斥量
}
