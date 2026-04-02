#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <windows.h>
#include <string>

enum {
	PARAM_ERROR = 1, /*API的參數錯誤*/

	/*GetKey的Error List*/
	GET_ENCKEY_FAILED, //2
	READ_KEYPRO_FAILED, //3
	DEC_KEY_FAILED, //4
	READ_REG_FAILED, //5
	KEY_SIZE_ERROR, //6
	AREA_NOT_MATCH, //7
	
	/*SettingFile的Error List*/
	USBDISK_NOT_FOUND, //8
	FILE_WRITE_FAILED, //9
	FILE_READ_FAILED, //10
	DATALEN_ERROR, //11
	GET_FILESIZE_FAILED, //12
	FILESIZE_ERROR, //13
	CRC_COMPARE_FAILED, //14
	OTHER_ERROR, //15
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

	/*	
		參數 :
		Data : 要寫入的資料
		DataLen : 要寫入的長度

		回傳值 :
		0 : 成功
		< 0 : 失敗
	*/
	KEYLIB_API int SettingFileWrite(BYTE* Data, DWORD DataLen);

	/*
		參數 :
		Data : 讀取資料的Buffer，要有足夠的空間來放DataLen的資料
		DataLen : Buffer的長度
		RtnDataLen : 實際讀取到的資料長度

		回傳值 :
		0 : 成功
		< 0 : 失敗
	*/
	KEYLIB_API int SettingFileRead(BYTE* Data, DWORD DataLen, DWORD* RtnDataLen);
	
	/*
		參數 :
		DataLen : 讀取資料的長度

		回傳值 :
		0 : 成功
		< 0 : 失敗
	*/
	KEYLIB_API int SettingFileGetLen(DWORD* DataLen);

#ifdef __cplusplus
}
#endif
