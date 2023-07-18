#include "stdafx.h"

#include "PropertyView.h"

/****************************\

  * CPropertyController

\****************************/

CPropertyController::CPropertyController(CPropertyEditWindow *pView)
	: m_cRef(0)
	, m_pView(pView) {

}

CPropertyController::~CPropertyController() {
	delete m_pView;

	for (size_t i = 0; i < m_arrPropertyInfo.GetCount(); i++) {
		m_arrPropertyInfo.GetAt(i)->Release();
	}
}

HRESULT CPropertyController::ClearProperties() {
	for (size_t i = 0; i < m_arrPropertyInfo.GetCount(); i++) {
		m_arrPropertyInfo.GetAt(i)->Release();
	}

	m_arrPropertyInfo.RemoveAll();
	m_pView->ClearProperties();

	return S_OK;
}

HRESULT CPropertyController::AddPropertyInfo(ITedPropertyInfo* pPropertyInfo) {
	HRESULT hr = S_OK;

	if (pPropertyInfo == NULL) {
		return E_POINTER;
	}

	m_pView->ShowProperies(pPropertyInfo);

	return hr;
}

CPropertyEditWindow* CPropertyController::GetWindow() const {
	return m_pView;
}

HRESULT CPropertyController::QueryInterface(REFIID riid, void **ppInterface) {
	if (ppInterface == NULL) {
		return E_POINTER;
	}

	if (riid == IID_IUnknown || riid == IID_ITedPropertyController) {
		*ppInterface = this;
		AddRef();
		return S_OK;
	}

	*ppInterface = NULL;
	return E_NOINTERFACE;
}

ULONG CPropertyController::AddRef() {
	LONG cRef = InterlockedIncrement(&m_cRef);

	if (cRef == 0) {
		delete this;
	}

	return cRef;
}

/****************************************\

  * TEDUTIL functions

\****************************************/
DWORD TEDGetAttributeListLength();
LPCWSTR TEDGetAttributeName(DWORD dwIndex);
GUID TEDGetAttributeGUID(DWORD dwIndex);
VARTYPE TEDGetAttributeType(DWORD dwIndex);
TED_ATTRIBUTE_CATEGORY TEDGetAttributeCategory(DWORD dwIndex);

/*****************************************\

  * CPropertyEditWindow

\*****************************************/

const int CPropertyEditWindow::ms_LabelWidth = 250;
const int CPropertyEditWindow::ms_LabelHeight = 15;
const int CPropertyEditWindow::ms_EditWidth = 250;
const int CPropertyEditWindow::ms_ButtonWidth = 50;
const int CPropertyEditWindow::ms_ButtonHeight = 20;
const int CPropertyEditWindow::ms_MarginWidth = 5;

CPropertyEditWindow::CPropertyEditWindow()
	: m_hLabelFont(NULL) {

}

CPropertyEditWindow::~CPropertyEditWindow() {
	for (size_t i = 0; i < m_arrPropertyInfo.GetCount; i++) {
		PropertyInfoDisplay* pDisplay = m_arrPropertyInfoDisplay.GetAt(i);
		for (size_t j = 0; j < pDisplay->m_arrLabels.GetCount(); i++) {
			delete pDisplay->m_arrLabels.GetAt(j);
			delete pDisplay->m_arrEdits.GetAt(j);
			delete pDisplay->m_arrTooltips.GetAt(j);
		}

		delete pDisplay;

		delete m_arrTitles.GetAt(i);

		m_arrPropertyInfo.GetAt(i)->Release();
	}

	if (m_hLabelFont) {
		DeleteObject(m_hLabelFont);
	}
}

void CPropertyEditWindow::ShowProperies(ITedPropertyInfo *pPropertyInfo) {
	// Don't redraw window every time we add a component;
	LockWindowUpdate();

	m_arrPropertyInfo.Add(pPropertyInfo);
	pPropertyInfo->AddRef();

	PropertyInfoDisplay* pDisplay = new PropertyInfoDisplay();
	m_arrPropertyInfoDisplay.Add(pDisplay);

	RECT labelRect = m_lastRect;

	// Add a margin in between sets of properties;
	if (m_arrPropertyInfo.GetCount() > 1) {
		labelRect.top += ms_LabelHeight;
		labelRect.bottom += ms_LabelHeight;
	}

	CreateTitle(DWORD(m_arrPropertyInfo.GetCount() - 1), labelRect);
	DWORD dwCount = 0;
	pPropertyInfo->GetPropertyCount(&dwCount);
	for (DWORD i = 0; i < dwCount; i++) {
		CreatePropertyInterfaceForIndex(DWORD(m_arrPropertyInfo.GetCount() - 1), i, labelRect);
	}

	m_AddButton.ShowWindow(SW_SHOW);
	m_OKButton.ShowWindow(SW_SHOW);

	LockWindowUpdate(FALSE);
}

void CPropertyEditWindow::ClearProperties() {
	if (m_arrPropertyInfo.GetCount() > 0) {
		// Don't redraw window every time we remove a component;
		LockWindowUpdate();
	}

	for (size_t i = 0; i < m_arrPropertyInfo.GetCount(); i++) {
		PropertyInfoDisplay* pDisplay = m_arrPropertyInfoDisplay.GetAt(i);
		for (size_t j = 0; j < pDisplay->m_arrLabels.GetCount(); j++) {
			pDisplay->m_arrLabels.GetAt(j)->DestroyWindow();
			delete pDisplay->m_arrLabels.GetAt(j);
			pDisplay->m_arrEdits.GetAt(j)->DestroyWindow();
			delete pDisplay->m_arrEdits.GetAt(j);
			pDisplay->m_arrTooltips.GetAt(j)->DestroyWindow();
			delete pDisplay->m_arrTooltips.GetAt(j);
		}

		delete pDisplay;

		m_arrTitles.GetAt(i)->DestroyWindow();
		delete m_arrTitles.GetAt(i);

		m_arrPropertyInfo.GetAt(i)->Release();
	}

	m_arrPropertyInfo.RemoveAll();
	m_arrPropertyInfoDisplay.RemoveAll();
	m_arrTitles.RemoveAll();

	RECT clientRect;
	GetClientRect(&clientRect);
	DWORD dwViewWidth = clientRect.right - clientRect.left - 5;

	m_lastRect.top = 5;
	m_lastRect.left = 5;
	m_lastRect.right = m_lastRect.left + dwViewWidth / 2;
	m_lastRect.bottom = m_lastRect.top + ms_LabelHeight;

	m_AddButton.ShowWindow(SW_HIDE);
	m_OKButton.ShowWindow(SW_HIDE);

	if (m_arrPropertyInfo.GetCount() > 0) {
		LockWindowUpdate(FALSE);
	}
}

void CPropertyEditWindow::OnFinalMessage(HWND hwnd) {

}

LRESULT CPropertyEditWindow::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) {
	CAtlString strFontSize = LoadAtlString(IDS_FONT_SIZE_14);
	CAtlString strFontFace = LoadAtlString(IDS_FONT_FACE_ARIAL);

	LOGFONT lfLabelFont;
	lfLabelFont.lfHeight = _wtol(strFontSize);
	lfLabelFont.lfWeight = 0;
	lfLabelFont.lfEscapement = 0;
	lfLabelFont.lfOrientation = 0;
	lfLabelFont.lfWeight = FW_DONTCARE;
	lfLabelFont.lfItalic = FALSE;
	lfLabelFont.lfUnderline = FALSE;
	lfLabelFont.lfStrikeOut = FALSE;
	lfLabelFont.lfCharSet = DEFAULT_CHARSET;
	lfLabelFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	lfLabelFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	lfLabelFont.lfQuality = DEFAULT_QUALITY;
	lfLabelFont.lfPitchAndFamily = FF_DONTCARE | DEFAULT_PITCH;
	wcscpy_s(lfLabelFont.lfFaceName, 32, strFontFace);
	m_hLabelFont = CreateFontIndirect(&lfLabelFont);

	RECT buttonRect;
	GetClientRect(&buttonRect);
	buttonRect.right -= ms_ButtonWidth + ms_MarginWidth + ms_MarginWidth;
	buttonRect.bottom -= ms_MarginWidth;
	buttonRect.top = buttonRect.bottom - ms_ButtonHeight;
	buttonRect.left = buttonRect.right - ms_ButtonWidth;
	m_AddButton.Create(m_hWnd, &buttonRect, LoadAtlString(IDS_ADD), WS_CHILD | BS_DEFPUSHBUTTON, 0, IDC_ADDBUTTON);

	buttonRect.right += ms_ButtonWidth + ms_MarginWidth;
	buttonRect.left += ms_ButtonWidth + ms_MarginWidth;
	m_OKButton.Create(m_hWnd, &buttonRect, LoadAtlString(IDS_SAVE), WS_CHILD | BS_DEFPUSHBUTTON, 0, IDC_OKBUTTON);

	return 0;
}

LRESULT CPropertyEditWindow::OnEraseBkgId(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) {
	HDC hdc = (HDC)wParam;

	RECT rect;
	GetClientRect(&rect);

	HBRUSH brush = CreateSolidBrush(GetSysColor(COLOR_3DFACE));
	HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);

	Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
	
	SelectObject(hdc, oldBrush);
	DeleteObject(oldBrush);

	return 1;
}

LRESULT CPropertyEditWindow::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled) {
	ResizeChildren();

	return 0;
}

LRESULT CPropertyEditWindow::OnOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled) {
	bool fAllSucceeded = true;

	for (size_t i = 0; i < m_arrPropertyInfo.GetCount(); i++) {
		ITedPropertyInfo* pPropertyInfo = m_arrPropertyInfo.GetAt(i);
		PropertyInfoDisplay* pDisplay = m_arrPropertyInfoDisplay.GetAt(i);
		for (size_t j = 0; j < pDisplay->m_arrLabels.GetCount(); j++) {
			VARTYPE vt = pDisplay->m_arrVartypes.GetAt(j);

			if (vt == VT_EMPTY || vt == VT_UNKNOWN || vt == (VT_VECTOR) | VT_UI1) {
				continue;
			}

			LPWSTR strName, strValue;

			// Get label text(property name);
			int nameLen = pDisplay->m_arrLabels.GetAt(j)->GetWindowTextLength();
			strName = new WCHAR[nameLen + 1];
			if (strName == NULL) {
				continue;
			}

			pDisplay->m_arrLabels.GetAt(j)->GetWindowText(strName, nameLen + 1);

			// Get edit text (property value)
			int valueLen = pDisplay->m_arrEdits.GetAt(j)->GetWindowTextLength();
			strValue = new WCHAR[valueLen + 1];
			if (strValue == NULL) {
				delete[] strName;
				continue ;
			}
			pDisplay->m_arrEdits.GetAt(j)->GetWindowText(strValue, valueLen + 1);

			HRESULT hr = pPropertyInfo->SetProperty((DWORD)j, strName, vt, strValue);
			if (FAILED(hr) && fAllSucceeded) {
				CAtlStringW strError;
				strError.FormatMessage(IDS_E_SAVE_PROP, hr);
				MessageBox(strError, LoadAtlString(IDS_ERROR), MB_OK);

				fAllSucceeded = false;
			}

			delete[] strName;
			delete[] strValue;
		}
	}

	return 0;
}

LRESULT CPropertyEditWindow::OnAdd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled) {
	CPropertyAddDialog dialog;

	// Add the property categories;
	for (size_t i = 0; i < m_arrPropertyInfo.GetCount(); i++) {
		if (m_arrPropertyInfo.GetAt(i)->IsWriteable() == S_OK) {
			LPWSTR szPropertyCategory;
			TED_ATTRIBUTE_CATEGORY Category;
			m_arrPropertyInfo.GetAt(i)->GetPropertyInfoName(&szPropertyCategory, &Category);
			dialog.AddPropertyCategory(szPropertyCategory, Category, (DWORD)i);
			CoTaskMemFree(szPropertyCategory);
		}
	}

	// Fill dialog properties;
	if (dialog.DoModal() == IDOK) {
		DWORD dwIndex = dialog.GetChosenCategory();
		CAtlStringW strName = dialog.GetChosenProperty();
		CAtlStringW strValue = dialog.GetValue();
		ITedPropertyInfo* pPropertyInfo = m_arrPropertyInfo.GetAt(dwIndex);

		for (DWORD i = 0; i < TEDGetAttributeListLength(); i++) {
			if (TEDGetAttributeName(i) == strName) {
				DWORD dwPropertyCount;

				pPropertyInfo->GetPropertyCount(&dwPropertyCount);
				pPropertyInfo->SetProperty(dwPropertyCount, strName.GetBuffer(), TEDGetAttributeType(i), strValue.GetBuffer());

				RECT clientRect;
				GetClientRect(&clientRect);
				DWORD dwViewWidth = clientRect.right - clientRect.left - 5;

				RECT rectNextLabel;
				rectNextLabel.top = 5 + (ms_LabelHeight * dwPropertyCount);
				rectNextLabel.left = 5;
				rectNextLabel.right = rectNextLabel.left + dwViewWidth / 2;
				rectNextLabel.bottom = rectNextLabel.top + ms_LabelHeight;

				CreatePropertyInterfaceForIndex(dwIndex, dwPropertyCount, rectNextLabel);
				ResizeChildren();

				break;
			}
		}
	}

	return 0;
}

HRESULT CPropertyEditWindow::CreateTitle(DWORD dwPropertyInfoIndex, RECT &labelRect) {
	HRESULT hr = S_OK;
	LPWSTR szName = NULL;

	TED_ATTRIBUTE_CATEGORY category;
	IFC(m_arrPropertyInfo[dwPropertyInfoIndex]->GetPropertyInfoName(&szName, &category));

	CStatic* pStatic = new CStatic();
	CHECK_ALLOC(pStatic);
	pStatic->Create(m_hWnd, &labelRect, szName, WS_CHILD | WS_VISIBLE);
	pStatic->SetFont(m_hLabelFont);
	m_arrTitles.Add(pStatic);

	labelRect.top += ms_LabelHeight;
	labelRect.bottom += ms_LabelHeight;

Cleanup:
	CoTaskMemFree(szName);
	return hr;
}

HRESULT CPropertyEditWindow::CreatePropertyInterfaceForIndex(DWORD dwPropertyInfoIndex, DWORD dwIndex, RECT &labelRect) {
	HRESULT hr = S_OK;

	LPWSTR strName = NULL;
	LPWSTR strValue = NULL;
	VARTYPE vt;

	ITedPropertyInfo* pPropertyInfo = m_arrPropertyInfo[dwPropertyInfoIndex];
	PropertyInfoDisplay* pDisplay = m_arrPropertyInfoDisplay[dwPropertyInfoIndex];

	RECT clientRect;
	GetClientRect(&clientRect);
	DWORD dwViewWidth = clientRect.right - clientRect.left - ms_MarginWidth;

	IFC(pPropertyInfo->GetProperty(dwIndex, &strName, &strValue));
	IFC(pPropertyInfo->GetPropertyType(dwIndex, &vt));
	IFC(CreatePropertyLabel(dwPropertyInfoIndex, strName, labelRect));

	labelRect.left = labelRect.right + ms_MarginWidth;
	labelRect.right = dwViewWidth - ms_MarginWidth;

	bool fReadOnly = false;
	if (vt == VT_EMPTY || vt == VT_UNKNOWN || vt == (VT_VECTOR | VT_UI1)) {
		fReadOnly = true;
	}

	IFC(CreatePropertyEdit(dwPropertyInfoIndex, strValue, labelRect, fReadOnly));

	RECT rectEditClient;
	CEdit* pEditWnd = pDisplay->m_arrEdits.GetAt(pDisplay->m_arrEdits.GetCount() - 1);
	pEditWnd->GetClientRect(&rectEditClient);
	IFC(CreatePropertyToolTip(dwPropertyInfoIndex, pEditWnd->m_hWnd, GetTextForVartype(vt), rectEditClient));
	pEditWnd->SetToolTipControl(pDisplay->m_arrTooltips.GetAt(pDisplay->m_arrTooltips.GetCount() - 1));

	m_arrPropertyInfoDisplay[dwPropertyInfoIndex]->m_arrVartypes.Add(vt);

	labelRect.left = ms_MarginWidth;
	labelRect.right = labelRect.left + dwViewWidth / 2;
	labelRect.top += ms_LabelHeight;
	labelRect.bottom += ms_LabelHeight;

	m_lastRect = labelRect;

Cleanup:
	if (strName) {
		CoTaskMemFree(strName);
	}

	if (strValue) {
		CoTaskMemFree(strValue);
	}

	// If there was a failure, roll back changes to the property displays;
	if (FAILED(hr)) {
		PropertyInfoDisplay* pDisplay = m_arrPropertyInfoDisplay[dwPropertyInfoIndex];
		if (pDisplay->m_arrVartypes.GetCount() < pDisplay->m_arrEdits.GetCount()) {
			DWORD dwIndex = DWORD(pDisplay->m_arrEdits.GetCount() - 1);
			pDisplay->m_arrEdits[dwIndex]->DestroyWindow();
			delete pDisplay->m_arrEdits[dwIndex];
			pDisplay->m_arrEdits.RemoveAt(pDisplay->m_arrEdits.GetCount() - 1);
		}

		if (pDisplay->m_arrVartypes.GetCount() < pDisplay->m_arrTooltips.GetCount()) {
			pDisplay->m_arrTooltips.RemoveAt(pDisplay->m_arrTooltips.GetCount() - 1);
		}

		if (pDisplay->m_arrVartypes.GetCount() < pDisplay->m_arrLabels.GetCount()) {
			DWORD dwIndex = DWORD(pDisplay->m_arrLabels.GetCount() - 1);
			pDisplay->m_arrLabels[dwIndex]->DestroyWindow();
			delete pDisplay->m_arrLabels[dwIndex];
			pDisplay->m_arrLabels.RemoveAt(pDisplay->m_arrLabels.GetCount() - 1);
		}

		labelRect = m_lastRect;
	}

	return hr;
}

HRESULT CPropertyEditWindow::CreatePropertyLabel(DWORD dwPropertyInfoIndex, CAtlStringW strLabelText, RECT &rectLabel) {
	HRESULT hr = S_OK;

	CStatic* pStatic = new CStatic();
	CHECK_ALLOC(pStatic);
	pStatic->Create(m_hWnd, &rectLabel, strLabelText, WS_CHILD | WS_VISIBLE);
	pStatic->SetFont(m_hLabelFont);
	m_arrPropertyInfoDisplay[dwPropertyInfoIndex]->m_arrLabels.Add(pStatic);

Cleanup:
	return hr;
}

HRESULT CPropertyEditWindow::CreatePropertyEdit(DWORD dwPropertyInfoIndex, CAtlStringW strInitialText, RECT &rectLabel, bool fReadOnly) {
	HRESULT hr = S_OK;

	CEdit* pEdit = new CEdit();
	CHECK_ALLOC(pEdit);

	DWORD dwStyle = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL;
	if (fReadOnly) {
		dwStyle |= ES_READONLY;
	}

	pEdit->Create(m_hWnd, &rectLabel, strInitialText, dwStyle);
	pEdit->SetFont(m_hLabelFont);
	m_arrPropertyInfoDisplay[dwPropertyInfoIndex]->m_arrEdits.Add(pEdit);

Cleanup:
	return hr;
}

HRESULT CPropertyEditWindow::CreatePropertyToolTip(DWORD dwPropertyInfoIndex, HWND hWndParent, CAtlStringW strLabelText, RECT &rectTooltip) {
	HRESULT hr = S_OK;

	CToolTipControl* pToolTip = new CToolTipControl();
	CHECK_ALLOC(pToolTip);

	if (pToolTip->Create(hWndParent, rectTooltip, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, WS_EX_TOPMOST) == NULL) {
		assert(!L"Failed to create tooltip window");
		IFC(E_FAIL);
	}

	pToolTip->SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	pToolTip->SendMessage(TTM_ACTIVATE, TRUE, 0);
	pToolTip->AddTool(hWndParent, strLabelText, rectTooltip, 0);

	m_arrPropertyInfoDisplay[dwPropertyInfoIndex]->m_arrTooltips.Add(pToolTip);
	
Cleanup:
	return hr;
}

void CPropertyEditWindow::ResizeChildren() {
	DWORD cItems = 0;
	RECT clientRect;
	GetClientRect(&clientRect);
	DWORD dwViewWidth = clientRect.right - clientRect.left - 5;

	if (m_arrPropertyInfoDisplay.GetCount() > 0) {
		LockWindowUpdate();
	}

	for (size_t i = 0; i < m_arrPropertyInfoDisplay.GetCount(); i++) {
		// Add an item for the margin;
		if (i != 0) {
			cItems++;
		}

		// Manage the title's position first;
		m_arrTitles[i]->SetWindowPos(HWND_TOP, ms_MarginWidth, ms_MarginWidth + (cItems * ms_LabelHeight), dwViewWidth, ms_LabelHeight, 0);
		cItems++;

		PropertyInfoDisplay* pDisplay = m_arrPropertyInfoDisplay.GetAt(i);
		for (size_t j = 0; j < pDisplay->m_arrLabels.GetCount(); j++) {
			pDisplay->m_arrLabels.GetAt(j)->SetWindowPos(HWND_TOP, ms_MarginWidth, ms_MarginWidth + (cItems * ms_LabelHeight),
														 dwViewWidth / 2, ms_LabelHeight, 0);

			pDisplay->m_arrEdits.GetAt(j)->SetWindowPos(HWND_TOP, dwViewWidth / 2 + ms_MarginWidth,
														ms_MarginWidth + (cItems * ms_LabelHeight), dwViewWidth / 2 - ms_MarginWidth,
														ms_LabelHeight, 0);
			cItems++;
		}

		RECT buttonRect;
		GetClientRect(&buttonRect);
		buttonRect.right -= ms_ButtonWidth + ms_MarginWidth + ms_MarginWidth;
		buttonRect.bottom -= ms_MarginWidth;
		buttonRect.top = buttonRect.bottom - ms_ButtonHeight;
		buttonRect.left = buttonRect.right - ms_ButtonWidth;

		m_AddButton.SetWindowPos(HWND_TOP, buttonRect.left, buttonRect.top,
								 buttonRect.right - buttonRect.left, buttonRect.bottom - buttonRect.top, 0);
		buttonRect.right += ms_ButtonWidth + ms_MarginWidth;
		buttonRect.left += ms_ButtonWidth + ms_MarginWidth;

		m_OKButton.SetWindowPos(HWND_TOP, buttonRect.left, buttonRect.right,
								buttonRect.right - buttonRect.left,
								buttonRect.bottom - buttonRect.top, 0);

		if (m_arrPropertyInfoDisplay.GetCount() > 0) {
			LockWindowUpdate(FALSE);
		}
	}
}