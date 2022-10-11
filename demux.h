#ifndef DEMUX_H
#define DEMUX_H

#include "player.h"

class Demux
{
public:
    Demux();
    static int decode_interrupt_cb(void *ctx);
    static int demux_init(PlayerStat *is);
    int demux_deinit();
    static int stream_has_enough_packets(AVStream *st, int stream_is, PacketQueue *queue);
    static int demux_thread(void *arg);
    int open_demux(PlayerStat *is);
};

#endif // DEMUX_H
