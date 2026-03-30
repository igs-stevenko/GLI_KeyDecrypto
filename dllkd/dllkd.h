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
	KEY_SIZE_ERROR,
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

	/* 參數 :
		Key : 取得Key的buffer，要有32bytes的大小 
	   
	   回傳值 :
		0 : 成功
		< 0 : 失敗
	   
	 */
	KEYLIB_API int GetKey(BYTE* Key);

#ifdef __cplusplus
}
#endif
