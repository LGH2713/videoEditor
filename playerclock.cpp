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
    if(*c->queue_serial != c->serial) {
        return NAN;
    }
    if(c->paused) {
        return c->pts;
    }
    else {
//        获取从某个未指定的起始点开始的当前时间(以微秒为单位)
        double time = av_gettime_relative() / 1000000.0;
        double ret = c->pts_drift + time; // 展开得： c->pts + (time - c->last_updated)
        return ret;
    }
}

void PlayerClock::set_clock_at(PlayerClock *c, double pts, int serial, double time)
{
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    c->serial = serial;
}

void PlayerClock::set_clock(PlayerClock *c, double pts, int serial)
{
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, serial, time);
}

void PlayerClock::init_clock(PlayerClock *c, int *queue_serial)
{
    c->speed = 1.0;
    c->paused = 0;
    c->queue_serial = queue_serial;
    set_clock(c, NAN, -1);
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

