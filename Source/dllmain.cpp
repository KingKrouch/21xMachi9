// dllmain.cpp: This is where the magic happens!

/**
 21xMachi9 (C) 2020 Bryce Q.

 21xMachi9 is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 21xMachi9 is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES

#include "../Source/pch.h"
#include "wtypes.h"
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <math.h>
#include <process.h>
#include <stdio.h>
#include <wininet.h>
#include "../Source/ThirdParty/IniReader/iniReader.h"
#include "../Source/ThirdParty/ModUtils/MemoryMgr.h"

using namespace std;

// Config.ini variables
bool useCustomFPSCap;
bool useCustomFOV;
int tMaxFPS;
bool ignoreUpdates;
int customFOV;

// Resolution and Aspect Ratio variables
int hRes;
int vRes;
float originalAspectRatio = 1.777777777777778;
float aspectRatio;

// FOV variables
float originalFOV = 0.008726646192; // Declares the original 16:9 vertical FOV.
float FOV;

// Misc variables
bool check = true; // do not change to false or else resolution checks won't run.

// Process HMODULE variable
HMODULE baseModule = GetModuleHandle(NULL);

void readConfig()
{
    cout.flush();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout); // Allows us to add outputs to the ASI Loader Console Window.
    cout.clear();
    cin.clear();
    CIniReader config("config.ini");
    // FOV Config Values
    useCustomFOV = config.ReadBoolean("FieldOfView", "useCustomFOV", false);
    customFOV = config.ReadInteger("FieldOfView", "FOV", 90);
    // Framerate/VSync Config Values
    useCustomFPSCap = config.ReadBoolean("Framerate", "useCustomFPSCap", false);
    tMaxFPS = config.ReadInteger("Framerate", "t.MaxFPS", 200);
}

void fovCalc()
{
    // Declare the vertical and horizontal resolution variables.
    int hRes = *(int*)((intptr_t)baseModule + 0x41995B0); // Grabs Horizontal Resolution integer.
    int vRes = *(int*)((intptr_t)baseModule + 0x41995B4); // Grabs Vertical Resolution integer.

    // Convert the int values to floats, so then we can determine the aspect ratio.
    float aspectRatio = (float)hRes / (float)vRes;

    if (useCustomFOV)
    {
		// Subtracts the custom FOV by the default FOV to get the difference
		float FOVDifference = (float)customFOV - 90.0f;
        // If useCustomFOV is set to "1", then calculate the vertical FOV using the new aspect ratio, the old aspect ratio, and the desired custom FOV (based on the FOVDifference to offset any oddities).
        FOV = round((2.0f * atan(((aspectRatio) / (16.0f / 9.0f)) * tan(((originalFOV * 10000.0f) + FOVDifference) / 2.0f * ((float)M_PI / 180.0f)))) * (180.0f / (float)M_PI) * 100.0f) / 100.0f / 10000.0f;
    }
    else
    {
        // If useCustomFOV is set to "0", then calculate the vertical FOV using the new aspect ratio, the old aspect ratio, and the original FOV.
        FOV = round((2.0f * atan(((aspectRatio) / (16.0f / 9.0f)) * tan((originalFOV * 10000.0f) / 2.0f * ((float)M_PI / 180.0f)))) * (180.0f / (float)M_PI) * 100.0f) / 100.0f / 10000.0f;
    }

    // Writes FOV to Memory.
    *(float*)((intptr_t)baseModule + 0x2CF6DA0) = FOV;
}

void pillarboxRemoval()
{
	// Writes pillarbox removal into memory ("33 83 4C 02" to "33 83 4C 00").
	memcpy((LPVOID)((intptr_t)baseModule + 0x1E39420), "\x33\x83\x4c\x00", 4);
}

void uncapFPS() //Uncaps the framerate.
{
	//Writes the new t.MaxFPS cap to memory, alongside pointer.
	*(float*)(*((intptr_t*)((intptr_t)baseModule + 0x045C1298)) + 0x0) = (float)tMaxFPS;
}

void resolutionCheck()
{
    if (aspectRatio != (*(int*)((intptr_t)baseModule + 0x41995B0) / *(int*)((intptr_t)baseModule + 0x41995B4)))
    {
        fovCalc();
    }
}

void framerateCheck()
{
	if (tMaxFPS != *(float*)(*((intptr_t*)((intptr_t)baseModule + 0x045C1298)) + 0x0))
	{
		uncapFPS();
	}
}

void StartPatch()
{
	// Unprotects the main module handle.
    ScopedUnprotect::FullModule UnProtect(baseModule);

    Sleep(5000); // Sleeps the thread for five seconds before applying the memory values.

	fovCalc(); // Calculates the new vertical FOV.

	pillarboxRemoval(); // Removes the in-game pillarboxing.

    // checks if CustomFPS cap is enabled before choosing which processes to loop, and if to uncap the framerate. This is done to save on CPU resources.
    if (useCustomFPSCap)
    {
        uncapFPS(); //Uncaps the framerate.
        // Runs resolution and framerate check in a loop.
		while (check != false)
		{
			resolutionCheck();
			framerateCheck();
		}
    }
    else
    {
        // Runs resolution check in a loop.
		while (check != false)
		{
			resolutionCheck();
		}
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) // This code runs when the DLL file has been attached to the game process.
    {
        // Reads the "config.ini" config file for values that we are going to want to modify.
        readConfig();

		HANDLE patchThread; // Creates a handle to the patch thread, so it can be closed easier
		patchThread = (HANDLE)_beginthreadex(NULL, 0, (unsigned(__stdcall*)(void*))StartPatch, NULL, 0, 0); // Calls the StartPatch function in a new thread on start.
		// CloseHandle(patchThread); // Closes the StartPatch thread handle.
    }
    return TRUE;
}
