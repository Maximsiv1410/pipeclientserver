#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <string>
#include "pipeclient.h"

#include <thread>
#include <QDebug>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    //this->setFixedSize(314, 348);
    ui->setupUi(this);

    scene.reset(new QGraphicsScene(0, 0, 200, 200, this));
    cls.reset(new PipeClient(R"(\\.\pipe\testpipe)"));
    connect(cls.get(), &PipeClient::gotImage, this, &MainWindow::onImageReceived);
    connect(cls.get(), &PipeClient::disconnectEvent, this, &MainWindow::onDisconnect);
}

MainWindow::~MainWindow()
{
    qDebug() << (cls->disconnect()
                ? "[Successfully disconnected]\n"
                : "[Failed to disconnect]\n");
   delete ui;
}

void MainWindow::onDisconnect() {
    cls->disconnect();
    ui->btnStart->setEnabled(false);
    ui->btnStop->setEnabled(false);
    ui->btnDisconnect->setEnabled(false);

    ui->btnConnect->setEnabled(true);
    connected = false;
}

void MainWindow::on_btnConnect_clicked()
{
    if (!connected) {
        if (!cls->connect()) {
            QMessageBox::about(this, "Info", "Failed to connect");
        }
        else {
            QMessageBox::about(this, "Info", "Connected");
            connected = true;
            ui->btnStart->setEnabled(true);
            ui->btnDisconnect->setEnabled(true);

            ui->btnConnect->setEnabled(false);
        }
    }
    else {
        QMessageBox::about(this, "Info", "You are already connected!");
    }
}

void MainWindow::on_btnStart_clicked()
{
    if (connected){
        PipeClient::command cmd = PipeClient::command::CMD_START;
        cls->send(cmd);

        ui->btnStart->setEnabled(false);
        ui->btnStop->setEnabled(true);

        qDebug() << "sent start command\n";
    }

}

void MainWindow::on_btnDisconnect_clicked()
{
    if (connected) {
        ui->btnStart->setEnabled(false);
        ui->btnStop->setEnabled(false);
        ui->btnDisconnect->setEnabled(false);

        ui->btnConnect->setEnabled(true);
    }
    cls->disconnect();
    connected = false;
}

void MainWindow::on_btnStop_clicked()
{
    if (connected) {
        cls->send(PipeClient::command::CMD_BREAK);
        ui->btnStop->setEnabled(false);
        ui->btnStart->setEnabled(true);
    }
}

void MainWindow::onImageReceived(QPixmap pixmap) {
    qDebug() << "===== FORM RECEIVED IMAGE =====\n";
    scene->setSceneRect(0, 0, pixmap.width(), pixmap.height());
    pixItem.reset(new QGraphicsPixmapItem(pixmap));
    ui->graphicsView->setScene(scene.get());
    scene->addItem(pixItem.get());
}
