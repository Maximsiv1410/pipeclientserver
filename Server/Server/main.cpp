// Server.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Server.h"




int main(int argc, char* argv[])
{

	if (argc < 2) {
		std::cerr << "Usage: <progname> <picture-folder>\n";
		return 1;
	}

    Server server(R"(\\.\pipe\testpipe)", argv[1]);
    server.start(5);
	std::cout << "Press any key to stop server..." << std::endl;
	std::cin.get();
    server.stop();
    return 0;
}

