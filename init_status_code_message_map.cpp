#include "webserv.hpp"

std::map<int, std::string> init_status_code_message_map()
{
    std::map<int, std::string> _map;

	_map[200] = "200 OK";
	_map[204] = "204 No Content";
    _map[400] = "400 Bad Request";
    _map[403] = "403 Forbidden";
    _map[404] = "404 Not Found";
    _map[405] = "405 Method Not Allowed";
    _map[410] = "410 Gone";
    _map[413] = "413 Request Entity Too Large";
    _map[500] = "500 Internal Server Error";
    return _map;
}