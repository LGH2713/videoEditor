#include "playerclock.h"
#include "player.h"

PlayerClock::PlayerClock()
{

}

PlayerClock::PlayerClock(PlayerClock *c, int *queue_sreial) : speed(1.0), paused(0), queue_serial(queue_sreial)
{
    set_clock(c, NAN, -1);
}

PlayerClock::~PlayerClock()
{

}


// 返回值：返回上一帧的pts更新值（上一帧的pts + 流逝的时间）
double PlayerClock::get_clock(PlayerClock *c)
{
    if(*c->queue_serial != c->serial) // 若传入的帧序列与帧队列的序列不同则返回NAN
    {
        return NAN;
    }
    if(c->paused) // 若在暂停播放播放状态则返回当前帧的播放时间
    {
        return c->pts;
    }
    else // 主流程
    {
//        获取从某个未指定的起始点开始的当前时间(以微秒为单位)
        double time = av_gettime_relative() / 1000000.0; // 获取当前系统时间
        double ret = c->pts_drift/*当前帧的显示时间戳与系统时间的差值*/ + time; // 展开得： c->pts + (time - c->last_updated)
        return ret;
    }
}

void PlayerClock::set_clock_at(PlayerClock *c, double pts, int serial, double time)
{
    c->pts = pts; // 设置包队列的数据包解码后需要显示的时间
    c->last_updated = time; // 设置当前时钟时间
    c->pts_drift = c->pts - time; // 设置当前显示的帧的时间与系统时间的差值
    c->serial = serial; // 时钟指向的包队列的序列与输入的包队列的序列一致
}

void PlayerClock::set_clock(PlayerClock *c, double pts, int serial)
{
    double time = av_gettime_relative() / 1000000.0; // 系统时间（微秒转秒）
    set_clock_at(c, pts, serial, time);
}

void PlayerClock::init_clock(PlayerClock *c, int *queue_serial)
{
    c->speed = 1.0; // 分配速度
    c->paused = 0;  // 直接播放
    c->queue_serial = queue_serial;     // 时钟的包队列序列与输入的包序列一致
    set_clock(c, NAN, -1);              // 初始化时钟的解码后显示时间为NAN，序列为-1
}

void PlayerClock::set_clock_speed(PlayerClock *c, double speed)
{
    set_clock(c, get_clock(c), c->serial);
    c->speed = speed;
}

void PlayerClock::sync_play_clock_to_slave(PlayerClock *c, PlayerClock *slave)
{
    double clock = get_clock(c);
    double slave_clock = get_clock(c);
    if(!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
        set_clock(c, slave_clock, slave->serial);
}

