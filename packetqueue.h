#ifndef PACKETQUEUE_H
#define PACKETQUEUE_H

#include "playerclock.h"
#include "tool.h"

class PacketQueue
{
public:
    PacketQueue();
    static int packet_queue_init(PacketQueue *q);
    static int packet_queue_put(PacketQueue *q, AVPacket *pkt);
    static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);
    static int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);
    static void packet_queue_flush(PacketQueue *q);
    static void packet_queue_destory(PacketQueue *q);
    static void  packet_queue_abort(PacketQueue *q);

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
