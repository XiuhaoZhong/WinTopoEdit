#pragma once

#include "resource.h"

struct DMOCategory {
	GUID m_GUID;
	UINT m_nID;
};

class CTedTransformDialog : public CDialogImpl<CTedTransformDialog> {
public:
	~CTedTransformDialog();

	enum {
		IDD = IDD_TRANSFORM
	};

	IMFActivate *GetChosenActivate() const;
	CAtlStringW GetChosenName() const;

protected:
	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
	LRESULT OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
	LRESULT OnOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
	LRESULT OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);

	void PopulateCategory(DMOCategory &category);

	BEGIN_MSG_MAP(CTedTransformDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)

		COMMAND_HANDLER(IDOK, 0, OnOK)
		COMMAND_HANDLER(IDCANCEL, 0, OnCancel)
	END_MSG_MAP()

private:
	HWND m_hList;
	CAtlArray<IMFActivate *> m_Activates;
	CAtlArray<CAtlStringW> m_strNames;
	unsigned int m_nChosenIndex;
	static DMOCategory ms_categories[9];
};
