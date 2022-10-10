#ifndef CLOCK_H
#define CLOCK_H


class Clock
{
public:
    Clock();

private:
    double pts; // 当前帧（待播放）显示时间戳，播放后，当前帧变成上一帧
    double pts_drift; // 当前帧显示时间戳与当前系统时间的差值
    double last_updated; // 当前时钟（如视频时钟）最后一次更新时间，也可称为当前时钟时间
    double speed; // 时钟速度控制，用于控制播放速度
    int serial; // 播放序列。所谓播放序列就是一段连续的播放动作，一个seek操作会启动一段新的播放序列
    int paused; // 暂停标志
    int *queue_serial; // 指向packet_serial

    double get_clock();
};

#endif // CLOCK_H
