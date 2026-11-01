#ifndef _LOG_ABOUT_H
#define _LOG_ABOUT_H
//#pragma once

#define LOG_INFO(msg)    "[*] " msg
#define LOG_SUCCESS(msg) "[+] " msg
#define LOG_ERROR(msg)   "[x] " msg

#define CHECK(func, msg) if(func) {SDL_Log(LOG_ERROR(msg));}

#endif // !_LOG_ABOUT_H