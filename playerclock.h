#ifndef PLAYERCLOCK_H
#define PLAYERCLOCK_H

#include "tool.h"

class PlayerClock
{
public:
    PlayerClock();
    PlayerClock(PlayerClock *c, int *queue_sreial);
    ~PlayerClock();

    double pts; // 当前帧（待播放）显示时间戳，播放后，当前帧变成上一帧
    double pts_drift; // 当前帧显示时间戳与当前系统时间的差值
    double last_updated; // 当前时钟（如视频时钟）最后一次更新时间，也可称作当前时钟时间
    double speed; // 时钟速度控制，用于控制播放速度
    int serial; // 播放序列，所谓的播放序列就是一段连续的播放动作，一个seek操作会启动一段新的播放序列
    int paused; // 暂停标志
    int *queue_serial; // 指向packet_serial

    static double get_clock(PlayerClock *c);
    static void set_clock_at(PlayerClock *c, double pts, int serial, double time);
    static void set_clock(PlayerClock *c, double pts, int serial);
    void init_clock(PlayerClock *c, int *queue_serial);
    void set_clock_speed(PlayerClock *c, double speed);
    void sync_play_clock_to_slave(PlayerClock *c, PlayerClock *slave);
};

#endif // PLAYERCLOCK_H
