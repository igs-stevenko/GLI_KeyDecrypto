// dllmain.cpp : 定義 DLL 應用程式的進入點。
#include "pch.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <windows.h>
#include <string>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdint.h>
#include <tbs.h>
#include <ncrypt.h>
#include <commctrl.h>
#include <thread>
#include <openssl/sha.h>
#include <setupapi.h>
#include <devguid.h>
#include <winioctl.h>
#include <aclapi.h>
#include "dllkd.h"
#include "hasp_api.h"

#pragma warning(disable:4996)
#pragma comment(lib, "setupapi.lib")

#define CHUNK 4096
#define KEYPATH R"(C:\Program Files (x86)\IGS\z.bin)"
#define X_IMG_NAME "x.img"
#define SETTING_FILE "Setting.bin"

enum {
	USB_STORAGE_DETECTED = 0,
	USB_STORAGE_NOT_DETECTED,
};

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

void PrintHex(unsigned char* buf, int len) {

	int i = 0;
	for (i = 0; i < len; i++) {
		if (i % 16 == 0)	printf("\n");
		printf("0x%02x, ", buf[i]);
	}
}

uint32_t Crc32(const uint8_t* data, size_t length) {
	uint32_t crc = 0xFFFFFFFF;

	for (size_t i = 0; i < length; i++) {
		crc ^= data[i];

		for (int j = 0; j < 8; j++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xEDB88320;
			else
				crc >>= 1;
		}
	}

	return ~crc;
}

bool FileExists(const char* FilePath)
{

	if (!FilePath) {
		return false;
	}

	DWORD attr = GetFileAttributesA(FilePath);

	if (attr == INVALID_FILE_ATTRIBUTES)
	{
		//printf("Error Code: %lu\n", GetLastError());
		return false;
	}

	return !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool IsUpdateStorage(BYTE* Path){

	BYTE FilePath[512];

	sprintf((char *)FilePath, "%s:\\%s", Path, X_IMG_NAME);

	return FileExists((const char *)FilePath);
}

bool GetFirstUSBMountedPath(BYTE* Path)
{
	if (!Path)
		return false;

	HDEVINFO hDevInfo = SetupDiGetClassDevs(
		&GUID_DEVINTERFACE_DISK,
		NULL,
		NULL,
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (hDevInfo == INVALID_HANDLE_VALUE)
		return false;

	SP_DEVICE_INTERFACE_DATA ifData;
	ifData.cbSize = sizeof(ifData);

	for (DWORD i = 0;
		SetupDiEnumDeviceInterfaces(
			hDevInfo, NULL,
			&GUID_DEVINTERFACE_DISK,
			i, &ifData);
		i++)
	{
		DWORD requiredSize = 0;
		SetupDiGetDeviceInterfaceDetail(
			hDevInfo, &ifData,
			NULL, 0,
			&requiredSize, NULL);

		PSP_DEVICE_INTERFACE_DETAIL_DATA detail =
			(PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);

		if (!detail) continue;

		detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		if (SetupDiGetDeviceInterfaceDetail(
			hDevInfo, &ifData,
			detail, requiredSize,
			NULL, NULL))
		{
			HANDLE hDisk = CreateFile(
				detail->DevicePath,
				0,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				0,
				NULL);

			if (hDisk != INVALID_HANDLE_VALUE)
			{
				STORAGE_PROPERTY_QUERY query = {};
				query.PropertyId = StorageDeviceProperty;
				query.QueryType = PropertyStandardQuery;

				BYTE buffer[1024];
				DWORD bytes;

				if (DeviceIoControl(
					hDisk,
					IOCTL_STORAGE_QUERY_PROPERTY,
					&query,
					sizeof(query),
					buffer,
					sizeof(buffer),
					&bytes,
					NULL))
				{
					STORAGE_DEVICE_DESCRIPTOR* desc =
						(STORAGE_DEVICE_DESCRIPTOR*)buffer;

					if (desc->Size > sizeof(buffer))
					{
						CloseHandle(hDisk);
						free(detail);
						continue;
					}

					if (desc->BusType == BusTypeUsb)
					{
						STORAGE_DEVICE_NUMBER number;
						if (DeviceIoControl(
							hDisk,
							IOCTL_STORAGE_GET_DEVICE_NUMBER,
							NULL, 0,
							&number,
							sizeof(number),
							&bytes,
							NULL))
						{

							CloseHandle(hDisk);
							free(detail);
							detail = NULL;

							// 現在比對磁碟機字母
							char drives[512];
							DWORD len = GetLogicalDriveStringsA(
								sizeof(drives), drives);


							// ⭐ FIX: 檢查長度
							if (len == 0 || len > sizeof(drives))
							{
								SetupDiDestroyDeviceInfoList(hDevInfo);
								return false;
							}

							char* drive = drives;
							while (*drive)
							{
								char volumePath[64];
								sprintf_s(volumePath, sizeof(volumePath),
									"\\\\.\\%c:",
									drive[0]);

								HANDLE hVol = CreateFileA(
									volumePath,
									0,
									FILE_SHARE_READ |
									FILE_SHARE_WRITE,
									NULL,
									OPEN_EXISTING,
									0,
									NULL);

								if (hVol != INVALID_HANDLE_VALUE)
								{

									STORAGE_DEVICE_NUMBER volNumber;
									if (DeviceIoControl(
										hVol,
										IOCTL_STORAGE_GET_DEVICE_NUMBER,
										NULL, 0,
										&volNumber,
										sizeof(volNumber),
										&bytes,
										NULL))
									{

										if (volNumber.DeviceNumber ==
											number.DeviceNumber)
										{

											*Path = drive[0];
											if (IsUpdateStorage(Path) == false) {
												CloseHandle(hVol);
												SetupDiDestroyDeviceInfoList(
													hDevInfo);
												return true;
											}
										}
									}
									CloseHandle(hVol);
								}
								size_t l = strlen(drive);
								if (l == 0 || l > 10) break;

								drive += l + 1;
							}
						}
					}
				}
				CloseHandle(hDisk);
			}
		}
		if(detail)	free(detail);
	}
	SetupDiDestroyDeviceInfoList(hDevInfo);
	return false;
}


/* 執行內容 :
	偵測是否有USB Storage掛載起來，沒有則回傳USB_STORAGE_NOT_DETECTED

   參數 :
	參數1.若有USB Storage被掛載起來，則此參數會被放入掛仔的路徑

   回傳值 :
	  USB_STORAGE_DETECTED : 有USB Storage被掛載起來
	  USB_STORAGE_NOT_DETECTED : 沒有任何USB Storage被掛載起來

 */
int DetectUSBStorage(BYTE* MountPath)
{
	if (!MountPath) {
		return -1;
	}

	BYTE pathBuf[128] = { 0 };

	if (GetFirstUSBMountedPath(pathBuf))
	{
		sprintf((char*)MountPath, "%s", pathBuf);
		return USB_STORAGE_DETECTED;
	}
	else
	{
		return USB_STORAGE_NOT_DETECTED;
	}
}

int Aes256Decrypt(BYTE* Key, BYTE* IV, BYTE* Input, DWORD InputLen, BYTE* Output, DWORD* OutputLen)
{

	if (Key == NULL || IV == NULL || Input == NULL || Output == NULL || OutputLen == NULL)
		return -1;

	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
	if (!ctx) return -2;

	if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, Key, IV))
		return -3;


	DWORD Remain = InputLen;
	DWORD Offset = 0;
	DWORD LEN = 0;
	DWORD Total = 0;

	int DecError = 0;

	while (Offset < InputLen) {
		size_t DecLen = (InputLen - Offset > CHUNK)
			? CHUNK
			: InputLen - Offset;

		if (!EVP_DecryptUpdate(
			ctx,
			Output + Total,
			(int*)&LEN,
			Input + Offset,
			(int)DecLen
		)) {
			DecError = -1;
			break;
		}

		Total += LEN;
		Offset += DecLen;
	}

	if (DecError != 0) {
		return -4;
	}

	if (!EVP_DecryptFinal_ex(ctx, Output + Total, (int*)&LEN)) {
		return -5;
	}

	Total += LEN;
	*OutputLen = Total;

	return 0;
}

int GetSettingFileSize(const char* filename, DWORD* filesize) {
	
	if (filename == NULL || filesize == NULL) {
		return -1;
	}
	FILE* fin = fopen(filename, "rb");
	if (!fin) {
		return -2;
	}
	fseek(fin, 0, SEEK_END);
	*filesize = ftell(fin);
	fclose(fin);
	return 0;
}

int ReadFromFile(const char* filename, unsigned char* buf, int len) {

	int rtn = 0;
	FILE* fin = fopen(filename, "rb");
	if (!fin) {
		return -1;
	}

	rtn = fread(buf, 1, len, fin);
	if (rtn != len) {
		rtn = -2;
	}
	else {
		rtn = 0;
	}

	fclose(fin);

	return rtn;
}

int WriteToFile(const char* filename, unsigned char* buf, int len) {
	
	int rtn = 0;
	// 若檔案已存在，先確保移除只讀屬性並刪除，確保後續能覆蓋
	if (FileExists(filename)) {
		SetFileAttributesA(filename, FILE_ATTRIBUTE_NORMAL);
		DeleteFileA(filename);
	}

	FILE* fout = fopen(filename, "wb");
	if (!fout) return -1;

	size_t written = fwrite(buf, 1, len, fout);
	if (written != (size_t)len) {
		rtn = -2;
	}
	else {
		rtn = 0;
	}

	fclose(fout);

	// 授予 Everyone 讀取權限
	EXPLICIT_ACCESSA ea;
	PACL pNewAcl = NULL;
	ZeroMemory(&ea, sizeof(EXPLICIT_ACCESSA));

	ea.grfAccessPermissions = GENERIC_READ;
	ea.grfAccessMode = GRANT_ACCESS;
	ea.grfInheritance = NO_INHERITANCE;
	ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
	ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	ea.Trustee.ptstrName = (LPSTR)"EVERYONE";

	if (SetEntriesInAclA(1, &ea, NULL, &pNewAcl) == ERROR_SUCCESS) {
		SetNamedSecurityInfoA((LPSTR)filename, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, pNewAcl, NULL);
		if (pNewAcl) LocalFree(pNewAcl);
	}
	return rtn;
}

int ReadFromKeypro(BYTE *Data, DWORD DataLen, DWORD Offset) {

	int rtn = 0;
	hasp_status_t status = HASP_STATUS_OK;

	hasp_feature_t feature_id = 0;
	unsigned char vendor_code[] =
		"eApZw4pLQubJE71z6zkRH18zzQEebtatmnG4GEgoATapsLUtlFGGeq61r1dQt9BW3jVKSTkr7FhwoiqK"
		"1D3MvmNxzHLX79ycT/bBk1wd+15On+nXoI9mBvRRo6igEsR7lBzsKywK1O2devo/qnzXr5YH5vDXhoJa"
		"WJErCH/gmzejKHuaARWAo7dPrl6z4hhBFbmhWdDFyk7sxO9I+XVStaVjDNRXypBkczGfjsnYOPFjIZx4"
		"F/mIn09BBQ8bCjnnj7C1SYvGXGgM8zzxjo40Aj2wB/JFRhAf4xnCiCLAPtBmtBpN4BEMWBuKtqVHYAvk"
		"1PpWpOWJ30WOK5hwFXayzUMuaETtfZgwHKAMgzADxKSfg1QDlgFhehp098GuJu94IR1D5YHIIChdCWvW"
		"Fn0cezcmErPRqlXJ6Pn22VE7E+LRd58zpm2mhnql4jF3ncFAaFTzwpzMTIqI3VPOBRCHFeQ0GoXQs65J"
		"FJW3EShyrN4oTq6deZ8Jn11cGwJ6DPepQrTRllXfg+1qzN2+S8KuZ6B2Do+gzgnLnzj3uDiYK7p2eCMT"
		"oWTjO2hZypTFa46lzl3Drs7bA5wP6H9j1tuHzxpFi07q4bnGCF3JXOGFRp4TeTk5YNh/4A4DiDXesiWh"
		"wNWuI7dGEYpoKpfuMPLuKYQeZ5h1u3/48rGFwOK0zazk/zeRZL3NbCpoCMFuRh8Pg6GTArv52tMY9yqz"
		"640avx7sr64BEd3RU8Zc+P/wYT2YvHIVmrXPnUGzTiiNgZf6sUcORkzqj//Kl+bEQWqPMcpZn8VIeOHt"
		"+vxbo4jstn/rBmgyWu51gsw21DthaTb3SYCs/fmope07EDK0pOSh/MUFqdGQN4dtbwPFFZQ991UlNYf4"
		"eGuQiED7V7+IPpcItvPj6+nzM809J2sShrVN1cStbltYRPveVRL1UhZtW8dLPNvVllIV1B9UaFzMv8Nf"
		"2vmr6LFnIveLiYwN71s4Lg==";

	hasp_handle_t handle;

	status = hasp_login(feature_id, vendor_code, &handle);
	if (status != HASP_STATUS_OK) {
		return -1;
	}

	status = hasp_read(handle, HASP_FILEID_RO, Offset, DataLen, Data);
	if (status != HASP_STATUS_OK) {
		return -2;
	}

	hasp_logout(handle);

	return rtn;
}


int ReadAreaFromReg(BYTE *Area, DWORD *AreaLen) {

	int rtn = 0;

	const std::wstring& subKey = L"SOFTWARE\\IGS\\ProductionTool";
	const std::wstring& valueName = L"Region";

	HKEY hKey = nullptr;

	// 開啟 Key（強制 64-bit registry）
	LONG status = RegOpenKeyExW(
		HKEY_LOCAL_MACHINE,
		subKey.c_str(),
		0,
		KEY_READ | KEY_WOW64_64KEY,
		&hKey
	);

	if (status != ERROR_SUCCESS)
		return -1;

	DWORD type = 0;
	DWORD size = 0;

	// 🔹 第一次：取得長度（bytes）
	status = RegQueryValueExW(
		hKey,
		valueName.c_str(),
		nullptr,
		&type,
		nullptr,
		&size
	);

	if (status != ERROR_SUCCESS || type != REG_SZ || size == 0)
	{
		RegCloseKey(hKey);
		return -2;
	}

	// size 是「bytes」，包含 NULL terminator
	// 轉成 wchar_t 數量
	DWORD wcharCount = size / sizeof(wchar_t);

	std::vector<wchar_t> buffer(wcharCount);

	// 🔹 第二次：真正讀資料
	status = RegQueryValueExW(
		hKey,
		valueName.c_str(),
		nullptr,
		nullptr,
		reinterpret_cast<LPBYTE>(buffer.data()),
		&size
	);

	RegCloseKey(hKey);

	if (status != ERROR_SUCCESS)
		return -3;

	std::wstring wstr(buffer.data());
	std::string narrow(wstr.begin(), wstr.end());

	memcpy(Area, narrow.data(), strlen(narrow.data()));
	*AreaLen = strlen(narrow.data());

	return 0;
}

int ReadKeyEnc(BYTE *KeyEncrypt, DWORD KeyEncryptLen) {

	int rtn = 0;

	rtn = ReadFromFile(KEYPATH, KeyEncrypt, KeyEncryptLen);
	if (rtn != 0) {
		return -1;
	}

	return rtn;
}

int GetKey(BYTE *Key) {

	if (Key == NULL) {
		return -PARAM_ERROR;
	}
	
	int rtn = 0;
	int i = 0;

	BYTE KeyPlain[48] = { 0x00 };
	BYTE KeyEncrypt[48] = { 0x00 };

	DWORD KeyPlainLen;

	rtn = ReadKeyEnc(KeyEncrypt, sizeof(KeyEncrypt));
	if (rtn != 0) {
		return -GET_ENCKEY_FAILED;
	}

	BYTE RegArea[32] = { 0x00 };
	DWORD RegAreaLen = 0;
	rtn = ReadAreaFromReg(RegArea, &RegAreaLen);
	if (rtn != 0) {
		return -READ_REG_FAILED;
	}


	BYTE Area[32] = { 0x00 };
	//與Keypro通訊加入Retry 3次機制
	for (i = 0; i < 3; i++) {
		rtn = ReadFromKeypro(Area, sizeof(Area), 0);
		if (rtn != 0) {
			Sleep(3000);
			continue;
		}
		break;
	}

	if(i == 3) {
		return -READ_KEYPRO_FAILED;
	}

	if (memcmp(Area, RegArea, RegAreaLen) != 0) {
		return -AREA_NOT_MATCH;
	}

	BYTE GameData[32] = { 0x00 };
	//與Keypro通訊加入Retry 3次機制
	for (i = 0; i < 3; i++) {
		rtn = ReadFromKeypro(GameData, sizeof(GameData), 32);
		if (rtn != 0) {
			Sleep(3000);
			continue;
		}
		break;
	}
	if (i == 3) {
		return -READ_KEYPRO_FAILED;
	}


	BYTE Mix[64] = { 0x00 };
	memcpy(Mix, Area, sizeof(Area));
	memcpy(Mix+ sizeof(Area), GameData, sizeof(GameData));

	BYTE MixSha256[32] = { 0x0 };
	SHA256(Mix, sizeof(Mix), MixSha256);

	BYTE IV[16] = { 0x4a,0x78,0x26,0x82,0x23,0xd2,0xe1,0xcb,0x70,0x3a,0x4a,0x8b,0x23,0xa0,0xaf,0x3e };

	rtn = Aes256Decrypt(MixSha256, IV, KeyEncrypt, sizeof(KeyEncrypt), KeyPlain, &KeyPlainLen);
	if(rtn != 0) {
		return -DEC_KEY_FAILED;
	}

	if (KeyPlainLen != 32) {
		return -KEY_SIZE_ERROR;
	}

	memcpy(Key, KeyPlain, KeyPlainLen);

	return rtn;
}

int SettingFileWrite(BYTE *Data, DWORD DataLen) {

	if(Data == NULL || DataLen <= 0)
		return -PARAM_ERROR;

	int rtn = 0;
	BYTE MountPath[512] = { 0x00 };
	uint32_t DataCrc32 = { 0x00 };
	BYTE* DataWrite = NULL;
	BYTE FilePath[512] = { 0x00 };
	
	rtn = DetectUSBStorage(MountPath);
	if (rtn != 0) {
		rtn = -USBDISK_NOT_FOUND;
		goto out;
	}

	/* 計算Data的crc32 */
	DataCrc32 = Crc32(Data, DataLen);
	//printf("DataCrc32 = 0x%08x\n", DataCrc32);

	DataWrite = (BYTE*)malloc(DataLen + sizeof(DataCrc32));
	if (!DataWrite) {
		rtn = -OTHER_ERROR;
		goto out;
	}


	/* 將Crc32放到Data的最尾端 */
	memcpy(DataWrite, Data, DataLen);
	memcpy(DataWrite + DataLen, &DataCrc32, sizeof(DataCrc32));


	/* 將資料寫入目標檔案內 */
	sprintf((char*)FilePath, "%s:\\%s", MountPath, SETTING_FILE);

	rtn = WriteToFile((const char*)FilePath, DataWrite, DataLen + sizeof(DataCrc32));
	if (rtn != 0) {
		rtn = -FILE_WRITE_FAILED;
		goto out;
	}

out:
	
	if(DataWrite)	free(DataWrite);
	
	return rtn;
}

int SettingFileRead(BYTE* Data, DWORD DataLen, DWORD *RtnDataLen) {

	if (Data == NULL || DataLen <= 0 || RtnDataLen == NULL)
		return -PARAM_ERROR;

	int rtn = 0;
	BYTE MountPath[512] = { 0x00 };
	BYTE FilePath[512] = { 0x00 };
	DWORD FileSize = 0;
	BYTE* DataRead = NULL;
	uint32_t DataReadCrc32;
	uint32_t DataCrc32;

	rtn = DetectUSBStorage(MountPath);
	if (rtn != 0) {
		rtn = -USBDISK_NOT_FOUND;
		goto out;
	}

	/* 取得檔案長度 */
	sprintf((char*)FilePath, "%s:\\%s", MountPath, SETTING_FILE);

	rtn = GetSettingFileSize((const char *)FilePath, &FileSize);
	if (rtn != 0) {
		rtn = -GET_FILESIZE_FAILED;
		goto out;
	}

	if (FileSize <= 4) {
		rtn = -FILESIZE_ERROR;
		goto out;
	}

	/* 讀取出檔案內容，這裡的FileSize是檔案總長度，包含crc32的4bytes */
	DataRead = (BYTE*)malloc(FileSize);
	if (!DataRead) {
		rtn = -OTHER_ERROR;
		goto out;
	}

	rtn = ReadFromFile((const char *)FilePath, DataRead, FileSize);
	if(rtn != 0) {
		rtn = -FILE_READ_FAILED;
		goto out;
	}

	if ((FileSize - sizeof(uint32_t)) > DataLen) {
		rtn = -DATALEN_ERROR;
		goto out;
	}

	/* 取得最後4bytes的crc32 */
	memcpy(&DataReadCrc32, DataRead + FileSize - sizeof(uint32_t), sizeof(uint32_t));
	
	/* 計算扣除最後4bytes之後的crc32*/
	DataCrc32 = Crc32(DataRead, FileSize - sizeof(uint32_t));

	/* 比對crc32是否相同 */
	if (DataCrc32 != DataReadCrc32) {
		//printf("DataCrc32 = 0x%08x\n", DataCrc32);
		//printf("DataReadCrc32 = 0x%08x\n", DataReadCrc32);
		rtn = -CRC_COMPARE_FAILED;
		goto out;
	}

	/* 將DataRead的內容放到Data裡面 */
	memcpy(Data, DataRead, FileSize - sizeof(uint32_t));
	*RtnDataLen = FileSize - sizeof(uint32_t);

out:

	if (DataRead) free(DataRead);
	
	return rtn;
}

int SettingFileGetLen(DWORD *DataLen) {
	
	if (DataLen == NULL)
		return -PARAM_ERROR;

	int rtn = 0;
	BYTE MountPath[512] = { 0x00 };
	BYTE FilePath[512] = { 0x00 };
	DWORD FileSize = 0;

	rtn = DetectUSBStorage(MountPath);
	if (rtn != 0) {
		rtn = -USBDISK_NOT_FOUND;
		goto out;
	}

	/* 取得檔案長度 */
	sprintf((char*)FilePath, "%s:\\%s", MountPath, SETTING_FILE);
	rtn = GetSettingFileSize((const char *)FilePath, &FileSize);
	if (rtn != 0) {
		rtn = -GET_FILESIZE_FAILED;
		goto out;
	}

	if (FileSize <= 4) {
		rtn = -FILESIZE_ERROR;
		goto out;
	}

	*DataLen = FileSize - sizeof(uint32_t);

out :

	return rtn;
}