#include "pch.h"
#include "hook_util.h"

#ifdef _WIN64
PLH::ZydisDisassembler disassembler(PLH::Mode::x64);
#else
PLH::ZydisDisassembler disassembler(PLH::Mode::x86);
#endif
