#ifndef PIPECLIENT_H
#define PIPECLIENT_H

#include <QGraphicsPixmapItem>
#include <QtCore>
#include <OVDataEnvelopMaker.h>
#include <OVPipeClientManager.h>


using namespace std;
#include <string>
#include <iostream>
#include <vector>
#include <QDebug>
#include <memory>

class PipeClient :
    public QObject,
    public ov_pipe_client_manager::COVPipeClientManager< PipeClient, SPS_ACTIONPACKET >,
    public COVEnvelopMaker< PipeClient, SPS_ACTIONPACKET >

{
    Q_OBJECT

public:
    enum command {
        CMD_START = 1,
        CMD_STOP,
        CMD_BREAK,
        CMD_TOTALSIZE,
        CMD_TOTALPACKNUM,
        CMD_IMGDATA,
        CMD_PACKSIZE,
        CMD_DISCONNECT
    };
    PipeClient(std::string name);
    ~PipeClient();
public:
    bool connect(unsigned timeoutMsec = 5000);
    bool send(PipeClient::command cmd);


    void disconnect();

       // получить общий размер в байтах
    bool getTotal(SPS_ACTIONPACKET *packet, unsigned long /*error*/);

     // получить кол-во пакетов на прием
    bool getCount(SPS_ACTIONPACKET *packet, unsigned long /*error*/);

        // получить сам кусок изображения
    bool getImgData(SPS_ACTIONPACKET *packet, unsigned long /*error*/);


        // получить размер пакета на последующий прием
    bool getCurr(SPS_ACTIONPACKET *packet, unsigned long /*error*/);


signals:
    void gotImage(QPixmap image);

    void disconnectEvent();

private:
    string m_name;

    long total = 0;
    long count = 0;
    long curr = 0;
    long tempTotal = 0;
    long tempCount = 0;

    std::vector<char> image;

};



#endif // PIPECLIENT_H
