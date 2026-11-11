/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "winsetup.h"

#ifdef TARGET_WIN32

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <urlmon.h>
#ifdef _MSC_VER
#pragma warning(pop)
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "urlmon.lib")
#endif

#include <stdio.h>
#include <string.h>
#include <wchar.h>

/*
 * The Tyrian 2000 data has been released as freeware, but without explicit
 * permission to redistribute it, so it is not bundled; it is fetched from the
 * same place the README points to.  The hash pins the exact archive so that
 * a modified or truncated download is never extracted.
 */
static const wchar_t data_zip_url[] = L"https://www.camanis.net/tyrian/tyrian2000.zip";
static const wchar_t data_zip_name[] = L"tyrian2000.zip";
static const unsigned long long data_zip_size = 5051363;
static const char data_zip_sha256[] = "348bc76e73514e452279b8730cf217daf0f70a282f07b6b94af653d87e921667";

static const wchar_t window_title[] = L"OpenTyrian2000";

static int message_box(const wchar_t *text, UINT type)
{
	return MessageBoxW(NULL, text, window_title, type | MB_SETFOREGROUND);
}

static bool file_exists(const wchar_t *path)
{
	DWORD attributes = GetFileAttributesW(path);
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

/** Gets the directory that contains the executable, without a trailing separator. */
static bool get_exe_dir(wchar_t *dir, DWORD size)
{
	DWORD length = GetModuleFileNameW(NULL, dir, size);
	if (length == 0 || length >= size)
		return false;

	wchar_t *last_separator = wcsrchr(dir, L'\\');
	if (last_separator == NULL)
		return false;
	*last_separator = L'\0';

	return true;
}

static bool join_path(wchar_t *out, size_t size, const wchar_t *dir, const wchar_t *name)
{
	int length = swprintf(out, size, L"%ls\\%ls", dir, name);
	return length > 0 && (size_t)length < size;
}

static bool file_has_sha256(const wchar_t *path, unsigned long long expected_size, const char *expected_hex)
{
	bool matches = false;

	FILE *f = _wfopen(path, L"rb");
	if (f == NULL)
		return false;

	BCRYPT_ALG_HANDLE algorithm = NULL;
	BCRYPT_HASH_HANDLE hash = NULL;

	if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0)))
		goto done;
	if (!BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, NULL, 0, NULL, 0, 0)))
		goto done;

	static unsigned char buffer[64 * 1024];
	unsigned long long total = 0;
	size_t count;
	while ((count = fread(buffer, 1, sizeof(buffer), f)) > 0)
	{
		if (!BCRYPT_SUCCESS(BCryptHashData(hash, buffer, (ULONG)count, 0)))
			goto done;
		total += count;
	}
	if (ferror(f) || total != expected_size)
		goto done;

	unsigned char digest[32];
	if (!BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof(digest), 0)))
		goto done;

	char hex[2 * sizeof(digest) + 1];
	for (size_t i = 0; i < sizeof(digest); ++i)
		snprintf(&hex[2 * i], 3, "%02x", digest[i]);

	matches = strcmp(hex, expected_hex) == 0;

done:
	if (hash != NULL)
		BCryptDestroyHash(hash);
	if (algorithm != NULL)
		BCryptCloseAlgorithmProvider(algorithm, 0);
	fclose(f);

	return matches;
}

static bool download_file(const wchar_t *url, const wchar_t *destination)
{
	HRESULT com_result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	HRESULT result = URLDownloadToFileW(NULL, url, destination, 0, NULL);

	if (SUCCEEDED(com_result))
		CoUninitialize();

	return SUCCEEDED(result);
}

/**
 * Extracts tyrian2000.zip from \p work_dir into \p work_dir\\data using the
 * tar.exe (bsdtar) that ships with Windows 10 1803 and later.  Relative paths
 * are passed so that the command line never contains non-ASCII characters.
 */
static bool extract_data_zip(const wchar_t *work_dir, bool strip_top_folder)
{
	wchar_t tar_path[MAX_PATH];
	UINT length = GetSystemDirectoryW(tar_path, MAX_PATH);
	if (length == 0 || length >= MAX_PATH - 16)
		return false;
	wcscat(tar_path, L"\\tar.exe");
	if (!file_exists(tar_path))
		return false;

	wchar_t command_line[2 * MAX_PATH];
	swprintf(command_line, sizeof(command_line) / sizeof(*command_line), L"\"%ls\" -xf %ls %ls-C data",
	         tar_path, data_zip_name, strip_top_folder ? L"--strip-components=1 " : L"");

	STARTUPINFOW startup_info;
	memset(&startup_info, 0, sizeof(startup_info));
	startup_info.cb = sizeof(startup_info);

	PROCESS_INFORMATION process_info;
	if (!CreateProcessW(tar_path, command_line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, work_dir, &startup_info, &process_info))
		return false;

	DWORD exit_code = 1;
	if (WaitForSingleObject(process_info.hProcess, 120 * 1000) == WAIT_OBJECT_0)
		GetExitCodeProcess(process_info.hProcess, &exit_code);
	else
		TerminateProcess(process_info.hProcess, 1);

	CloseHandle(process_info.hThread);
	CloseHandle(process_info.hProcess);

	return exit_code == 0;
}

bool win_install_data_files(void)
{
	wchar_t exe_dir[MAX_PATH];
	wchar_t zip_path[MAX_PATH];
	wchar_t download_path[MAX_PATH];
	wchar_t data_path[MAX_PATH];
	wchar_t check_path[MAX_PATH];

	if (!get_exe_dir(exe_dir, MAX_PATH) ||
	    !join_path(zip_path, MAX_PATH, exe_dir, data_zip_name) ||
	    !join_path(download_path, MAX_PATH, exe_dir, L"tyrian2000.zip.download") ||
	    !join_path(data_path, MAX_PATH, exe_dir, L"data") ||
	    !join_path(check_path, MAX_PATH, data_path, L"tyrian1.lvl"))
	{
		message_box(L"프로그램이 있는 폴더 경로를 확인할 수 없습니다.\n경로가 너무 길지 않은 폴더로 옮겨서 다시 실행해 주세요.", MB_ICONERROR);
		return false;
	}

	bool downloaded = false;

	if (!file_exists(zip_path))
	{
		int answer = message_box(
			L"Tyrian 2000 게임 데이터가 없습니다.\n\n"
			L"공식 프리웨어 배포처(camanis.net)에서 tyrian2000.zip(약 5MB)을 내려받아 "
			L"data 폴더에 설치할까요?\n"
			L"내려받는 동안 몇 초 정도 아무 창도 보이지 않을 수 있습니다.\n\n"
			L"직접 준비하려면 [아니요]를 누른 뒤, tyrian2000.zip을 이 프로그램과 "
			L"같은 폴더에 두고 다시 실행하세요.",
			MB_YESNO | MB_ICONQUESTION);
		if (answer != IDYES)
			return false;

		DeleteFileW(download_path);

		if (!download_file(data_zip_url, download_path))
		{
			DeleteFileW(download_path);
			message_box(
				L"tyrian2000.zip을 내려받지 못했습니다.\n\n"
				L"인터넷 연결을 확인하거나, 아래 주소에서 직접 받아 이 프로그램과 같은 폴더에 두고 다시 실행하세요.\n\n"
				L"https://www.camanis.net/tyrian/tyrian2000.zip",
				MB_ICONERROR);
			return false;
		}

		if (!file_has_sha256(download_path, data_zip_size, data_zip_sha256))
		{
			DeleteFileW(download_path);
			message_box(
				L"내려받은 파일이 예상한 tyrian2000.zip과 다릅니다. 안전을 위해 설치하지 않았습니다.\n\n"
				L"아래 주소에서 직접 받아 이 프로그램과 같은 폴더에 두고 다시 실행하세요.\n\n"
				L"https://www.camanis.net/tyrian/tyrian2000.zip",
				MB_ICONERROR);
			return false;
		}

		if (!MoveFileExW(download_path, zip_path, MOVEFILE_REPLACE_EXISTING))
		{
			DeleteFileW(download_path);
			message_box(L"내려받은 파일을 저장하지 못했습니다.\n프로그램 폴더에 쓰기 권한이 있는지 확인해 주세요.", MB_ICONERROR);
			return false;
		}

		downloaded = true;
	}

	CreateDirectoryW(data_path, NULL);

	// The official archive keeps everything in a "tyrian2000" folder; fall back
	// to a flat extraction for archives that were repacked without it.
	if (!(extract_data_zip(exe_dir, true) && file_exists(check_path)))
		extract_data_zip(exe_dir, false);

	if (!file_exists(check_path))
	{
		message_box(
			L"tyrian2000.zip의 압축을 풀지 못했습니다.\n\n"
			L"압축을 직접 풀어서, tyrian2000 폴더 안의 파일들을 이 프로그램 옆의 data 폴더에 넣은 뒤 다시 실행하세요.",
			MB_ICONERROR);
		return false;
	}

	if (downloaded)
		DeleteFileW(zip_path);

	return true;
}

#endif /* TARGET_WIN32 */
