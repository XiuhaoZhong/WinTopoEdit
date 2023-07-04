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

//////////////////////////////////////////////////////////////
// CTedChooseDialog
// Generic drop-down selection dialog
CTedChooserDialog::CTedChooserDialog(const CAtlString &strTitle)
	: m_strTitle(strTitle) {

}

void CTedChooserDialog::AddProcessibleChoice(CAtlStringW strChoice) {
	m_arrChoices.Add(strChoice);
}

LRESULT CTedChooserDialog::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) {
	SetWindowText(m_strTitle);

	m_hChooserCombo = GetDlgItem(IDC_CHOOSER);
	for (DWORD i = 0; i < m_arrChoices.GetCount(); i++) {
		::SendMessage(m_hChooserCombo, CB_ADDSTRING, 0, (LPARAM)m_arrChoices.GetAt(i).GetString());
	}

	return 0;
}

LRESULT CTedChooserDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled) {
	HRESULT hr;
	int comboLength = ::GetWindowTextLength(m_hChooserCombo);

	LPWSTR strCombo = new WCHAR[comboLength + 1];
	CHECK_ALLOC(strCombo);

	::GetWindowText(m_hChooserCombo, strCombo, comboLength + 1);
	m_strChoice = CAtlStringW(strCombo);

Cleanup:
	EndDialog(IDOK);
	return 0;
}

LRESULT CTedChooserDialog::OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled) {
	EndDialog(IDCANCEL);

	return 0;
}

////////////////////////////////////////////////////////////////////
// CTedAboutDialog
LRESULT CTedAboutDialog::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) {
	WCHAR szFileName[1024];
	if (!GetModuleFileName(g_hInst, szFileName, 1024)) {
		return GetLastError();
	}

	HANDLE hFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == NULL) {
		return GetLastError();
	}

	FILETIME LastModifiedTime;
	if (!GetFileTime(hFile, NULL, NULL, &LastModifiedTime)) {
		return GetLastError();
	}

	SYSTEMTIME SystemTime;
	if (!FileTimeToSystemTime(&LastModifiedTime, &SystemTime)) {
		return GetLastError();
	}

	SYSTEMTIME LocalTime;
	if (!SystemTimeToTzSpecificLocalTime(NULL, &SystemTime, &LocalTime)) {
		return GetLastError();
	}

	WCHAR szVersion[10];
	if (LoadString(g_hInst, IDS_VERSION, szVersion, 10) == 0) {
		return GetLastError();
	}

	CAtlString strVersion;
	strVersion.FormatMessage(IDS_APP_VERSION, szVersion, LocalTime.wMonth, LocalTime.wDay, LocalTime.wYear);
	SetDlgItemText(IDS_VERSION, strVersion.GetString());

	return 0;
}

////////////////////////////////////////////////
// CTedInputURLDialog
CAtlString CTedInputURLDialog::GetURL() {
	return m_strURL;
}

LRESULT CTedInputURLDialog::OnOk(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled) {
	HRESULT hr;

	HWND hInputURLWnd = GetDlgItem(IDC_INPUTURL);
	int iTextLength = ::GetWindowTextLength(hInputURLWnd);

	LPWSTR szURL = new WCHAR[iTextLength + 1];
	CHECK_ALLOC(szURL);

	::GetWindowText(hInputURLWnd, szURL, iTextLength + 1);
	m_strURL.SetString(szURL);

	delete[] szURL;
	
Cleanup:
	EndDialog(IDOK);
	return 0;
}

///////////////////////////////////////////////
// CTedCaptureSourceDialog
CTedCaptureSourceDialog::CTedCaptureSourceDialog(bool bVideo)
	: m_ppSourceActivates(NULL)
	, m_dwActivates(0)
	, m_dwSelectedIndex(0) {

	EnumCaptureSources(bVideo);
}

CTedCaptureSourceDialog::~CTedCaptureSourceDialog() {
	if (m_ppSourceActivates) {
		for (DWORD i = 0; i < m_dwActivates; i++) {
			m_ppSourceActivates[i]->Release();
		}

		CoTaskMemFree(m_ppSourceActivates);
	}
}

IMFActivate* CTedCaptureSourceDialog::GetSourceActivate() {
	IMFActivate* pActivate = NULL;

	if ((m_ppSourceActivates != NULL) &&
		(m_dwSelectedIndex < m_dwActivates)) {
		pActivate = m_ppSourceActivates[m_dwSelectedIndex];
		pActivate->AddRef();
	}
}

LRESULT CTedCaptureSourceDialog::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) {
	/*
		Fill in the dropdown valus based on enumerated activates.
	*/
	for (DWORD i = 0; i < m_dwActivates; i++) {
		UINT32 nLen = 0;
		WCHAR* pwsz = NULL;
		HRESULT hr = S_OK;

		// Get the friendly name.
		hr = m_ppSourceActivates[i]->GetStringLength(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &nLen);
		if (SUCCEEDED(hr)) {
			pwsz = new WCHAR[nLen + 1];
			if (pwsz == NULL) {
				hr = E_OUTOFMEMORY;
			}
		}

		if (SUCCEEDED(hr)) {
			hr = m_ppSourceActivates[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
												   pwsz,
												   nLen + 1,
												   &nLen);
		}

		// Add to the dropdown
		if (SUCCEEDED(hr)) {
			ComboBox_AddString(GetDlgItem(IDC_COMBOSOURCES), pwsz);
		} else {
			ComboBox_AddString(GetDlgItem(IDC_COMBOSOURCES), TEXT("Capture Source"));
		}

		if (pwsz) {
			delete[] pwsz;
		}
	}

	if (m_dwActivates != 0) {
		// present the first one.
		ComboBox_AddString(GetDlgItem(IDC_COMBOSOURCES), 0);
	}

	return 0;
}

LRESULT CTedCaptureSourceDialog::OnOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled) {
	m_dwSelectedIndex = ComboBox_GetCurSel(GetDlgItem(IDC_COMBOSOURCES));

	EndDialog(IDOK);
	return 0;
}

HRESULT CTedCaptureSourceDialog::EnumCaptureSources(bool bVideo) {
	HRESULT hr = S_OK;
	IMFAttributes* pAttributes = NULL;

	// Set source type GUID to indicate video capture deivces.
	hr = MFCreateAttributes(&pAttributes, 10);
	if (SUCCEEDED(hr)) {
		if (bVideo) {
			hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
		} else {
			hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID);
		}
	}

	// Enumerate the capture sources.
	if (SUCCEEDED(hr)) {
		hr = MFEnumDeviceSources(pAttributes, &m_ppSourceActivates, (UINT32*)&m_dwActivates);
	}

	if (pAttributes) {
		pAttributes->Release();
	}

	return hr;
}

//////////////////////////////////////////////////////////
// CTedApp
// Main application window controller.
CTedApp::CTedApp()
	: m_pPlayer(NULL)
	, m_pVideoWindowHandler(NULL)
	, m_pTopoEventHandler(NULL)
	, m_pMediaEventHandler(NULL)
	, m_pCPM(NULL)
	, m_pPropertyController(NULL)
	, m_pDock(NULL)
	, m_pSplitter(NULL)
	, m_pMainToolBar(NULL)
	, m_fMergeRequired(false)
	, m_fResolved(false)
	, m_fPendingPlay(false)
	, m_fStopTrackingUintSeesionStarted(false)
	, m_pPendingTopo(NULL)
	, m_pTopoView(NULL)
	, m_fCanSeek(false) {
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	if (FAILED(MFStartup(MF_VERSION))) {
		assert(false);
	}
}

CTedApp::~CTedApp() {
	if (m_pTopoView) {
		m_pTopoView->CloseTopoWindow();
		m_pTopoView->Release();
	}

	if (m_pVideoWindowHandler) {
		m_pVideoWindowHandler->Release();
	}

	if (m_pTopoEventHandler) {
		m_pTopoEventHandler->Release();
	}

	if (m_pPropertyController) {
		m_pPropertyController->Release();
	}

	if (m_pCPM) {
		m_pCPM->Release();
	}

	delete m_pMainToolBar;
	delete m_pDock;
	delete m_pSplitter;
	delete m_pPlayer;
	delete m_pPropertyController;
	delete m_pMediaEventHandler;

	if (m_pPendingTopo) {
		m_pPendingTopo->Release();
	}

	MFShutdown();
	CoUninitialize();
}

HRESULT CTedApp::Init(LPCWSTR lpCmdLine) {
	HRESULT hr = S_OK;

	// create window.
	m_hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDC_TRV));
	if (m_hMenu == NULL) {
		hr = HRESULT_FROM_WIN32(GetLastError());
		goto Cleanup;
	}

	if (Create(NULL, NULL, LoadAtlString(IDS_WDW_NAME), WS_OVERLAPPEDWINDOW, 0, m_hMenu, NULL) == NULL) {
		hr = HRESULT_FROM_WIN32(GetLastError());
		goto Cleanup;
	}

	if (lpCmdLine && lpCmdLine[0] != 0) {
		LoadFile(lpCmdLine);
	}

Cleanup:
	if (FAILED(hr)) {
		HandleMMError(LoadAtlString(IDS_E_APP_INIT));
	}
	return hr;
}

void CTedApp::HandleMMError(const CAtlStringW &message, HRESULT errResult) {
	m_MFErrorHandler.HandleMFError(message, errResult);
}

void CTedApp::NotifySplitterMoved() {
	// NO processing currently needed after splitter movement.
}

void CTedApp::NotifyTopoChange() {
	m_pMainToolBar->MarkResolved(false);
	m_fResolved = false;
}

void CTedApp::HandleSeekerScroll(WORD wPos) {
	HRESULT hr = S_OK;

	if (m_pPlayer && m_pPlayer->IsPlaying() && m_fCanSeek) {
		MFTIME duration;
		IFC(m_pPlayer->GetDuration(duration));

		MFTIME seekTime = (wPos * duration) / m_nSeekerRange;
		IFC(m_pPlayer->PlayFrom(seekTime));

		m_fStopTrackingUintSeesionStarted = true;
	}

Cleanup:
	if (FAILED(hr)) {
		HandleMMError(LoadAtlString(IDS_E_PLAY_MEDIA), hr);
	}
}

void CTedApp::HandleRateScroll(WORD wPos) {
	HRESULT hr = S_OK;
	if (m_pPlayer && m_pPlayer->IsTopologySet()) {
		float flRate = wPos / 10.0f;

		if (flRate != 0) {
			hr = m_pPlayer->SetRate(flRate);
		}

		m_pMainToolBar->UpdateRateDisplay(flRate);
	}

	if (FAILED(hr)) {
		HandleMMError(LoadAtlString(IDS_E_PLAY_RATE), hr);
	}
}

LRESULT CTedApp::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) {
	HRESULT hr = S_OK;
	RECT rect;
	RECT toolRect;

	m_pMediaEventHandler = new CTedAppMediaEventHandler(this);
	CHECK_ALLOC(m_pMediaEventHandler);

	// create toolbar.
	m_pMainToolBar = new CTedMainToolBar();;
	CHECK_ALLOC(m_pMainToolBar);
	IFC(m_pMainToolBar->Init(m_hWnd, MAIN_TOOLBAR_ID));
	m_pMainToolBar->SetTrackbarScrollCallback(&HandleSeekerScrollFunc);
	m_pMainToolBar->GetRateBar()-> SetScrollCallback(&HandleRateScrollFunc);
	m_pMainToolBar->ShowRateBar(SW_HIDE);

	m_pMainToolBar->GetClientRect(&toolRect);
	GetClientRect(&rect);

	rect.top = toolRect.bottom;
	rect.bottom = rect.bottom - 100;

	m_pDock = new CDock();
	CHECK_ALLOC(m_pDock);


Cleanup:
	return hr;
}
