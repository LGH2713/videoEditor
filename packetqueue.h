#ifndef PACKETQUEUE_H
#define PACKETQUEUE_H

#include "tool.h"

class PacketQueue
{
public:
    PacketQueue();
    int packet_queue_init(PacketQueue *q);
    int packet_queue_put(PacketQueue *q, PacketList *pkt);
    int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);
    int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);
    void packet_queue_destory(PacketQueue *q);
    void  packet_queue_abort(PacketQueue *q);

    PacketList *first_pkt, *last_pkt;
    int nb_packets; // 队列中packet的数量
    int size; // 队列所占内存空间大小
    int64_t duration; // 队列中所有packet的播放时长
    int abort_request;
    int serial;
    SDL_mutex *mutex;
    SDL_cond *cond;
};

#endif // PACKETQUEUE_H