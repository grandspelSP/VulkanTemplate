#include "Application.h"
#include "Window.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// ñ¢égópïœêîÇÃåxçêÇó}êß
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	auto app = std::make_unique<Application>();
	app->Initialize();

	auto& window = getAppWindow();
	while (!window->getIsExitRequired()) {
		window->processMessages();
		app->process();
	}

	app->Shutdown();
	return 0;
}
