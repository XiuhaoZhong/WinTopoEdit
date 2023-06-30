#pragma once

#include "CDock.h"

class CSplitterBar : public CWindowImpl<CSplitterBar> {
public:
	CSplitterBar(CDock *pDock, bool vert, HWND parent);

	static CWndClassInfo& GetWndClassInfo() {
		static CWndClassInfo hrozWC = {
			{
				sizeof(WNDCLASSEX),
				CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
				StartWindowProc,
				0,0,
				NULL,
				NULL,
				NULL,
				(HBRUSH)(COLOR_WINDOW + 1),
				NULL,
				L"HorzSplitter",
				NULL
			},
			NULL,
			NULL,
			MAKEINTRESOURCE(IDC_SIZENS),
			TRUE,
			0,_T("")
		};

		static CWndClassInfo vertWC = {
		{
			sizeof(WNDCLASSEX),
			CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
			StartWindowProc,
			0,
			0,
			NULL,
			NULL,
			NULL,
			(HBRUSH)(COLOR_WINDOW + 1),
			NULL,
			L"VertSplitter",
			NULL
		},
		NULL,
		NULL,
		MAKEINTRESOURCE(IDC_SIZE),
		TRUE,
		0,
		_T("")
		};

		if (ms_fVert) {
			return vertWC;
		} else {
			return hrozWC;
		}
	}

protected:
	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
	LRESULT OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
	LRESULT OnMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
	LRESULT OnLButtonDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
	LRESULT OnLButtonUp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);

	BEGIN_MSG_MAP(CSplitterBar)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
	END_MSG_MAP()

private:
	static LPCTSTR ms_strNextCursor;
	CDock *m_pDock;
	bool m_bSelected;
	bool m_fVert;
	HWND m_hParent;

	static bool ms_fVert;
};

