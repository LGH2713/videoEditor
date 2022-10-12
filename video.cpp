#include "video.h"

Video::Video()
{

}

int Video::queue_picture(PlayerStat *is, AVFrame *src_frame, double pts, double duration, int64_t pos)
{

}

int Video::video_decode_frame(AVCodecContext *p_codec_ctx, PacketQueue *p_pkt_queue, AVFrame *frame)
{

}

int Video::video_decode_thread(void *arg)
{

}

double Video::compute_target_delay(double delay, PlayerStat *is)
{

}

double Video::vp_duration(PlayerStat *is, Frame *vp, Frame *nextvp)
{

}

void Video::update_video_pts(PlayerStat *is, double pts, int64_t pos, int serial)
{

}

void Video::video_display(PlayerStat *is)
{

}

void Video::video_fresh(void *opaque, double *remaining_time)
{

}

int Video::video_playing_thread(void *arg)
{

}

void Video::open_video_playing(void *arg)
{

}

int Video::open_video_stream(PlayerStat *is)
{

}
