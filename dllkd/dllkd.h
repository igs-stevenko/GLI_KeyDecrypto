#pragma once

/*
 * dllkd — Windows native plugin API
 * ---------------------------------------------------------------------------
 * Unity
 *   - Add the DLL that matches your player CPU (e.g. x64 `dllkd.dll` for a
 *     64-bit Windows player) under the appropriate `Assets/Plugins/...` folder.
 *   - Use this file as the reference for C# [DllImport] declarations.
 *
 * Calling convention
 *   - These entry points use the MSVC C default: __cdecl.
 *   - In C# use: CallingConvention = CallingConvention.Cdecl
 *
 * Return value
 *   - 0 = success.
 *   - On failure: negative of a KeyLibErrorCode enumerator (e.g. -PARAM_ERROR).
 *
 * Types for C# interop
 *   - BYTE*  -> byte[] or IntPtr
 *   - DWORD -> uint
 *   - DWORD* -> out uint (or ref uint)
 *
 * C# example
 *
 *   const string Dll = "dllkd";
 *
 *   [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
 *   public static extern int GetKey(byte[] key); // key.Length >= 32
 *
 *   [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
 *   public static extern int SettingFileWrite(byte[] data, uint dataLen);
 *
 *   [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
 *   public static extern int SettingFileRead(byte[] data, uint dataLen, out uint rtnDataLen);
 *
 *   [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)]
 *   public static extern int SettingFileGetLen(out uint dataLen);
 */

#include <windows.h>

/* When building the DLL, vcxproj defines DLLKD_EXPORTS. KEYLIB_EXPORTS is accepted for compatibility. */
#if defined(KEYLIB_EXPORTS) || defined(DLLKD_EXPORTS)
#define KEYLIB_API __declspec(dllexport)
#else
#define KEYLIB_API __declspec(dllimport)
#endif

/** Error codes: APIs return 0 on success, or -(code) on failure. */
typedef enum KeyLibErrorCode {
	KEYLIB_OK = 0,

	PARAM_ERROR = 1, /**< Invalid argument (null pointer, invalid length, etc.) */

	/* GetKey */
	GET_ENCKEY_FAILED = 2, /**< Failed to read or process encrypted key material. */
	READ_KEYPRO_FAILED = 3, /**< Key-device communication failed. */
	DEC_KEY_FAILED = 4, /**< Decryption failed. */
	READ_REG_FAILED = 5, /**< Failed to read registry area. */
	KEY_SIZE_ERROR = 6, /**< Decrypted key length is not 32 bytes. */
	AREA_NOT_MATCH = 7, /**< Registry area does not match device area. */

	/* Setting file on USB (file stores payload + 4-byte CRC32 trailer) */
	USBDISK_NOT_FOUND = 8, /**< No suitable USB volume found. */
	FILE_WRITE_FAILED = 9,
	FILE_READ_FAILED = 10,
	DATALEN_ERROR = 11, /**< Output buffer smaller than file payload (excluding CRC). */
	GET_FILESIZE_FAILED = 12,
	FILESIZE_ERROR = 13, /**< File too small or malformed. */
	CRC_COMPARE_FAILED = 14, /**< Stored CRC does not match payload. */
	OTHER_ERROR = 15,
} KeyLibErrorCode;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Copies a 32-byte key into \p Key.
 * @param Key Must point to at least 32 writable bytes.
 * @return 0 on success, or a negative KeyLibErrorCode value.
 */
KEYLIB_API int GetKey(BYTE* Key);

/**
 * Writes \p Data to the setting file on the detected USB volume.
 * The implementation appends a CRC32 after \p Data in the file.
 *
 * @param Data Payload bytes (must not be NULL).
 * @param DataLen Byte length of \p Data; must be > 0.
 * @return 0 on success, or a negative KeyLibErrorCode value.
 */
KEYLIB_API int SettingFileWrite(BYTE* Data, DWORD DataLen);

/**
 * Reads the setting file from USB, verifies CRC, and copies the payload
 * (excluding the final 4-byte CRC) into \p Data.
 *
 * @param Data Caller-allocated buffer.
 * @param DataLen Size of \p Data in bytes.
 * @param RtnDataLen On success, receives payload length excluding CRC.
 * @return 0 on success, or a negative KeyLibErrorCode value.
 */
KEYLIB_API int SettingFileRead(BYTE* Data, DWORD DataLen, DWORD* RtnDataLen);

/**
 * Returns the payload byte length of the setting file (excluding the 4-byte CRC trailer).
 *
 * @param DataLen On success, receives the payload length in bytes.
 * @return 0 on success, or a negative KeyLibErrorCode value.
 */
KEYLIB_API int SettingFileGetLen(DWORD* DataLen);

#ifdef __cplusplus
}
#endif
