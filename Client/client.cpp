#ifndef CLIENT_H
#define CLIENT_H

#include <QGraphicsPixmapItem>
#include <QtCore>
#include "winapiPipe/OVDataEnvelopMaker.h"
#include "winapiPipe/OVPipeClientManager.h"

using namespace std;
#include <string>
#include <iostream>
#include <vector>
#include <QDebug>
#include <memory>

class Client :
    public QObject,
    public ov_pipe_client_manager::COVPipeClientManager< Client, SPS_ACTIONPACKET >,
    public COVEnvelopMaker< Client, SPS_ACTIONPACKET >

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
    Client(std::string name) : m_name(name)
    {
        pcLinkSpitOutHandler(&Client::MakeEnvelop);
        pcLinkEnvelopCleaner(&Client::ClearEnvelop);
        pcLinkActionHandler(command::CMD_TOTALSIZE, &Client::getTotal);
        pcLinkActionHandler(command::CMD_TOTALPACKNUM, &Client::getCount);
        pcLinkActionHandler(command::CMD_PACKSIZE, &Client::getCurr);
        pcLinkActionHandler(command::CMD_IMGDATA, &Client::getImgData);
        pcLinkServerLostHandler([this](Client& cls){
            qDebug() << "[This client was disconnected]";
            emit disconnectEvent();
        });
    }
    ~Client()
    {
        this->disconnect();
    }
public:
    bool connect(unsigned timeoutMsec = 5000)
    {
        wstring wideName(m_name.cbegin(), m_name.cend());
        auto result = pcConnect(wideName.c_str(), timeoutMsec);
        return OV_CLIENT_SIDE_CONNECTED == result;
    }

    bool send(Client::command cmd)
    {
        binary_buffer_space::c_bindata_bufer packet;
        SPS_ACTIONPACKET_HEADER header;
        header.dwClientLabel = 1;
        GetSystemTimeAsFileTime(&header.ftTimeStamp);
        header.nActionId = cmd;
        header.nMetadataSize = sizeof(command);
                // prepare data for package
        packet.write(&header, sizeof(header));
        packet.write(reinterpret_cast<unsigned char *>(&cmd), sizeof(cmd));

                // get package buffer
        auto code = pcSend(reinterpret_cast<unsigned char *>(packet.get_bufer()), packet.get_bufer_size());
        bool result = ov_pipe_client_manager::csr_all_ok == code;
        if (!result)
        {
            cerr << "Send error: " << code << endl;
        }
        return result;
    }

    void disconnect()
    {
        pcDisconnect(true);
    }

       // получить общий размер в байтах
    bool getTotal(SPS_ACTIONPACKET *packet, unsigned long /*error*/) {
        packet->pIStmCompletionBuffer->Read(reinterpret_cast<char*>(&total), packet->nMetadataSize, nullptr);
        image.resize(total);
        count = tempTotal = tempCount = 0;

        qDebug() << "[Received total size of picture: " << total << "]\n";
        return true;
    }

     // получить кол-во пакетов на прием
    bool getCount(SPS_ACTIONPACKET *packet, unsigned long /*error*/) {
        packet->pIStmCompletionBuffer->Read(reinterpret_cast<char*>(&count), packet->nMetadataSize, nullptr);
        qDebug() << "[Received total package count: " << count << "]\n";

        return true;
    }

        // получить сам кусок изображения
    bool getImgData(SPS_ACTIONPACKET *packet, unsigned long /*error*/) {
        tempTotal += packet->nMetadataSize;
        qDebug() << "[Received image data with size: " << packet->nMetadataSize << "]\n";
        packet->pIStmCompletionBuffer->Read(&image[0] + (tempCount * 1024), packet->nMetadataSize, nullptr);
        qDebug() << "[At the moment: " << tempCount++ << " packages]\n";

        if (tempTotal == total) {
            QByteArray qarr(reinterpret_cast<char*>(image.data()), total);
            qDebug() << "[Totally transferred " << tempTotal << " bytes]\n";
            QPixmap tempix;
            tempix.loadFromData(qarr, "BMP");

            emit gotImage(tempix);

            image.clear();
            count = total = tempTotal = tempCount = curr =  0;

            qDebug() << "[Received full picture]\n";
            qDebug() << tempix.width() << " width, " << tempix.height() << " height\n";

        }
        return true;
    }


        // получить размер пакета на последующий прием
    bool getCurr(SPS_ACTIONPACKET *packet, unsigned long /*error*/) {
        packet->pIStmCompletionBuffer->Read(reinterpret_cast<char*>(&curr), packet->nMetadataSize, nullptr);

        qDebug() << "[Received size of next package " << curr << "]\n";
        return true;
    }


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



#endif // CLIENT_H
