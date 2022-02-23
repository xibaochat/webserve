#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>

#include <iostream>
#include <fstream>

#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

int		which_port(void)
{
	std::string		choice;

	std::cout << YELLOW << "Which PORT :" << std::endl;
	std::cout << "(a) 8000" << std::endl;
	std::cout << "(b) 8001" << std::endl;
	std::cout << "(c) 8080" << RESET << std::endl;
	getline(std::cin, choice);

	if (choice == "a")
		return (8000);
	else if (choice == "b")
		return (8001);
	else if (choice == "c")
		return (8080);
	return (8000);
}

std::string		which_method(void)
{
	std::string		choice;

	std::cout << std::endl << YELLOW << "Which METHOD :" << std::endl;
	std::cout << "(a) GET" << std::endl;
	std::cout << "(b) POST" << std::endl;
	std::cout << "(c) DELETE" << std::endl;
	getline(std::cin, choice);

	if (choice == "a")
		return ("get/GET");
	else if (choice == "b")
		return ("post/POST");
	else if (choice == "c")
		return ("delete/DELETE");
	return ("get/GET");
}

std::string		which_target(int cgi)
{
	std::string		choice;

	std::cout << std::endl << YELLOW << "Which TARGET FILE :" << std::endl;
	std::cout << "(a) Exists" << std::endl;
	std::cout << "(b) Doesn't exist" << std::endl;
	std::cout << "(c) Wrong permissions" << std::endl;
	std::cout << "(d) Bad request" << RESET << std::endl;
	if (cgi)
		std::cout << YELLOW << "(e) CGI (on)" << RESET << std::endl;

	getline(std::cin, choice);

	if (choice == "a")
		return ("_example");
	else if (choice == "b")
		return ("_nofile");
	else if (choice == "c")
		return ("_permission");
	else if (choice == "d")
		return ("_bad");
	else if (cgi && choice == "e")
		return ("_cgi");
	return ("_example");
}

void	send(int port, std::string filename)
{
	int					sock;
	struct sockaddr_in	serv_addr;
	char				buffer[4096] = {0};
	std::fstream		file;
	std::string			content;

	file.open(filename);
	content.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	file.close();

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		std::cerr << std::endl << RED << "< Socket creation error >" << RESET << std::endl << std::endl;
		return ;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);


	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		std::cout << std::endl << RED << "< Connection failed >" << RESET << std::endl << std::endl;
		return ;
	}

	content += "\r\n";

	std::cout << std::endl << "Sending :" << std::endl;
	std::cout << "[" << RED << content << RESET << "]" << std::endl << std::endl;

	send(sock, content.c_str(), content.size(), 0);
	read(sock, buffer, 4095);

	std::cout << std::endl << "Response :" << std::endl;
	std::cout << "[" << GREEN << std::string(buffer) << RESET << "]" << std::endl << std::endl;

	close(sock);

	return ;
}

int		main()
{
	int				port;
	std::string		filename;

	while (true)
	{
		port = which_port();
		filename = "test_this/request/";
		filename += which_method();
		if (filename.find("get") != std::string::npos || filename.find("post") != std::string::npos || filename.find("put") != std::string::npos)
			filename += which_target(1);
		else
			filename += which_target(0);
		send(port, filename);
	}

	return (0);
}