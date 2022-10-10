#ifndef FRAME_H
#define FRAME_H

#include "tool.h"

//用于处理所有类型的解码数据和分配的呈现缓冲区的公共结构。
class Frame
{
public:
    Frame();
    ~Frame();
    static void frame_queue_unref_item(Frame *vp);

    AVFrame *frame;
    int serial;
    double pts;
    double duration;
    int64_t pos; // frame对应packet在输入文件中的地址偏移
    int width;
    int height;
    int format;
    AVRational sar; // 有理数
    int uploaded;
    int flip_v;
};

#endif // FRAME_H
