#pragma once


#include <OVDataEnvelopMaker.h>
#include <OVPipeServerManager.h>

#include <iterator>
#include <iostream>
#include <atomic>
#include <thread>
#include <fstream>
#include <map>
#include <set>

#include <boost/filesystem.hpp>




typedef std::pair<HANDLE, std::thread> service;

class Server :
	public ov_pipe_svr_manager::COVPipeServerManager< Server, SPS_ACTIONPACKET >,
	public COVEnvelopMaker<Server, SPS_ACTIONPACKET> {

	typedef SPS_ACTIONPACKET_HEADER action;
	typedef binary_buffer_space::c_bindata_bufer databuff;

	struct Session {
		uint32_t id;
		bool stream;
	};


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

public:
	Server(const std::string &name, const std::string& path);
	~Server();

	size_t start(size_t connections = 1);

void stop();

private:
	typedef SPS_ACTIONPACKET_HEADER action;
	typedef binary_buffer_space::c_bindata_bufer databuff;


	// обертка для отправки сообщения
	bool sendData(HANDLE stream, uint32_t actionId, SPS_ACTIONPACKET * packet, unsigned char* data, int size);

	// событие, когда клиент запрашивает начало трансляции
	bool handle(SPS_ACTIONPACKET *packet, unsigned long /*error*/, HANDLE pipe, HANDLE /*reserved*/);

	// непосредственная передача изображений
	void translate(SPS_ACTIONPACKET packet, HANDLE hPipe);

	// событие "паузы" от клиента
	bool stopHandler(SPS_ACTIONPACKET *packet, unsigned long /*error*/, HANDLE pipe, HANDLE /*reserved*/);



	void onDisconnect(HANDLE pipe);

private:
	std::atomic<bool> shutdown{ false };
	std::map<HANDLE, Session> streamMap;

	std::map<HANDLE, std::thread> pool;

	boost::filesystem::path path;
	std::mutex mtx;
	std::string m_name;
};

