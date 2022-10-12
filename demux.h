#ifndef DEMUX_H
#define DEMUX_H

#include "tool.h"
#include "common.h"

class Demux
{
public:
    Demux();
    static int decode_interrupt_cb(void *ctx);
    static int demux_init(PlayerStat *is);
    int demux_deinit();
    static int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue);
    static int demux_thread(void *arg);
    static int open_demux(PlayerStat *is);
    static int fail(AVFormatContext *ctx, int ret);
};

#endif // DEMUX_H
