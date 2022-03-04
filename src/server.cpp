#include "server.hpp"
#include "webserv.hpp"

int get_total_port(std::vector<Conf> &v)
{
	int total_port = 0;
	for (std::vector<Conf>::iterator it = v.begin() ; it != v.end(); ++it)
		total_port += (*it).port.size();
	return total_port;
}


std::set<int> get_all_port_nb_in_set(std::vector<Conf> &v)
{
	std::set<int> port;
	for (std::vector<Conf>::iterator it = v.begin() ; it != v.end(); ++it)
	{
		std::set<int> p = (*it).port;
		for (std::set<int>::iterator it_set = p.begin() ; it_set != p.end(); ++it_set)
			port.insert(*it_set);
	}
	return port;
}

/*Constructor of class Server*/
Server::Server(std::vector<Conf> &web_conf_vector)
{
	//get a set of port from multi server
	this->web_conf_vector = web_conf_vector;
	this->port = get_all_port_nb_in_set(this->web_conf_vector);

	this->serverAddr = new struct sockaddr_in[this->port.size()];
	std::set<int>::iterator it=this->port.begin();
	int i = 0;
	while(it!=this->port.end())
	{
		memset(this->serverAddr[i].sin_zero, '\0', sizeof this->serverAddr[i].sin_zero);
		this->serverAddr[i].sin_family = AF_INET;
		this->serverAddr[i].sin_addr.s_addr = INADDR_ANY;
		this->serverAddr[i].sin_port = htons(*it);
		it++;
		i++;
	}
	this->listener = new int[this->port.size()];
	this->epfd = 0;
}

/* create sockfd fd of endpoint, bind and listen, add sockfd to interest list of epoll*/
void Server::Init()
{
	int opt = 1;
	int size = this->port.size();
	int i = 0;

	/*the size argument is ignored, but must be greater than zero*/
	this->epfd = epoll_create(5000);
	if (this->epfd < 0)
		throw("[ERROR]epoll create error");
	while (i < size)
	{
		this->listener[i] = socket(AF_INET, SOCK_STREAM, 0);
		if (this->listener[i] < 0)
			throw("[ERROR]Failed to create socket fd");
	/*even stop the program and restart it, the port is still available.*/
		if (setsockopt(this->listener[i], SOL_SOCKET, SO_REUSEADDR, (const void*)&opt, sizeof(int)) < 0)
			throw("setsockopt(SO_REUSEADDR) failed");
		if (bind(this->listener[i], (struct sockaddr *)&this->serverAddr[i], sizeof(this->serverAddr[i])) < 0)
			throw("[ERROR]Failed to bind");
		if (listen(this->listener[i], 5000) < 0) //maximum length to which the  queue  of pending  connections  for sockfd may grow
			throw("[ERROR]Listen error");
		/*add this->listener to interest list of epoll*/
		this->addfd(this->listener[i], 0);
		i++;
	}
}

void Server::Close(int &sockfd)
{
	epoll_ctl(this->epfd, EPOLL_CTL_DEL, sockfd, NULL);
	close(sockfd);
	this->fd_responses_map.erase(sockfd);
}

Server::~Server()
{
	int i = this->port.size();
	while (--i >= 0)
		close(this->listener[i]);
	close(this->epfd);
	delete [] this->serverAddr;
	delete [] this->listener;
}

void Server::send_content_to_request(int &request_fd)
{
	std::map<int, cl_response>::iterator it;
	it = this->fd_responses_map.find(request_fd);
	if (it != this->fd_responses_map.end())
	{
		const char *new_str = (*it).second.content.c_str();
		if (send((*it).first, new_str, strlen(new_str), 0) < 0)
			std::cout << RED << ERR_SEND << NC << std::endl;
		ready_map.erase(request_fd);
		this->fd_responses_map.erase(request_fd);
		this->Close(request_fd);
	}
}

int Server::fd_is_in_listener(int fd)
{
	int i = this->port.size();
	while (--i >= 0)
	{
		if (fd == this->listener[i])
			return 1;
	}
	return 0;
}
/*loop for each event, manage the cas: new request(add request fd to epoll interest list);
  error or interrupt(close fd); read the request(read from buffer and store reponse in map);
  send reponse to request(send the reponse and close fd, erase fd from map)
 *:paramas (struct epoll_event *) events: event array that store all the info of each events
 *:params (int)epoll_event_count:total nb of events
 */
void Server::manage_event(struct epoll_event *events, int &epoll_event_count)
{
	for(int i = 0; i < epoll_event_count; i++)
	{
		int sockfd = events[i].data.fd;
		uint32_t ev = events[i].events;
		/* We receive a new request on the socket
		   We accept it and store new the FD of the request in the epoll interet list*/
		//if (sockfd == this->listener)
		if (fd_is_in_listener(sockfd))
		{
			try
			{
				this->acceptConnect(sockfd);
			}
			catch(const char *s)
			{
				std::cerr << s << std::endl;
			}
		}
		/* sth went wrong in the epoll monitoring list*/
		else if (ev & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
		{
			// ready_map.erase(sockfd);
			this->fd_responses_map.erase(sockfd);
			this->Close(sockfd);
		}
		// A request is now ready to receive a response
		else if (ev & EPOLLIN)
			this->ready_map[sockfd] = this->handle_client_event(sockfd);
		else if (ev & EPOLLOUT)// && this->ready_map[sockfd])/*send content to request*/
			this->send_content_to_request(sockfd);
	}
}

void Server::Start()
{
	struct epoll_event events[EPOLL_SIZE];
	try
	{
		this->Init();
	}
	catch(const char *s)
	{
		delete [] this->serverAddr;
		delete [] this->listener;
		std::cerr << s << std::endl;
		exit(EXIT_FAILURE);
	}
	std::cout << YELLOW << "Looking for request" << NC << std::endl;
	while (1)
	{
 		memset(events, 0, EPOLL_SIZE);
		int epoll_event_count = epoll_wait(this->epfd, events, EPOLL_SIZE, 10000000);
		/*err manage*/
		if (epoll_event_count < 0)
		{
			std::cout << RED << "epoll_wait error occurs" << NC << std::endl;
			continue ;
		}
		if (epoll_event_count == 0)
		{
			std::cout << GREEN << "NO REQUEST\n" << NC;
			continue;
		}
		/* `epoll_event_count` will most likely always be equal to 1, since `epoll_wait`
		   will return immediatly after receiving an event */
		this->manage_event(events, epoll_event_count);
	}
}

void Server::addfd(int fd, bool enable_et)
{
	struct epoll_event ev;
	memset(&ev, 0, sizeof(struct epoll_event));
	ev.data.fd = fd;
	ev.events = EPOLLIN;
	if (enable_et)
		//to  avoid  continuously  switching between EPOLLIN and EPOLLOUT calling
		ev.events = EPOLLIN | EPOLLOUT;
	if (epoll_ctl(this->epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
		throw("[ERROR]Failed to in epoll_ctl");
	fcntl(fd, F_SETFL, O_NONBLOCK);
	std::cout << "fd added to epoll" << std::endl;
}

/*create a new fd and add to the interest list
 */
void Server::acceptConnect(int &fd)
{
	struct sockaddr_in client_address;
	int addrlen = sizeof(struct sockaddr_in);
	int request_fd = accept(fd, (struct sockaddr *)&client_address, (socklen_t*)&addrlen);
	if (request_fd < 0)
		throw("[ERROR]accpet failure");
	this->addfd(request_fd, true);
}

route get_matching_route(Client_Request &obj, Conf &web_conf)
{
	std::map<std::string, route> loc_root = web_conf.m_location;
	std::string file = obj.get_client_ask_file();
	int loc_len = 0;
	std::string key;
	for (std::map<std::string, route>::iterator it=loc_root.begin(); it!=loc_root.end(); ++it)
    {
		if (it->first != "/" && check_substring(file, it->first))
        {
			if (it->first.length() > loc_len)
			{
				loc_len = it->first.length();
				key = it->first;
			}
        }
    }
	if(!key.size())
		key = "/";
	return loc_root[key];
}

void add_root_to_file(route &r, Client_Request &obj)
{
		std::string full_path("/");
		std::string file = obj.get_client_ask_file();

		// If `root` is defined in current `location` in conf file
		if (r.path_root.size() > 0)
		{
			std::vector<std::string> v = extract_words_in_vector(file);
			for (std::vector<std::string>::iterator it = v.begin() ; it != v.end(); ++it)
			{
				full_path += *it;
				if (it != v.end() - 1)
					full_path += "/";
			}
		}

		obj.clean_relative_path = full_path;
		full_path = r.path_root + file;
		if (full_path.length() - file.length() == 0)
			full_path = "." + full_path;
		obj.set_client_file(full_path);
}

std::string get_curr_server_name(Client_Request &obj)
{
	for (std::map<std::string, std::string>::iterator it=obj.client_request.begin();
		 it!=obj.client_request.end(); ++it)
		if (it->first == "Host")
			return it->second.substr(0, it->second.find(':'));
	return "";
}


int get_curr_port(Client_Request &obj)
{
	std::stringstream iss_port;
	int port;

	for (std::map<std::string, std::string>::iterator it=obj.client_request.begin();
		 it!=obj.client_request.end(); ++it)
		if (it->first == "Host")
		{
			iss_port << it->second.substr(it->second.find(':') + 1, it->second.length());
			iss_port >> port;
			return (port);
		}
	return (-1);
}

int port_is_matching_conf(int curr_port, Conf &curr_conf)
{
	return (curr_conf.port.count(curr_port));
}


/*
** Found the request's matching configuration based on the `Host` header.
** In case the `server_name` & `port` pair do not match any conf, we
** will use the default conf (first one parsed)
**
** :param (std::string) &curr_server_name: requested server name
** :param (int) curr_port: port used by request
** :param (std::vector<Conf>) &web_conf_vector: all parsed server configurations
** :param (Conf) &default_conf: server configuration to use if no matching Conf where found
** :return (Conf): matching Conf or default one
*/
Conf get_curr_conf(std::string &curr_server_name, int curr_port, std::vector<Conf> &web_conf_vector, Conf &default_conf)
{
	std::vector<Conf>::iterator it;
	for (it = web_conf_vector.begin() ;
		 it != web_conf_vector.end(); ++it)
	{
		if (curr_server_name == (*it).server_name)
		{
			if (port_is_matching_conf(curr_port, (*it)))
				return (*it);
			else
				return (default_conf);
		}
	}
	return default_conf;
}

void  Server::extract_info_from_buffer(Client_Request &obj, std::string buffer)
{
	extract_info_from_first_line(obj, buffer);
	extract_info_from_rest(obj, buffer);
}

void Server::extract_info_and_prepare_response(Conf &curr_conf, int &fd, Client_Request &obj)
{
	route r = get_matching_route(obj, curr_conf);
	add_root_to_file(r, obj);
	manage_request_status(r, obj, curr_conf, this->fd_responses_map[fd]);

	cl_response rep;
	rep.content = response_str(obj);
	this->fd_responses_map.erase(fd);
	this->fd_responses_map.insert(std::pair<int, cl_response> (fd, rep));
}

bool no_specific_file_asked(Client_Request &obj)
{
	return ((obj.clean_relative_path == "" || obj.clean_relative_path == "/")
			&& (obj.file == "" || obj.file == "/"));
}

void manage_default_file_if_needed(Client_Request &obj, Conf &curr_conf)
{
	route r = get_matching_route(obj, curr_conf);
	if (r.auto_index == false && obj.method == "GET" &&
		no_specific_file_asked(obj))
	{
		obj.file = curr_conf.default_file;
		obj.clean_relative_path = curr_conf.default_file;
	}
}

bool Server::prepare_error_response(int request_fd, int error_code, Conf curr_conf, Client_Request obj)
{
	set_error(obj, curr_conf, error_code);

	cl_response rep;
	rep.content = response_str(obj);
	this->fd_responses_map.erase(request_fd);
	this->fd_responses_map.insert(std::pair<int, cl_response> (request_fd, rep));
	return (this->ready_map[request_fd]);
}

bool Server::manage_http_redirection(route r, int request_fd, Conf curr_conf, Client_Request obj)
{
	std::string final_redir =  r.redirection + obj.origin_path;
	obj.custom_headers["Location"] = final_redir;
	return (this->prepare_error_response(request_fd, 301, curr_conf, obj));
}

bool is_body_too_large(int c_len, Conf &curr_conf, Client_Request &obj)
{
	return (curr_conf.get_client_max_body_size() != -1 &&
			c_len > curr_conf.get_client_max_body_size());
}

bool Server::is_chunked_request(int request_fd, Client_Request &obj)
{
	if (this->fd_responses_map.count(request_fd) == 0 &&
		obj.client_request.count("Content-Type"))
	{
		int i_equal = obj.client_request["Content-Type"].find("=");
		return (obj.client_request["Content-Type"].substr(0, i_equal) == "multipart/form-data; boundary");
	}
	return (this->fd_responses_map.count(request_fd));
}

bool Server::chunkManagement(int fd, Client_Request &obj, Conf &curr_conf)
{
	// FIRST CHUNK
	if (this->fd_responses_map.count(fd) == 0)
	{
		int i_equal = obj.client_request["Content-Type"].find("=");
		std::string c_type = obj.client_request["Content-Type"];
		this->fd_responses_map[fd].boundary = "--" + c_type.substr(i_equal + 1, c_type.length());
		this->fd_responses_map[fd].conf = curr_conf;
		this->fd_responses_map[fd].content_length = cast_as_int(obj.client_request["Content-Length"]);

		this->fd_responses_map[fd].obj = obj;
		std::string file = obj.get_client_ask_file();
		std::string extension = file.substr(file.find_last_of(".") + 1);
		this->fd_responses_map[fd].file_extension = extension;
		this->fd_responses_map[fd].route_path = file;


		std::vector<size_t> boundary_occurrences;
		boundary_occurrences = get_occurences_indexes(obj.payload,
													  this->fd_responses_map[fd].boundary);
		if (boundary_occurrences.size() > 0)
		{
			int i_file_start = obj.payload.find("filename=\"") + 10;
			int i_file_end = obj.payload.find_first_of("\r\n", 0) - 1;

			this->fd_responses_map[fd].filename = obj.payload.substr(i_file_start,
																	 i_file_end - i_file_start);

			int i_payload_start = obj.payload.find("\r\n\r\n") + 4;
			obj.payload.erase(0, i_payload_start);

			int i_payload_end = obj.payload.find("\r\n");
			obj.payload.erase(i_payload_end, obj.payload.length());
			this->fd_responses_map[fd].payloads = obj.payload;

			if (obj.payload.length() == 0)
				return (this->prepare_error_response(fd, 400, curr_conf, obj));
			return (true);
		}
		return (false);
	}
	// IS LAST CHUNK
	if (obj.payload.find(this->fd_responses_map[fd].boundary) != string::npos)
	{
		int i_end_payload = obj.payload.find(this->fd_responses_map[fd].boundary) - 2;
		this->fd_responses_map[fd].payloads += obj.payload.substr(0, i_end_payload);
		return (true);
	}
	this->fd_responses_map[fd].payloads += obj.payload;
	return (false);
}



/*read from the buffer and store the request fd and reponse in the map
 */
bool Server::handle_client_event(int &request_fd)
{
	Client_Request obj;
	int max_nb = 65536;
	char tmp_buffer[max_nb];
	memset(tmp_buffer, 0, max_nb);
	long nb_read = recv(request_fd, tmp_buffer, sizeof(tmp_buffer), 0);
	std::string buffer(tmp_buffer);
	Conf default_conf = this->web_conf_vector.at(0);
	Conf curr_conf = default_conf;

	std::cout << GREEN << buffer << NC << "\n";
	std::cout << YELLOW << "#########################################" << NC << "\n";

	// Initialize in case it isn't yet
	if (this->fd_responses_map.find(request_fd) == this->fd_responses_map.end())
		this->ready_map.insert(std::pair<int, bool> (request_fd, true));

	// Add what we just read from the buffer
	std::map<int, std::string> curr_request;
	curr_request[request_fd] = std::string(buffer, nb_read);

	if (nb_read <= 0)
		// -------- EMPTY REQUEST ---------
		return (this->prepare_error_response(request_fd, 204, default_conf, obj));
	else
	{
		// IS SCD CHUNKED REQUEST
		// We remove the boundary substring to still extract new headers `Content-Disposition` and get the `filename`
		if (this->fd_responses_map.count(request_fd) && this->fd_responses_map[request_fd].filename.size() == 0)
		{
			int boundary_len = this->fd_responses_map[request_fd].boundary.length();
			buffer.erase(0, boundary_len - 1);
			curr_conf = this->fd_responses_map[request_fd].conf;
			extract_info_from_rest(obj, buffer);

			std::string c_dispo = obj.client_request["Content-Disposition"];
			int i_filename = c_dispo.find("filename=\"") + 10;
			this->fd_responses_map[request_fd].filename = c_dispo.substr(i_filename, c_dispo.length() - i_filename - 1);
		}
		else if (this->fd_responses_map.count(request_fd))
			obj.payload = buffer;
		else
		{
			// -------- SERVER CONF MANGEMENT ---------
			this->extract_info_from_buffer(obj, buffer);
			std::string curr_server_name = get_curr_server_name(obj);
			int curr_port = get_curr_port(obj);
			curr_conf = get_curr_conf(curr_server_name, curr_port, this->web_conf_vector, default_conf);

			// -------- HTTP REDIRECTION ---------
			route r = get_matching_route(obj, curr_conf);
			if (r.redirection.length() > 0)
				return (this->manage_http_redirection(r, request_fd, curr_conf, obj));

			// -------- DEFAULT INDEX FILE ---------
			manage_default_file_if_needed(obj, curr_conf);
		}

		if (obj.client_request.size() > 0 || this->fd_responses_map.count(request_fd)) //end_of_header != std::string::npos)
		{
			if (obj.client_request.count("Content-Length") ||
				(this->fd_responses_map.count(request_fd) && this->fd_responses_map[request_fd].content_length > -1))
			{
				// -------- MAX CLIENT BODY SIZE ---------
				int c_len;
				if (obj.client_request.count("Content-Length"))
					c_len = cast_as_int(obj.client_request["Content-Length"]);
				else
					c_len = this->fd_responses_map[request_fd].content_length;
				if (is_body_too_large(c_len, curr_conf, obj))
					return (this->prepare_error_response(request_fd, 413, curr_conf, obj));
			}

			if (is_chunked_request(request_fd, obj))
				ready_map[request_fd] = this->chunkManagement(request_fd, obj, curr_conf);
			else
				ready_map[request_fd] = true;
			if (ready_map[request_fd])
				this->extract_info_and_prepare_response(curr_conf, request_fd, obj);
		}
		else
			return (true);
	}
	std::cout << "\t\t" << ready_map[request_fd] << std::endl;
	return (ready_map[request_fd]);
}