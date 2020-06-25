#include "pipeclient.h"


    PipeClient::PipeClient(std::string name) : m_name(name)
    {
        pcLinkSpitOutHandler(&PipeClient::MakeEnvelop);
        pcLinkEnvelopCleaner(&PipeClient::ClearEnvelop);
        pcLinkActionHandler(command::CMD_TOTALSIZE, &PipeClient::getTotal);
        pcLinkActionHandler(command::CMD_TOTALPACKNUM, &PipeClient::getCount);
        pcLinkActionHandler(command::CMD_PACKSIZE, &PipeClient::getCurr);
        pcLinkActionHandler(command::CMD_IMGDATA, &PipeClient::getImgData);
        pcLinkServerLostHandler([this](PipeClient& cls){
            qDebug() << "[This client was disconnected]";
            emit disconnectEvent();
        });
    }
    PipeClient::~PipeClient()
    {
        this->disconnect();
    }

    bool PipeClient::connect(unsigned timeoutMsec)
    {
        wstring wideName(m_name.cbegin(), m_name.cend());
        auto result = pcConnect(wideName.c_str(), timeoutMsec);
        return OV_CLIENT_SIDE_CONNECTED == result;
    }

    bool PipeClient::send(PipeClient::command cmd)
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

    void PipeClient::disconnect()
    {
        pcDisconnect(true);
    }

       // получить общий размер в байтах
    bool PipeClient::getTotal(SPS_ACTIONPACKET *packet, unsigned long /*error*/) {
        packet->pIStmCompletionBuffer->Read(reinterpret_cast<char*>(&total), packet->nMetadataSize, nullptr);
        image.resize(total);
        count = tempTotal = tempCount = 0;

        qDebug() << "[Received total size of picture: " << total << "]\n";
        return true;
    }

     // получить кол-во пакетов на прием
    bool PipeClient::getCount(SPS_ACTIONPACKET *packet, unsigned long /*error*/) {
        packet->pIStmCompletionBuffer->Read(reinterpret_cast<char*>(&count), packet->nMetadataSize, nullptr);
        qDebug() << "[Received total package count: " << count << "]\n";

        return true;
    }

        // получить сам кусок изображения
    bool PipeClient::getImgData(SPS_ACTIONPACKET *packet, unsigned long /*error*/) {
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
    bool PipeClient::getCurr(SPS_ACTIONPACKET *packet, unsigned long /*error*/) {
        packet->pIStmCompletionBuffer->Read(reinterpret_cast<char*>(&curr), packet->nMetadataSize, nullptr);

        qDebug() << "[Received size of next package " << curr << "]\n";
        return true;
    }






