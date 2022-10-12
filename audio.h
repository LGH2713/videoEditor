#ifndef AUDIO_H
#define AUDIO_H

#include "common.h"
#include "framequeue.h"

class Audio
{
public:
    Audio();
    // sdl音频回调函数
    static void sdl_audio_callback(void *opaque, Uint8 *stream, int len);
    // 从packet_queue中取一个packet，解码生成frame
    static int audio_decode_frame(AVCodecContext *p_codec_ctx, PacketQueue *p_pkt_queue, AVFrame *frame);
    // 音频解码线程，从音频packet_queue中取数据，解码后放入音频frame_queue
    static int audio_decode_thread(void *arg);
    // 打开音频流
    static int open_audio_stream(PlayerStat *is);
    // 音频重采样
    static int audio_resample(PlayerStat *is, int64_t audio_callback_time);
    // 播放音频
    static int open_audio_playing(void *arg);
    // 申请SDL音频缓冲区大小
    static int open_audio(PlayerStat *is);
};

#endif // AUDIO_H
