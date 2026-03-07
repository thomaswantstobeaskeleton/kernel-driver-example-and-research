
#pragma once
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <TlHelp32.h>
#include <windows.h>
#include <string>
#include <cstdio>
#include <random>
#include "console_color.h"
#include <D3D11.h>

ID3D11ShaderResourceView* sLogo = nullptr;
ID3D11ShaderResourceView* aaLogo = nullptr;

using namespace std;

#define WINVER 0x0501 // Windows XP and later
#define _WIN32_WINNT 0x0501

namespace patch
{
	template < typename T > std::string to_string(const T& n)
	{
		std::ostringstream stm;
		stm << n;
		return stm.str();
	}
}

class console_t
{
public:
	bool set_highest_priority() {
		BOOL result = SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
		if (result == 0) {
			return false;
		}

		return true;
	}

	void start()
	{
		HWND consoleWindow = GetConsoleWindow();
		int opacity = 225;
		SetLayeredWindowAttributes(consoleWindow, 0, opacity, LWA_ALPHA);
		SetConsoleSize(80, 20);
	}

	void play_wav(const char* filename)
	{
		PlaySoundA(filename, NULL, SND_FILENAME | SND_ASYNC);
	}

	void move_cursor(int x, int y)
	{
		SetCursorPos(x, y);
	}

	void get_cursor_pos()
	{
		POINT p;
		if (GetCursorPos(&p)) {
			std::cout << " X -> " << p.x << ", Y -> " << p.y << std::endl;
		}
		else {
			std::cerr << "Failed to get cursor position. Error: " << GetLastError() << std::endl;
		}
	}

	void SetWindowFullscreen(HWND hwnd)
	{
		RECT screenRect;
		GetWindowRect(GetDesktopWindow(), &screenRect);

		LONG style = GetWindowLong(hwnd, GWL_STYLE);
		LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

		SetWindowLong(hwnd, GWL_STYLE, style & ~(WS_BORDER | WS_CAPTION));
		SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_TOPMOST);

		SetWindowPos(hwnd, HWND_TOPMOST, screenRect.left, screenRect.top, screenRect.right - screenRect.left, screenRect.bottom - screenRect.top, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOSENDCHANGING);

		ShowWindow(hwnd, SW_MAXIMIZE);
		SetForegroundWindow(hwnd);
	}

	void SetConsoleSize(int width, int height)
	{
		HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
		SMALL_RECT rect = { 0, 0, static_cast<SHORT>(width - 1), static_cast<SHORT>(height - 1) };
		COORD size = { static_cast<SHORT>(width), static_cast<SHORT>(height) };
		SetConsoleWindowInfo(consoleHandle, TRUE, &rect);
		SetConsoleScreenBufferSize(consoleHandle, size);
	}

	void input(std::string text)
	{
		std::cout << dye::white("(");
		std::cout << dye::yellow("REX"); //Frozen Public
		std::cout << dye::white("(+) ");

		std::cout << text;
	}

	void write(std::string text)
	{
		std::cout << dye::white("(+)");
		std::cout << dye::yellow(""); //Frozen Public
		std::cout << dye::white("");

		std::cout << text << std::endl;
	}
	
	void success(std::string text)
	{
		std::cout << dye::white("[");
		std::cout << dye::green("+");
		std::cout << dye::white("] ");

		std::cout << text << std::endl;
	}

	void error(std::string error)
	{
		std::cout << dye::white("[");
		std::cout << dye::red("-");
		std::cout << dye::white("] ");

		std::cout << error << std::endl;
	}

	void sleep(DWORD MilliSeconds)
	{
		Sleep(MilliSeconds);
	}

	void exit(int exit_code)
	{
		ExitProcess(0);
	}

	void beep(DWORD dw_Freq, DWORD dw_Duration)
	{
		Beep(dw_Freq, dw_Duration);
	}

	void kill_cheat()
	{
		TerminateProcess(GetCurrentProcess(), 0);
	}
	std::string generate_string(int length)
	{
		const std::string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

		std::random_device rd;
		std::mt19937 gen(rd());  // Seed the generator
		std::uniform_int_distribution<> distrib(0, characters.size() - 1);  // Define the range

		std::string randomString;
		randomString.reserve(length);

		for (int i = 0; i < length; ++i) {
			randomString += characters[distrib(gen)];
		}

		Sleep(50);

		std::string name = "Fortnite - ";

		return name + randomString;
	}

	static bool admincheck()
	{
		HANDLE hToken;

		if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
		{
			TOKEN_ELEVATION elevation;
			DWORD size;

			if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size))
			{
				CloseHandle(hToken);

				return elevation.TokenIsElevated != 0;
			}

			CloseHandle(hToken);
		}

		return false;
	}
}; console_t console;

#ifndef ETHERA_PROTECT
#define ETHERA_PROTECT

#include <string>
#include <vector>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "ntdll.lib")

#define Hide_Function() try { int* __INVALID_ALLOC__ = new int[(std::size_t)std::numeric_limits<std::size_t>::max]; } catch ( const std::bad_alloc& except ) { 
#define Hide_Function_End() } 

std::string Ethera_License = "";
std::string Ethera_Webhook = "0";

bool Ethera_Logs = false;
bool Ethera_Bluescreen = false;
bool Ethera_Imgui_Support = false;
bool Ethera_Block_All_Console_Windows = false;

extern "C"
{
	class Protection
	{
	public:
		void Protect(std::string client_license, std::string webhook, bool discord_logs, bool bluescreen, bool imgui_support, bool block_all_console_windows);
		void Check();
		void Print(const std::string text);
		void Wait(int milliseconds);
		void Authswap();
		void Map_Driver(std::vector<uint8_t> data);
		std::string Getinput(); // example string License = Ethera_Getinput();
	}; Protection Ethera;
}
#endif
