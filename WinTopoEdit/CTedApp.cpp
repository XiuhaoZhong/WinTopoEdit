#include "CTedApp.h"

#include <commdlg.h>
#include <dmo.h>
#include <initguid.h>
#include <Mferror.h>
#include <uuids.h>
#include <windowsx.h>

#include "resource.h"
#include "CTedMainToolBar.h"
#include "CDock.h"
#include "CSplitterBar.h"
#include "CTedTransformDialog.h"
#include "CTedInputGuidDialog.h"
#include "PropertyView.h"
#include "CTedContentProtectionManager.h"
#include "CTedPlayer.h"

HINSTANCE g_hInst = NULL;
HINSTANCE g_hTedUtilInst = NULL;

CTedAppVideoWindowHandler::CTedAppVideoWindowHandler(HWND hWndParent) {
}

CTedApp::CTedApp() {

}

CTedApp::~CTedApp() {

}

HRESULT CTedApp::Init(LPCWSTR lpCmdLine) {
	return S_OK;
}
