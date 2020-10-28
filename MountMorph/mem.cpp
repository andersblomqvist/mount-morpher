#include "mem.h"

// Used to find address from source pointer
DWORD mem::FindDMAAddy(DWORD ptr, std::vector<unsigned int> offsets)
{
	DWORD addr = ptr;
	for (unsigned int i = 0; i < offsets.size(); i++)
	{
		addr = *(DWORD*)addr;
		addr += offsets[i];
	}
	return addr;
}