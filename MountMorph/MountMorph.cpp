/**
	For WoW Wotlk 3.3.5 Build 12340.
	Transmog your mount into any other mount in the game.

	=====================

	HOW TO:
	Write the commands in the regular wow chat ingame.
	**Preferably /w to yourself due to command messages are not hidden.

	Example on commands:
		.m 14376 31007		Changes your old mount(14376) to Invincible(31007)
		.m 31007			When old mount is already set you dont have to do it again

	See: https://wow.gamepedia.com/MountID for all possible mount display IDs

	=====================

	@file MountMorph.cpp
	@author Anders <Branders> Blomqvist
	@version 1.0 2020-10-27
*/

#include <iostream>
#include <Windows.h>
#include <math.h>
#include "mem.h"

// Number of bytes we overwrite (no bytes can be left out and minimun of 5 bytes)
#define INJECTION_HOOK_LENGTH 6

// Hex values for mountIDs
DWORD newMountID;		// Morph into this
DWORD oldMountID;		// Change this model into newMountID

// Reference to address we jump back to after finishing our hooked function.
DWORD jumpBack;

// Opens a console window for std::cout prints.
void ToggleConsole()
{
	AllocConsole();
	FILE* f = new FILE;
	freopen_s(&f, "CONOUT$", "w", stdout);
}

/*
	Reads a string in memory and store it in the string param. The string
	is stored in 4-byte words one after another which means there's 4
	character in each word.

	@param startAddress Address in memory where the string start
	@param size How many 4-byte words we will read.
	@param string A char[16] array which the string will be stored in.
*/
void ReadString(DWORD startAddress, int size, char* string)
{
	int index = 0;
	for (int i = 0; i < size; i++)
	{
		// Read 4-byte word
		int word = (int)mem::FindDMAAddy(startAddress + 4 * i, { 0x0 });

		// Go through individual bytes
		for (int j = 0; j < 4; j++)
		{
			int byte = (word >> (8 * j)) & 0xFF;

			// If we've encountered end of string - stop.
			if (byte == 0x0)
				break;

			// Add the value (which is in ascii already) into string
			string[index] = byte;
			index++;
		}
	}
}

/*
	Hardcoded parser for char arrays to extract mount IDs
	".m 12345 12345" is a valid format
	".m 1234 1234" aswell
	".m 12345
	.m <int> <int> or .m <int>

	@param cmd Chat message sent from player
*/
void ParseData(char* cmd)
{
	if (cmd[0] == '.' && cmd[1] == 'm' && cmd[2] == ' ')
	{
		std::cout << "[Parser] Prefix OK\n";

		// Search for second blank and end string
		int endIndex = 0;
		int blankIndex = 0;
		for (int i = 3; i < 16; i++)
		{
			if (cmd[i] == ' ')
			{
				blankIndex = i;
				std::cout << "[Parser] Second blank OK.\n";
			}
			if (cmd[i] == '\0')
			{
				endIndex = i;

				// If a blank was not found only one integer have been
				// passed which means we only parse one, of course.
				if (blankIndex == 0)
					blankIndex = endIndex;

				std::cout << "[Parser] End string at " << endIndex << std::endl;
				break;
			}
		}

		// Read replace mount int
		int replace = 0;
		for (int i = 3; i < blankIndex; i++)
		{
			int size = blankIndex - i;
			int value = (cmd[i] - 48) * (int)pow(10, size - 1);
			std::cout << "[Parser] Read int value: " << value << std::endl;
			replace += value;
		}

		std::cout << "[Parser] Got replace value: " << replace << std::endl;

		if (endIndex <= 8)
		{
			std::cout << "[Parser] Only got one integer. Setting that to morph instead.\n";
			newMountID = replace;
			return;
		}
			

		// Read morph mount int
		int morph = 0;
		for (int i = blankIndex + 1; i < endIndex; i++)
		{
			int size = endIndex - i;
			int value = (cmd[i] - 48) * (int)pow(10, size - 1);
			std::cout << "[Parser] Read int value: " << value << std::endl;
			morph += value;
		}

		std::cout << "[Parser] Got morph value: " << morph << std::endl;

		oldMountID = replace;
		newMountID = morph;
	}
}

/*
	Compares two player name strings

	@param player Player name
	@param sender Chat message sender name
	@returns true if they are the same, false otherwise
*/
bool IsPlayer(char* player, char* sender)
{
	for (int i = 0; i < 8; i++)
		if (player[i] != sender[i])
			return false;

	return true;
}

/*
	The function we redirect to.
	We want to modify the value in ecx before it does the:
	mov [eax+edx*4], ecx instruction.
*/
void _declspec(naked) injection()
{
	_asm
	{
		// Check sytax very carefully!
		cmp ecx, 0				// If ecx is 0 this event is a "dismount event"
		je	mountdone			// Then we dont want to do anything
		cmp ecx, oldMountID		// Otherwise we check if ecx is loaded with our old mount
		je	changemount			// If yes then we want to change it.
		jmp mountdone			// Othwewise just skip
	changemount:
		sub ecx, ecx			// ecx is a hex value with displayID so set it to zero
		add ecx, newMountID		// Now load it with our new displayID.
	mountdone:
		mov [eax+edx*4], ecx	// Original asm
		pop ebp					// Original asm
		ret 8					// Original asm
		jmp [jumpBack]
	}
}

/*
	Overwrite the existing assembly instruction with
	a jmp to our own function.

	@param toHook Address where the assembly instruction is, inject here.
	@param function Reference to our function
	@param How many bytes we change at injection point
*/
bool Hook(void* toHook, void* function, int length) 
{
	if (length < 5)
		return false;

	DWORD protection;
	VirtualProtect(toHook, length, PAGE_EXECUTE_READWRITE, &protection);

	memset(toHook, 0x90, length);

	DWORD address = ((DWORD)function - (DWORD)toHook) - 5;

	*(BYTE*)toHook = 0xE9;
	*(DWORD*)((DWORD)toHook + 1) = address;

	DWORD temp;
	VirtualProtect(toHook, length, protection, &temp);

	return true;
}

DWORD WINAPI MainThread(LPVOID hModule)
{
	// ToggleConsole();

	// Get base address for .exe
	DWORD base = (DWORD)GetModuleHandle(L"Wow.exe");

	// Addresses for chat
	DWORD bufStart = 0x00B75A60;	// First message starts here
	DWORD bufCount = 0x00BCEFF4;	// Number of messages in buffer
	DWORD next = 0x17C0;			// + offset to next message in buffer

	// Offsets within a chat message
	DWORD msg = 0xBF4;
	DWORD sender = 0xC;

	// Player name in string
	DWORD localPlayer = 0x00C79D18;

	// Setup hook length and address
	DWORD hookAddress = base + 0x343BAC;
	jumpBack = hookAddress + INJECTION_HOOK_LENGTH;

	// Default values for mounts
	newMountID = 31007;		// Invicible
	oldMountID = 14376;		// Swift White Mech. Strider

	// Do the hook to our injection function
	std::cout << "Hooking ...\n";
	Hook((void*)hookAddress, injection, INJECTION_HOOK_LENGTH);
	std::cout << "Successfully hooked.\n";

	// Track number of messages in buffer. Used to check when a new message
	// has arrived.
	int count = (int)mem::FindDMAAddy(bufCount, { 0x0 });
	int oldCount = count;

	while (true)
	{
		count = (int)mem::FindDMAAddy(bufCount, { 0x0 });

		// Read next message, buffer will go back to 0 after 59
		if (count > oldCount || oldCount - count == 4)
		{
			// Before we do anything check who the message is sent from.
			// If the message is not from ourselves we want to ignore it.
			char senderName[17] = { 0 };
			char playerName[17] = { 0 };
			ReadString(bufStart + (next * (count - 1)) + sender, 2, senderName);
			ReadString(localPlayer, 2, playerName);
			if (IsPlayer(playerName, senderName))
			{
				// Read message string
				char cmd[17] = { 0 };
				ReadString(bufStart + (next * (count - 1)) + msg, 4, cmd);

				// Parse the char array
				ParseData(cmd);

				std::cout << std::dec << "Message [" << count << "]: " 
					<< cmd << "\t at " 
					<< std::hex << bufStart + (next * (count - 1)) << std::endl;
			}

			oldCount = count;
		}

		Sleep(100);
	}

	FreeLibraryAndExitThread((HMODULE)hModule, 0);
	return 0;
}

BOOL WINAPI DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved) 
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		CreateThread(0, 0, MainThread, hModule, 0, 0);
		// MessageBox(0, L"DLL injected", L"Status", MB_ICONINFORMATION);
		break;
	}
	return TRUE;
}