#ifndef SERVER_HPP
# define SERVER_HPP

#include <sstream>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>

class server
{
public:
	server();
	~server();
	std::string file;
	std::string status_code;
	int total_nb;
	std::string total_line;
	std::string content_type;





};

#endif
