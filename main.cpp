#include "mainwindow.h"

#include <QApplication>

extern "C" {
#include "SDL.h"
#undef main
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    if(SDL_Init(SDL_INIT_VIDEO)) {
        printf( "Could not initialize SDL - %s\n", SDL_GetError());
    } else{
        printf("Success init SDL");
    }
    return 0;
    w.show();
    return a.exec();
}
