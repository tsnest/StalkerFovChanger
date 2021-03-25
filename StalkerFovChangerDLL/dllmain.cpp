#include <Windows.h>

typedef void (__stdcall* /*__cdecl*/ _Msg) (LPCSTR format, ...); // xrCore Msg
typedef LPCSTR (__stdcall* _xrGS_GetGameVersion) (LPCSTR keyValue); // xrGameSpy xrGS_GetGameVersion

const char* regSubKey = "Software\\StalkerFovChanger";

void WriteFovToReg(float fov, char* game_version)
{
	HKEY hKey = NULL;
	DWORD disposition = 0;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, regSubKey, 0, NULL, 0, KEY_WRITE, 0, &hKey, &disposition) == ERROR_SUCCESS)
	{
		// Записываем fov
		DWORD fov_dw = *(DWORD*)&fov;
		RegSetValueEx(hKey, game_version, 0, REG_DWORD, (LPBYTE)&fov_dw, sizeof(DWORD));
		RegCloseKey(hKey);
	}
}

float ReadFovFromReg(float fov_default, char* game_version)
{
	HKEY hKey = NULL;
	DWORD disposition = 0;
	float fov = fov_default;
	DWORD fov_default_dw = *(DWORD*)&fov_default;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, regSubKey, 0, NULL, 0, KEY_READ | KEY_WRITE, 0, &hKey, &disposition) == ERROR_SUCCESS)
	{
		if (disposition == 1)
		{
			// Раздел только-что создан. Записываем значение.
			RegSetValueEx(hKey, game_version, 0, REG_DWORD, (LPBYTE)&fov_default_dw, sizeof(DWORD));
		}
		else if (disposition == 2)
		{
			// Открыт существующий раздел. Читаем значение.
			DWORD fov_dw = 0;
			DWORD keyValueSize = sizeof(DWORD);
			DWORD keyValueType = REG_DWORD;
			LSTATUS status = RegQueryValueEx(hKey, game_version, NULL, &keyValueType, (LPBYTE)&fov_dw, &keyValueSize);
			if (status == ERROR_SUCCESS)
			{
				fov = *(float*)&fov_dw;
				// Проверяем валидность прочтённого fov. Если не валиден, то записываем fov_default
				if (fov < fov_default || fov > 90.0f)
				{
					RegSetValueEx(hKey, game_version, 0, REG_DWORD, (LPBYTE)&fov_default_dw, sizeof(DWORD));
					fov = fov_default;
				}
			}
			else if (status == ERROR_FILE_NOT_FOUND)
			{
				// Значение почему-то не оказалось в разделе.
				// (например ранее был запуск на ЗП, а сейчас на ТЧ)
				// Записываем fov_default
				RegSetValueEx(hKey, game_version, 0, REG_DWORD, (LPBYTE)&fov_default_dw, sizeof(DWORD));
			}
		}
		RegCloseKey(hKey);
	}
	return fov;
}

DWORD WINAPI FovChangerThread(HMODULE hModule)
{
	DWORD xrCore = 0;
	DWORD xrGame = 0;
	DWORD xrGameSpy = 0;

	while ((xrCore = (DWORD) GetModuleHandle("xrCore")) == 0) {}
	while ((xrGame = (DWORD) GetModuleHandle("xrGame")) == 0) {}
	while ((xrGameSpy = (DWORD) GetModuleHandle("xrGameSpy")) == 0) {}

	// в младших 16 битах GetModuleHandle может возвращать флаги - поэтому обнуляем их для получения адреса загрузки модуля
	xrCore = (xrCore >> 16) << 16;
	xrGame = (xrGame >> 16) << 16;
	xrGameSpy = (xrGameSpy >> 16) << 16;

	_Msg Msg = (_Msg)GetProcAddress((HMODULE)xrCore, "?Msg@@YAXPBDZZ");
	
	if (Msg != NULL) {
		Msg("[FovChanger] Loading...");

		_xrGS_GetGameVersion xrGS_GetGameVersion = (_xrGS_GetGameVersion)GetProcAddress((HMODULE)xrGameSpy, "xrGS_GetGameVersion");
		if (xrGS_GetGameVersion != NULL) {
			static char buff[256];
			static char	game_version[256];
			strcpy_s(game_version, xrGS_GetGameVersion(buff));

			int game = 0;
			if (strcmp(game_version, "1.0006") == 0) {
				game = 1;
			} else if (strcmp(game_version, "1.5.10") == 0) {
				game = 2;
			} else if (strcmp(game_version, "1.6.02") == 0) {
				game = 3;
			}

			if (game != 0) {
				float* g_fov = (float*)(xrGame + (game == 1 ? 0x53C598 : game == 2 ? 0x5DC8F8 : game == 3 ? 0x635C44 : 0));
				float fov_default = *g_fov;
				*g_fov = ReadFovFromReg(fov_default, game_version);

				Msg("[FovChanger] Loaded successfully! Default fov = %g, Loaded fov = %g", fov_default, *g_fov);
				Beep(1000, 200);

				bool isFovChanged = false;
				while (true) {
					if (GetAsyncKeyState(VK_OEM_PLUS)) {
						if (*g_fov < 90.0f)
						{
							*g_fov += 0.5f;
							isFovChanged = true;
							Msg("* FOV: %g", *g_fov);
						}
					} else if (GetAsyncKeyState(VK_OEM_MINUS)) {
						if (*g_fov > fov_default)
						{
							*g_fov -= 0.5f;
							isFovChanged = true;
							Msg("* FOV: %g", *g_fov);
						}
					} else {
						if (isFovChanged)
						{
							isFovChanged = false;
							WriteFovToReg(*g_fov, game_version);
						}
					}

					Sleep(100);
				}
			} else {
				Msg("! [FovChanger] Inappropriate version of the game.");
			}
		} else {
			Msg("! [FovChanger] xrGS_GetGameVersion == NULL");
		}
	}
	
	FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH: {
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)FovChangerThread, hModule, NULL, NULL);
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

