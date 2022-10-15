#ifndef VIDEO_H
#define VIDEO_H

#include "common.h"
#include "framequeue.h"

class Video
{
public:
    Video();
    static int queue_picture(PlayerStat *is, AVFrame *src_frame, double pts, double duration, int64_t pos);

    // 从packet_queue中取一个packet，解码生成frame
    static int video_decode_frame(AVCodecContext *p_codec_ctx, PacketQueue *p_pkt_queue, AVFrame *frame);

    // 将视频包解码得到视频帧，然后写入picture队列
    static int video_decode_thread(void *arg);

    // 根据视频时钟与同步时钟（入音频时钟）的差值，矫正delay值，使视频时钟追赶或等待同步时钟
    // 输入参数delay是上一帧播放时长，即上一帧播放后应延时多长时间后再播放当前帧，通过调节此值来调节当前帧播放快慢
    // 返回值delay是将输入参数delay经校正后得到的值
    static double compute_target_delay(double delay, PlayerStat *is);
    static double vp_duration(PlayerStat *is, Frame *vp, Frame *nextvp);
    static void update_video_pts(PlayerStat *is, double pts, int64_t pos, int serial);
    static void video_display(PlayerStat *is);

    // 调用后显示每一帧
    static void video_refresh(void *opaque, double *remaining_time);
    static int video_playing_thread(void *arg);
    static int open_video_playing(void *arg);
    static int open_video_stream(PlayerStat *is);
};

#endif // VIDEO_H
