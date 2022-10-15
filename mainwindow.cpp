#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "player.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_fileSelect_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(
                this,
                tr("open a file"),
                "E:/",
                tr("video files(*.avi *.mp4 *.wmv *.h265 *.mkv);All files(*.*)"));
    ui->fileName->setText(fileName);
}


void MainWindow::on_playButton_clicked()
{
    std::string fileName = ui->fileName->text().toStdString();
    Player *player = new Player(fileName);
    if(!player->input_filename.c_str())
    {
        return;
    }

    player->player_init(player->input_filename.c_str());
    if(player == nullptr)
    {
        std::cout << "player init failed\n" << std::endl;
        Player::do_exit(player->is);
    }

    player->demux->open_demux(player->is); // 开启解复用线程
    player->video->open_video_stream(player->is); // 开启视频流
    player->audio->open_audio_stream(player->is); // 开启音频流

    player->video->open_video_playing(player->is); // 开启视频播放open_video_playing(is); // 开启视频播放
    // 1. 创建SDL窗口，SDL2.0支持多窗口
    //      SDL_Window即运行程序后弹出的视频窗口
//    player->is->sdl_video.window = SDL_CreateWindow("simple ffplayer",
//                                            SDL_WINDOWPOS_UNDEFINED, // 不关心窗口的X坐标
//                                            SDL_WINDOWPOS_UNDEFINED, // 不关心窗口的Y坐标
//                                            player->is->sdl_video.rect.w,
//                                            player->is->sdl_video.rect.h,
//                                            SDL_WINDOW_OPENGL
//                                            );

    player->is->sdl_video.window = SDL_CreateWindowFrom((void *)ui->player->winId());

    // 2. 创建SDL_Renderer
    //      SDL_Renderer: 渲染器
    if(player->is->sdl_video.window == nullptr)
    {
        std::cout << "SDL_CreateWindow() failed: " << SDL_GetError() << std::endl;
        return ;
    }

    player->is->sdl_video.renderer = SDL_CreateRenderer(player->is->sdl_video.window, -1, 0);
    if(player->is->sdl_video.renderer == nullptr)
    {
        std::cout << "SDL_CreateRenderer() failed: " << SDL_GetError() << std::endl;
        return ;
    }

    // 3. 创建SDL_Texture
    //      一个SDL_Texture对应一帧YUV数据
    player->is->sdl_video.texture = SDL_CreateTexture(player->is->sdl_video.renderer,
                                              SDL_PIXELFORMAT_IYUV,
                                              SDL_TEXTUREACCESS_STREAMING,
                                              player->is->sdl_video.rect.w,
                                              player->is->sdl_video.rect.h
                                              );

    if(player->is->sdl_video.texture == nullptr)
    {
        std::cout << "SDL_CreateTexture() failed: " << SDL_GetError() << std::endl;
        return ;
    }

    SDL_CreateThread(player->video->video_playing_thread, "video playing thread", player->is);


    player->audio->open_audio_playing(player->is); // 开启音频播放

    player->playing_running();
}

