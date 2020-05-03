#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>
#include <bits/stdc++.h>
#include <boost/format.hpp>
#include <Windows.h>
#include <conio.h>
#include <comutil.h>
#pragma comment(lib, "comsuppw.lib")
#pragma comment(lib, "User32.lib")

using namespace std;

extern HANDLE handle;
extern HWND hwnd;

void start();

void show(string str, DWORD delay);
