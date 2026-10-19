#include "Log.h"

#include "Engine.h"

void Log::WriteLine(std::string_view text) { Engine::LogAlways(std::format("[TreasureFinder] {}", text).c_str()); }
