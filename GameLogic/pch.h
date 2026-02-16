#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#define NOMINMAX
#define NODRAWTEXT
#define NOBITMAP
#define NOMCX
#define NOSERVICE
#define NOHELP
#include <Windows.h>

#include <DirectXMath.h>
#include <DirectXCollision.h>
using namespace DirectX;

#include "Debug.h"
#include "NeuronHelper.h"
#include "GameMath.h"
#include "WorldTypes.h"

using namespace Neuron;
