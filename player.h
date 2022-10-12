#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include "common.h"
#include "demux.h"
#include "video.h"
#include "audio.h"




class Player
{
public:
    Player();

    int playing_running(const std::string p_input_file);
    PlayerStat * player_init(const char * p_input_file);
    static int player_deinit(PlayerStat *is);
    static void stream_toggle_pause(PlayerStat *is);
    static void toggle_pause(PlayerStat *is);
    static void do_exit(PlayerStat *is);
};

#endif // PLAYER_H
