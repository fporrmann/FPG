// Based on sigint.c from Christian Borgelt
#pragma once

#include <signal.h>

#ifdef _WIN32
#define NOMINMAX // Disable the build in MIN/MAX macros to prevent collisions
#include <windows.h>
#else
#define _POSIX_C_SOURCE 200809L
#endif





#ifdef WITH_SIG_TERM
static volatile sig_atomic_t aborted = 0;
#ifndef _WIN32
static struct sigaction sigOld;
static struct sigaction sigNew;
#endif

void sigAbort(const int& state)
{
	aborted = state;
}

#ifdef _WIN32

static BOOL WINAPI sigHandler(DWORD type)
{
	if (type == SIGINT)
		sigAbort(-1);
	return TRUE;
}

void sigInstall()
{
	SetConsoleCtrlHandler(sigHandler, TRUE);
}


void sigRemove()
{
	SetConsoleCtrlHandler(sigHandler, FALSE);
}

#else

static void sigHandler(int type)
{
	if (type == SIGINT)
		sigAbort(-1);
}

void sigInstall()
{
	sigNew.sa_handler = sigHandler;
	sigNew.sa_flags = 0;
	sigemptyset(&sigNew.sa_mask);
	sigaction(SIGINT, &sigNew, &sigOld);
}

void sigRemove()
{
	sigaction(SIGINT, &sigOld, reinterpret_cast<struct sigaction*>(0));
}
#endif

int sigAborted()
{
	return aborted;
}
#endif


