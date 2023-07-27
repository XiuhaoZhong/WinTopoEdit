#include "resource.h"
#include "CTedPlayer.h"

CTedVideoWindow::CTedVideoWindow() {

}

CTedVideoWindow::~CTedVideoWindow() {

}

void CTedVideoWindow::Init(CTedApp *pApp) {

}

LRESULT CTedVideoWindow::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) {
	return 0;
}

LRESULT CTedVideoWindow::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) {
	return 0;
}

LRESULT CTedVideoWindow::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) {
	return 0;
}

LRESULT CTedVideoWindow::OnSysCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) {
	if (wParam == SC_CLOSE) {
		bHandled = TRUE;

		HWND hWndParent = GetParent();
		::SendMessage(hWndParent, WM_CLOSE, MAKELONG(ID_PLAY_PLAY, 0), 0);
	} else {
		bHandled = FALSE
	}

	return 0;
}