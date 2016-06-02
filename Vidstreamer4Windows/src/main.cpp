#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <regex>
#include <iostream>

#include <iostream>
#include <vector>
#include "DefaultCapSender.h"


typedef enum MENU_STATES { STATE_STREAM_INIT, STATE_STREAM_SENDING, STATE_QUIT};


void flushcin()
{
	std::cin.clear();
	fflush(stdin);
}

int main()
{
	MENU_STATES current_state = STATE_STREAM_INIT;
	DefaultCapSender* lcs = NULL;
	std::vector<std::string>* sessions = NULL;

	int resp = -1;
	int vidbitrate = -1;
	int audbitrate = -1;
	bool quit = false;
	int ses = -1;

	std::string udp_add, serv_add, username, port, serv_ip;
	flushcin();

	while (!quit)
	{
		switch (current_state)
		{
		case STATE_STREAM_INIT:
			std::cout << "Destination (IP:PORT):\n";
			std::cin >> udp_add;
			udp_add = "udp://" + udp_add;
			flushcin();
			std::cout << "Video Bitrate: \n";
			std::cin >> vidbitrate;
			flushcin();
			std::cout << "Audio Bitrate:\n";
			std::cin >> audbitrate;
			flushcin();

			lcs = new DefaultCapSender(udp_add, vidbitrate, audbitrate);
			lcs->start();
			current_state = STATE_STREAM_SENDING;
			break;


		case STATE_STREAM_SENDING:
			std::cout << "Sending... Type '0' to quit\n";
			std::cin >> resp;
			flushcin();
			current_state = resp == 0 ? STATE_QUIT : STATE_STREAM_SENDING;
			break;

		case STATE_QUIT:
			quit = true;
			break;
		}
	}
}