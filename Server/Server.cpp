#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#define NOMINMAX
#define NODRAWTEXT
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>

#include <DirectXMath.h>
#include <DirectXCollision.h>
using namespace DirectX;

#include "Debug.h"
#include "NeuronHelper.h"
#include "GameMath.h"
#include "WorldTypes.h"
#include "NetProtocol.h"

using namespace Neuron;

#pragma comment(lib, "Ws2_32.lib")

#include "GameServer.h"

int main(int _argc, char* _argv[])
{
  uint16_t port = Neuron::Net::DEFAULT_PORT;

  if (_argc > 1)
    port = static_cast<uint16_t>(std::atoi(_argv[1]));

  std::cout << "=== StarStrike Server ===" << std::endl;

  Neuron::Server::GameServer::Startup(port);
  Neuron::Server::GameServer::Run();
  Neuron::Server::GameServer::Shutdown();

  return 0;
}
