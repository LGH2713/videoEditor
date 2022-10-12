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
    player->playing_running();
}

