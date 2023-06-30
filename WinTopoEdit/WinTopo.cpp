
#include "stdafx.h"
#include "CTedApp.h"

// ÏîÄ¿µØÖ· https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/multimedia/mediafoundation/topoedit/app/tedapp.cpp

HINSTANCE g_hInst = NULL;
HINSTANCE g_hTedUtilInst = NULL;
CTedApp *g_pApp;

HRESULT InitTedApp(LPCWSTR lpCmdLine, int nCmdShow) {
	HRESULT hr = S_OK;

	g_pApp = new CTedApp;
	if (g_pApp == NULL) {
		hr = E_OUTOFMEMORY;
		return hr;
	}

	IFC(g_pApp->Init(lpCmdLine));

	/* Make the window visible, update its client area; and return success. */
	g_pApp->ShowWindow(nCmdShow);
	g_pApp->UpdateWindow();

Cleanup:
	return hr;
}

BOOL WINAPI wWinMain(__in HINSTANCE hInstance,
					 __in_opt HINSTANCE hPrevInstance,
					 __in_opt LPWSTR lpCmdLine,
					 __in int nCmdShow) {

	HRESULT hr = S_OK;
	MSG msg;
	HACCEL hAccelTable;
	INITCOMMONCONTROLSEX iccex;

	g_hInst = hInstance;

	(void)HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

	g_hTedUtilInst = LoadLibraryEx(L"TEDUTIL.dll", NULL, 0);

	// initialize common controls.
	iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	iccex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
	InitCommonControlsEx(&iccex);

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TRV));

	/* Perform initializations that apply to a specific instance. */
	IFC(InitTedApp(lpCmdLine, nCmdShow));

	/* Acquire and dispatch messages until a WM_QUIT uMessage is received. */
	while (GetMessage(&msg, NULL, 0, 0)) {
		if (!TranslateAccelerator(g_pApp->getHwnd(), hAccelTable, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	delete g_pApp;

	DestroyAcceleratorTable(hAccelTable);

	::FreeLibrary(g_hTedUtilInst);

	return (int)msg.wParam;

Cleanup:
	assert(SUCCEEDED(hr));
	return FALSE;

}