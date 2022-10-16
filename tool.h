#ifndef TOOL_H
#define TOOL_H

extern "C" {
#undef main
#include <libavcodec//avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/frame.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <SDL.h>
#include <SDL_video.h>
#include <SDL_render.h>
#include <SDL_rect.h>
#include <SDL_mutex.h>
}

#include <iostream>

//如果低于最小AV同步阈值，则不进行AV同步校正
static const double AV_SYNC_THRESHOLD_MIN = 0.04;

//如果超过AV同步最大阈值，则进行AV同步校正
static const double AV_SYNC_THRESHOLD_MAX = 0.1;

//如果一个帧的持续时间超过这个值，它将不会被复制来补偿AV同步
static const double AV_SYNC_FRAMEDUP_THRESHOLD = 0.1;

//如果误差太大，则不进行AV校正
static const double AV_NOSYNC_THRESHOLD = 10.0;

//轮询可能需要的屏幕刷新至少这么频繁，应该小于1/fps
static const double REFRESH_RATE = 0.01;

static const unsigned int SDL_AUDIO_BUFFER_SIZE = 1024;
static const unsigned int MAX_AUDIO_FRAME_SIZE = 192000;

static const unsigned int MAX_QUEUE_SIZE = (15 * 1024 * 1024);
static const unsigned int MIN_FRAMES = 25;

//最小SDL音频缓冲区大小，在样本中。
static const unsigned int SDL_AUDIO_MIN_BUFFER_SIZE = 512;
//计算实际的缓冲区大小，记住不要引起太频繁的音频回调
static const unsigned int SDL_AUDIO_MAX_CALLBACKS_PRE_SEC = 30;

static const unsigned int VIDEO_PICTURE_QUEUE_SIZE = 3;
static const unsigned int SUBPICTURE_QUEUE_SIZE = 16;
static const unsigned int SAMPLE_QUEUE_SIZE = 9;

#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

#define FF_QUIT_EVENT (SDL_USEREVENT + 2)

class PacketList {
public:
    AVPacket pkt;
    PacketList *next;
};

#endif // TOOL_H
