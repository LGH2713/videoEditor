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
    Player(std::string input_filename);

    int playing_running();
    void player_init(const char * p_input_file);
    static int player_deinit(PlayerStat *is);
    static void stream_toggle_pause(PlayerStat *is);
    static void toggle_pause(PlayerStat *is);
    static void do_exit(PlayerStat *is);

public:
    std::string input_filename;
    PlayerStat *is;
    Demux *demux;
    Video *video;
    Audio *audio;
};

#endif // PLAYER_H
