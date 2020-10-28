#pragma once
#include <Windows.h>
#include <vector>

namespace mem
{
	DWORD FindDMAAddy(DWORD ptr, std::vector<unsigned int> offsets);
}