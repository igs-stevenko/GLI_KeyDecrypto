#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <windows.h>
#include <string>

enum {
	GET_ENCKEY_FAILED = 1,
	READ_KEYPRO_FAILED,
	DEC_KEY_FAILED,
	READ_REG_FAILED,
	AREA_NOT_MATCH,
};

#ifdef KEYLIB_EXPORTS
#define KEYLIB_API __declspec(dllexport)
#else
#define KEYLIB_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

	KEYLIB_API int GetKey(BYTE* Key, DWORD* KeyLen);

#ifdef __cplusplus
}
#endif
