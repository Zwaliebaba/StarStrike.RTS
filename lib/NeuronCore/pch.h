// Precompiled header for NeuronCore consumers
// All .cpp files in projects linking NeuronCore will precompile these headers.
#pragma once

// ── Standard Library ────────────────────────────────────────────────────────
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// ── NeuronCore ──────────────────────────────────────────────────────────────
#include "NeuronCore/Types.h"
#include "NeuronCore/Constants.h"
#include "NeuronCore/Socket.h"
#include "NeuronCore/FileSystem.h"
#include "NeuronCore/Threading.h"
#include "NeuronCore/Timer.h"
