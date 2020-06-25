#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsView>
#include <QThread>


#include <boost/process/pipe.hpp>
#include <atomic>
#include <memory>

//#include "Client.h"
#include "pipeclient.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void on_btnConnect_clicked();

    void on_btnStart_clicked();

    void on_btnDisconnect_clicked();

    void on_btnStop_clicked();

    void onImageReceived(QPixmap pixie);

    void onDisconnect();
private:

    Ui::MainWindow* ui;

    std::unique_ptr<QGraphicsScene> scene;
    std::unique_ptr<QGraphicsPixmapItem> pixItem;

    bool connected  {false};

    std::atomic<bool> isReceiving;

    //std::unique_ptr<Client> cls;
    std::unique_ptr<PipeClient> cls;

};
#endif // MAINWINDOW_H
