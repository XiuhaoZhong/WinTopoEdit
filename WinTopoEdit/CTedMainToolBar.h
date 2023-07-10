#pragma once
#include "stdafx.h"

#include "tedobj.h"
#include "CTrackbarControl.h"

class CTedMainToolBar : public CWindowImpl<CTedMainToolBar> {
public:
	CTedMainToolBar();
	~CTedMainToolBar();

	HRESULT Init(HWND parentWnd, _U_MENUorID id);

	CTrackbarControl* GetSeekBar();
	CTrackbarControl* GetRateBar();

	void ShowRateBar(int nCmdShow);
	void EnableButton(int nID, BOOL fEnale);
	void EnableButtonByCommand(UINT nID, BOOL fEnable);
	void SetTrackbarScrollCallback(HANDLESCROLLPROC scrollCallback);
	void SetTrackbarRange(int minValue, int maxValue);
	void SetTimeLabel(const CAtlStringW &strLabel);
	void UpdateTimeDisplay(MFTIME time, MFTIME duration);
	void UpdateRateDisplay(float flRate);

	void MarkResolved(bool fResolved = true);

	static const int PLAY_BUTTON;
	static const int STOP_BUTTON;
	static const int PAUSE_BUTTON;

	DECLARE_WND_SUPERCLASS(NULL, TOOLBARCLASSNAME)

protected:
	LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
	virtual LRESULT OnVScroll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
	virtual LRESULT OnHScorll(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);

	BEGIN_MSG_MAP(CTedMainToolBar)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_VSCROLL, OnVScroll)
		MESSAGE_HANDLER(WM_HSCROLL, OnHScorll)
	END_MSG_MAP()

private:
	TBBUTTON m_buttons[3];
	CSeekerTrackbarControl m_trackBar;
	CStatic m_timeLabel;
	UINT_PTR nTimerID;
	DWORD dwTimerInterval;
	HFONT m_hStaticFont;

	CStatic m_rateLabel;
	CTrackbarControl m_rateBar;

	CStatic m_resolvedLabel;

	static const int m_iButtonCount = 3;
	static const int m_iMarginSize = 5;
	static const int m_iToolbarButtonWidth = 75;
	static const int m_iTimeLabelWidth = 75;
	static const int m_iTrackbarWidth = 100;
	static const int m_iRateLabelWidth = 65;
	static const int m_iRateBarWidth = 50;
	static const int m_iResolvedLabelWidth = 175;
	static const int m_iSecondInMinute = 60;
};

