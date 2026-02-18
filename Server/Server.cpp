#include "pch.h"
#include <iostream>
#include "GameServer.h"

int main(int _argc, char *_argv[])
{
  uint16_t port = Net::DEFAULT_PORT;

  if (_argc > 1) port = static_cast<uint16_t>(std::atoi(_argv[1]));

  std::cout << "=== StarStrike Server ===" << std::endl;

  Server::GameServer::Startup(port);
  Server::GameServer::Run();
  Server::GameServer::Shutdown();

  return 0;
}