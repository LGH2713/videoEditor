#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include "tool.h"
#include "playerclock.h"
#include "framequeue.h"
#include "packetqueue.h"

class AudioParam {
public:
    int freq;
    int channels;
    AVChannelLayout channel_layout;
    enum AVSampleFormat fmt;
    int frame_size;
    int bytes_pre_sec;
};

class SDLVideo {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Rect rect;
};

class PlayerStat {
public:
    char *fileName;
    AVFormatContext *p_fmt_ctx;
    AVStream *p_audio_stream;
    AVStream *p_video_stream;
    AVCodecContext *p_acodec_ctx;
    AVCodecContext *p_vcodec_ctx;

    int audio_index;
    int video_index;
    SDLVideo sdl_video;

    PlayerClock audio_clk; // 音频时钟
    PlayerClock video_clk; // 视频时钟
    double frame_timer;

    PacketQueue audio_pkt_queue;
    PacketQueue video_pkt_queue;
    FrameQueue audio_frm_queue;
    FrameQueue video_frm_queue;

    struct SwsContext *img_convert_ctx;
    struct SwrContext *audio_swr_ctx;
    AVFrame *p_frm_yuv;

    AudioParam audio_param_src;
    AudioParam audio_param_tgt;
    int audio_hw_buf_size;          // SDL音频缓冲区大小（单位：字节）
    uint8_t *p_audio_frm;           // 指向待播放的一帧音频数据，指向的数据区将被拷入SDL音频缓冲区。若经过重采样则指向audio_frm_rwr，否则指向frame中的音频
    uint8_t *audio_frm_rwr;        // 音频重采样的输出缓冲区
    unsigned int audio_frm_size;    // 待播放的一帧音频数据（audio_buf指向）的大小
    unsigned int audio_frm_rwr_size;// 申请到的音频缓冲区audio_frm_rwr的实际尺寸
    int audio_cp_index;             // 当前音频帧中已拷入SDL音频缓冲区的位置索引（指向第一个待拷贝字节）
    int audio_write_buf_size;       // 当前音频帧中尚未拷入SDL音频缓冲区的数据量 audio_frm_size = audio_cp_index + audio_write_buf_size
    double audio_clock;
    int audio_clock_serial;

    int abort_request;
    int paused;
    int step;

    SDL_cond *continue_read_thread;
    SDL_Thread *read_tid;           // demux解复用线程
};

class Player
{
public:
    Player();

    int playing_running(const std::string p_input_file);
};

#endif // PLAYER_H
