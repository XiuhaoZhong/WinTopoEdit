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
#include "tedtranscode.h"

unsigned int CTedApp::MAIN_TOOLBAR_ID = 5000;
const int CTedApp::m_nSeekerRange = 100;
const UINT_PTR CTedApp::ms_nTimerID = 0;
const DWORD CTedApp::ms_dwTimerLen = 200;
const double CTedApp::m_dblInitalSplitterPos = 0.65;

HINSTANCE g_hInst = NULL;
HINSTANCE g_hTedUtilInst = NULL;

//////////////////////////////////
// CTedAppVideoWIndowHandler
// Provides video windows to TEDUTIL.
CTedAppVideoWindowHandler::CTedAppVideoWindowHandler(HWND hWndParent)
	: m_hWndParent(hWndParent)
	, m_cRef(0) {

}

CTedAppVideoWindowHandler::~CTedAppVideoWindowHandler() {
	for (size_t i = 0; i < m_arrWindows.GetCount(); i++) {
		if (m_arrWindows.GetAt(i)->m_hWnd) {
			m_arrWindows.GetAt(i)->DestroyWindow();
		}
		delete m_arrWindows.GetAt(i);
	}
}

HRESULT CTedAppVideoWindowHandler::GetVideoWindow(LONG_PTR *phWnd) {
	HRESULT hr = S_OK;

	RECT rectLastWindow = { 0, 0, 0, 0 };
	if (m_arrWindows.GetCount() >= 1) {
		m_arrWindows.GetAt(m_arrWindows.GetCount() - 1)->GetWindowRect(&rectLastWindow);
	}

	RECT rect;
	rect.left = m_dwCascadeMargin + rectLastWindow.left;
	rect.top = m_dwCascadeMargin + rectLastWindow.top;
	rect.right = rect.left + m_dwDefaultWindowWidth;
	rect.bottom = rect.bottom + m_dwDefaultWindowHeight;

	CTedVideoWindow* pVideoWindow = new CTedVideoWindow();
	if (phWnd == NULL) {
		delete pVideoWindow;
		IFC(E_POINTER);
	}

	if (pVideoWindow->Create(m_hWndParent, &rect, LoadAtlString(IDS_VIDEO_PLAYBACK), WS_CAPTION | WS_POPUPWINDOW, 0, 0U, NULL) == NULL) {
		IFC(HRESULT_FROM_WIN32(GetLastError()));
	}

	*phWnd = (LONG_PTR)pVideoWindow->m_hWnd;
	m_arrWindows.Add(pVideoWindow);

Cleanup:
	return hr;
}

HRESULT CTedAppVideoWindowHandler::ReleaseVideoWindow(LONG_PTR hwnd) {
	for (size_t i = 0; i < m_arrWindows.GetCount(); i++) {
		CTedVideoWindow *pWindow = m_arrWindows.GetAt(i);

		if (pWindow->m_hWnd == (HWND)hwnd) {
			m_arrWindows.RemoveAt(i);
			--i;

			pWindow->DestroyWindow();
			delete pWindow;
			break;
		}
	}

	return S_OK;
}

HRESULT CTedAppVideoWindowHandler::ShowWindows(int nCmdShow) {
	for (size_t i = 0; i < m_arrWindows.GetCount(); i++) {
		m_arrWindows.GetAt(i)->ShowWindow(nCmdShow);
	}

	return S_OK;
}

HRESULT CTedAppVideoWindowHandler::QueryInterface(REFIID riid, _COM_Outptr_ void **ppInterface) {
	if (ppInterface == NULL) {
		return E_POINTER;
	}

	HRESULT hr = S_OK;
	if (riid == IID_IUnknown) {
		IUnknown *punk = this;
		*ppInterface = punk;
		AddRef();
	} else if (riid == IID_ITedVideoWindowHandler) {
		ITedVideoWindowHandler *ptvmh = this;
		*ppInterface = ptvmh;
		AddRef();
	} else {
		*ppInterface = NULL;
		hr = E_NOINTERFACE;
	}

	return hr;
}

ULONG CTedAppVideoWindowHandler::AddRef() {
	LONG cRef = InterlockedIncrement(&m_cRef);

	return cRef;
}

ULONG CTedAppVideoWindowHandler::Release() {
	LONG cRef = InterlockedDecrement(&m_cRef);
	if (cRef == 0) {
		delete this;
	}

	return cRef;
}

/////////////////////////////////////////////////////////////////////
// CTedAppMediaEventHandler
// Decouples player from app class. Initiates application response to 
// media events
CTedAppMediaEventHandler::CTedAppMediaEventHandler(CTedApp *pApp)
	: m_pApp(pApp) {

}

CTedAppMediaEventHandler::~CTedAppMediaEventHandler() {

}

void CTedAppMediaEventHandler::NotifyEventError(HRESULT hr) {
	m_pApp->HandleMMError(LoadAtlString(IDS_E_MEDIA_EVENT), hr);
}

void CTedAppMediaEventHandler::HandleMediaEvent(IMFMediaEvent *pEvent) {
	HRESULT hr = S_OK;

	MediaEventType met;
	HRESULT hrEvent;

	IFC(pEvent->GetType(&met));
	IFC(pEvent->GetStatus(&hrEvent));

	if (SUCCEEDED(hrEvent)) {
		switch (met) {
		case MESessionStarted:
			m_pApp->PostMessage(WM_MF_SESSIONPLAY, hrEvent, 0);
			break;
		case MESessionEnded:
			m_pApp->PostMessage(WM_MF_SESSIONENDED, hrEvent, 0);
			break;
		case MESessionTopologyStatus: {
				UINT32 unTopoStatus = MFGetAttributeUINT32(pEvent, MF_EVENT_TOPOLOGY_STATUS, 0);
				if (unTopoStatus == MF_TOPOSTATUS_READY) {
					m_pApp->PostMessage(WM_MF_TOPOLOGYREADY, hrEvent, 0);
				}
			}
			break;
		case MESessionCapabilitiesChanged:
			m_pApp->PostMessage(WM_MF_CAPABILITIES_CHANGED, hrEvent, 0);
			break;
		}
	} else {
		switch (met) {
		case MESessionStarted:
			m_pApp->HandleMMError(LoadAtlString(IDS_E_PLAYBACK_START), hrEvent);
			break;
		case MESessionTopologiesCleared:
			// This can fail if the session has been closed, 
			// but this is OK, the session will still accept new topogies.
			break;
		default:
			NotifyEventError(hrEvent);
		}
	}

Cleanup:
	if (FAILED(hr)) {
		NotifyEventError(hr);
	}
}

////////////////////////////////////////////////////////////////////////
// CTedAppTopoEventHandler
// Receives event notifications from the topology editor.
CTedAppTopoEventHandler::CTedAppTopoEventHandler(CTedApp *pApp)
	: m_pApp(pApp) {

}

CTedAppTopoEventHandler::~CTedAppTopoEventHandler() {

}

HRESULT CTedAppTopoEventHandler::NotifyAddedNode(int nNodeID) {
	m_pApp->NotifyTopoChange();
	return S_OK;
}

HRESULT CTedAppTopoEventHandler::NotifyRemovedNode(int nNodeID) {
	m_pApp->NotifyTopoChange();
	return S_OK;
}

HRESULT CTedAppTopoEventHandler::NotifyConnection(int nUpNodeID, int nDownNodeID) {
	m_pApp->NotifyTopoChange();
	return S_OK;
}

HRESULT CTedAppTopoEventHandler::NotifyDisconnection(int nUpNodeID, int nDownNodeID) {
	m_pApp->NotifyTopoChange();
	return S_OK;
}

HRESULT CTedAppTopoEventHandler::QueryInterface(REFIID riid, _COM_Outptr_ void **ppInterface) {
	if (ppInterface == NULL) {
		return E_POINTER;
	}

	HRESULT hr = S_OK;
	if (riid == IID_IUnknown) {
		IUnknown *punk = this;
		*ppInterface = punk;
		AddRef();
	} else {
		*ppInterface = NULL;
		hr = E_NOINTERFACE;
	}

	return hr;
}

ULONG CTedAppTopoEventHandler::AddRef() {
	LONG cRef = InterlockedIncrement(&m_cRef);
	return cRef;
}

ULONG CTedAppTopoEventHandler::Release() {
	LONG cRef = InterlockedDecrement(&m_cRef);
	if (cRef == 0) {
		delete this;
	}

	return cRef;
}

CTedApp::CTedApp() {

}

CTedApp::~CTedApp() {

}

HRESULT CTedApp::Init(LPCWSTR lpCmdLine) {
	return S_OK;
}
