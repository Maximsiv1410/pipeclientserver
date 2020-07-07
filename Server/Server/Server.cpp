#include "stdafx.h"
#include "Server.h"



Server::Server(const std::string &name, const std::string& path) : m_name(name), path(path) {
	std::wstring wideName(name.cbegin(), name.cend());
	psSetPipeId(wideName.c_str());

	psSetSpitOutHandler(&Server::MakeEnvelop);
	psLinkEnvelopCleaner(&Server::ClearEnvelop);
	psLinkAction(CMD_START, &Server::handle);
	psLinkAction(CMD_BREAK, &Server::stopHandler);

	

	psLinkOnDisconnectHandler(&Server::onDisconnect);
}

Server::~Server() {
	stop();
}

size_t Server::start(size_t connections) {
	size_t threads = 0;
	for (int i = 0; i < connections; i++) {
		auto id = psRun();
		if (0 == id) {
			std::cerr << "Pipe connection isn't started" << std::endl;
		}
		else {
			++threads;
		}
	}
	return threads;
}

void Server::stop() {
	if (shutdown.load(std::memory_order_relaxed)) {
		return;
	}

	shutdown.store(true, std::memory_order_relaxed);
	{
		command cmd = command::CMD_DISCONNECT;
		std::unique_lock<std::mutex> ulock(mtx);
		// отправить всем клиентам уведомление об отключении
		// а так же прекратить трансляцию данных
		for (auto &flag : streamMap) {
			flag.second.stream = false;
		}
	}

	auto count = psStopOverall();

	std::cout << "Threads stopped: " << count << std::endl;
}



// обертка для отправки сообщения
bool Server::sendData(HANDLE stream, uint32_t actionId, SPS_ACTIONPACKET * packet, unsigned char* data, int size) {
	action header;
	databuff responsePacket;
	header.dwClientLabel = packet->dwClientLabel;
	header.nActionId = actionId;
	GetSystemTimeAsFileTime(&header.ftTimeStamp);
	header.nMetadataSize = size;
	responsePacket.write(&header, sizeof(header));
	responsePacket.write(data, size);

	return SendAnswer(
		reinterpret_cast<unsigned char *>(responsePacket.get_bufer()),
		responsePacket.get_bufer_size(), stream, 0);
}

// событие, когда клиент запрашивает начало трансляции
bool Server::handle(SPS_ACTIONPACKET *packet, unsigned long /*error*/, HANDLE pipe, HANDLE /*reserved*/) {
	if (shutdown.load(std::memory_order_relaxed))
		return false;

	{
		std::unique_lock<std::mutex> ulock(mtx);
		auto pos = streamMap.find(pipe);

		streamMap[pipe] = Session{ packet->dwClientLabel, true };

		std::cout << "[Starting translation for client No. " << packet->dwClientLabel << "]\n";
		pool.emplace(pipe, std::thread(&Server::translate, this, *packet, pipe));
	}


	return true;
}

// непосредственная передача изображений
void Server::translate(SPS_ACTIONPACKET packet, HANDLE hPipe) {
	SPS_ACTIONPACKET_HEADER header;
	binary_buffer_space::c_bindata_bufer responsePacket;
	command cmd;
	int fps = 1;
	int millis = 1000;
	int interval = millis / fps;
	int i = 1;


	// translate all data
	for (auto & file : boost::filesystem::directory_iterator{ path }) {
		// check if we should exit
		if (shutdown.load(std::memory_order_relaxed)) {
			std::cout << "Stopping thread id...\n";
			return; // stopped
		}

		std::ifstream ifs(file.path().c_str(), std::ios::binary);
		std::vector<char> image{
			std::istreambuf_iterator<char>(ifs),
			std::istreambuf_iterator<char>()
		};
		ifs.close();

		long buffSize = 1024L;
		long last = (long)image.size() % 1024L;
		long full = ((long)image.size() - last) / 1024L;
		long total = last ? full + 1L : full;

		auto transferred = sendData(hPipe, CMD_TOTALPACKNUM, &packet, reinterpret_cast<unsigned char*>(&total), sizeof(long));

		long imgsz = image.size();

		transferred = sendData(hPipe, CMD_TOTALSIZE, &packet, reinterpret_cast<unsigned char*>(&imgsz), sizeof(long));

		for (int i = 0; i < full; ++i) {
			transferred = sendData(hPipe, CMD_PACKSIZE, &packet, reinterpret_cast<unsigned char*>(&buffSize), sizeof(long));

			transferred = sendData(hPipe, CMD_IMGDATA, &packet,
								   reinterpret_cast<unsigned char*>(image.data() + i * buffSize), buffSize);
		}

		if (last) {
			transferred = sendData(hPipe, CMD_PACKSIZE, &packet, reinterpret_cast<unsigned char*>(&last), sizeof(long));

			transferred = sendData(hPipe, CMD_IMGDATA, &packet,
								   reinterpret_cast<unsigned char*>(image.data() + full * buffSize), last);
		}
		std::cout << "=====\tSent " << i++ << " item for ID: " << packet.dwClientLabel << "\t=====\n";




		std::this_thread::sleep_for(std::chrono::milliseconds(interval));

		{
			std::unique_lock<std::mutex> ulock(mtx);
			auto pos = streamMap.find(hPipe);
			if (pos != streamMap.end()) {
				if (pos->second.stream == false) {
					std::cout << "[Stopping translation for client No. " << packet.dwClientLabel << "]\n";
					return;
				}
			}
			else {
				std::cout << "[Client " << packet.dwClientLabel << " was disconnected, interrupting translation]\n";
				return;
			}
		}
	}


	// возможно, стоит отключать клиента, если больше нет потока изображений
	std::cout << "[There's no more files to send for client " << packet.dwClientLabel << "]\n";
	std::thread(&Server::disconnect_pipe, this, hPipe).detach();

}

// событие "паузы" от клиента
bool Server::stopHandler(SPS_ACTIONPACKET *packet, unsigned long /*error*/, HANDLE pipe, HANDLE /*reserved*/) {
	std::unique_lock<std::mutex> ulock(mtx);
	auto pos = streamMap.find(pipe);
	if (pos != streamMap.end()) {
		pos->second.stream = false;
		ulock.unlock();
		auto service = pool.find(pipe);
		if (service != pool.end()) {
			if (service->second.joinable())
				service->second.join();
			pool.erase(service);
		}
	}

	return true;
}



void Server::onDisconnect(HANDLE pipe) {
	std::unique_lock<std::mutex> ulock(mtx);
	auto pos = streamMap.find(pipe);
	if (pos != streamMap.end()) {
		std::cout << "[Disconnected client-side. ID: " << pos->second.id << "]\n";
		streamMap.erase(pos);
		ulock.unlock();
		auto service = pool.find(pipe);
		if (service != pool.end()) {
			if (service->second.joinable())
				service->second.join();
			pool.erase(service);
		}
	}
}