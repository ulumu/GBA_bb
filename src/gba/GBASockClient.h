#pragma once

#ifdef _SFML_SUPPORT_
#include <SFML/Network.hpp>
#endif //_SFML_SUPPORT_

#include "../common/Types.h"

#ifdef _SFML_SUPPORT_
class GBASockClient : public sf::SocketTCP
{
public:
	GBASockClient(sf::IPAddress server_addr);
	~GBASockClient();

	void Send(std::vector<char> data);
	char ReceiveCmd(char* data_in);

private:
	sf::IPAddress server_addr;
	sf::SocketTCP client;
};
#else
class GBASockClient : public sf::SocketTCP
{
public:
	GBASockClient(sf::IPAddress server_addr);
	~GBASockClient();

	void Send(std::vector<char> data);
	char ReceiveCmd(char* data_in);

private:
	sf::IPAddress server_addr;
	sf::SocketTCP client;
};
#endif // _SFML_SUPPORT_
