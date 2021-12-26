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
#include <map>

int has_new_line(char *buffer)
{
	int i = 0;
	while (buffer[i])
	{
		if (buffer[i] == '\n')
			return i;
		i++;
	}
	return 0;
}

std::string get_key_from_line(char *str, int *i)
{
	int lens = 0;

	while (str[*i] && str[*i] != ':')
	{
		(*i)++;
		lens++;
	}
	char k[lens + 1];
	strncpy(k, str, lens);
	k[lens] = '\0';
	std::string key(k);
	return (key);
}

std::string get_value_from_line(char *str)
{
	int j = 0;
	while (str[j])
		j++;
	char v[j + 1];
	strncpy(v, str, j);
	v[j] = '\0';
	std::string value(v);
	return (value);
}

int get_r_n_index(char *buffer)
{
	int i = 0;
	while (buffer[i])
	{
		if (i > 0 && buffer[i] == '\r' && buffer[i + 1] && buffer[i + 1] == '\n')
			return i;
		i++;
	}
	return 0;
}

//std::map<std::string, std::string> extract_info_from_header(char *buffer)
void extract_info_from_header(char *buffer)
{
	int len = 0;
	int i;
	std::map<std::string,std::string> header;

	std::string line;
	buffer += get_r_n_index(buffer) + 1 + 1;//skip fst line which ask filename
	while ((len = get_r_n_index(buffer)) > 0)
	{
		i = 0;
		char str[len + 1];
		strncpy(str, buffer, len);//cpy rill \r\n
		str[len] = '\0';
		std::string key = get_key_from_line(str, &i);
		while (str[i] && (str[i] == ':' || str[i] == ' '))
			i++;
		std::string value = get_value_from_line(str + i);
		header[key] = value;
		buffer += len + 1 + 1;
	}
	for (std::map<std::string, std::string>::iterator it=header.begin(); it!=header.end(); ++it)
	 	std::cout << it->first << " => " << it->second << '\n';
	//return header;
}
