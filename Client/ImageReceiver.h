#ifndef IMAGERECEIVER_H
#define IMAGERECEIVER_H


#include <QtCore>
#include <functional>
#include <QGraphicsPixmapItem>
#include <QDebug>

#include "commands.h"
#include <boost/process/pipe.hpp>
#include <windows.h>
#include <thread>
#include <atomic>

class ImageReceiver : public QObject {
    Q_OBJECT
public:
    ImageReceiver(std::atomic<bool>* StopFlag, HANDLE hPipe) : Stop(StopFlag) {
        this->hPipe = hPipe;
    }
    ~ImageReceiver(){}
public slots:

    void process() {

        boost::process::pipe pipe;
        pipe.assign_source(hPipe);
        pipe.assign_sink(hPipe);


        char buff[12];
        command comm = command::start;
        pipe.write((char*)&comm, sizeof(comm));

        pipe.read(buff, sizeof(buff));

        while(true) {
            if (!Stop->load(std::memory_order_relaxed)) {
                pipe.assign_source(INVALID_HANDLE_VALUE);
                pipe.assign_sink(INVALID_HANDLE_VALUE);
                qDebug() << "received interrupt\n";
                emit processFinished();
                return;
            }
            else {
                qDebug() << "no.";
            }
            command comm = command::empty;
            pipe.read((char*)&comm, sizeof(command));
            //read
            if (command::stop == comm) {
                pipe.assign_source(INVALID_HANDLE_VALUE);
                pipe.assign_sink(INVALID_HANDLE_VALUE);
                emit processFinished();
                return;
            }

            if (command::receive == comm) {
                char* buffer = nullptr;
                try {
                    // get number of packages to receive later
                    long packNum = 0;
                    pipe.read((char*)&packNum, sizeof(long));

                    // get total size of file
                    long totalSize = 0;
                    pipe.read((char*)&totalSize, sizeof(long));

                    // allocate buffer
                    buffer = new char[totalSize];

                    int total_read = 0;
                    for (long i = 0; i < packNum; ++i) {
                        long currPack = 0;
                        pipe.read((char*)&currPack, sizeof(long));
                        total_read += pipe.read(buffer + (i * 1024), currPack);
                    }


                    // send information about action
                    // 1. receive
                    // 2. stop

                    QByteArray qarr(reinterpret_cast<char*>(buffer), totalSize);
                    QPixmap tempix;
                    tempix.loadFromData(qarr, "BMP");

                    emit imageReceived(tempix);
                    delete[] buffer;
                }
                catch (...) {
                    //std::cout << pe.what() << "\n";
                    if (buffer) delete[] buffer;
                    return;
                }
            }

        }
    }


   // void onInterruptRequested() {
     //   qDebug() << "emitted interrupt\n";
   // }

signals:
    void imageReceived(QPixmap);

    void processFinished();

private:
   // bool *Stop;
    std::atomic<bool>* Stop;
    HANDLE hPipe;
};



#endif // IMAGERECEIVER_H
