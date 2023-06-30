#pragma once

#include "stdafx.h"

class CTedInputGuidDialog : public CDialogImpl<CTedInputGuidDialog> {
public:
	enum {
		IDD = IDD_INPUTGUID
	};
	
	GUID GetInputGuid() const;
	bool IsValidGuid() const {
		return m_fIsValidGuid;
	}

protected:
	LRESULT OnOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
	LRESULT OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);

	BEGIN_MSG_MAP(CTedInputGuidDialog)
		COMMAND_HANDLER(IDOK, 0, OnOK)
		COMMAND_HANDLER(IDCANCEL, 0, OnCancel)
	END_MSG_MAP()

private:
	GUID m_InputGUID;
	bool m_fIsValidGuid;
	static const DWORD m_dwGUIDLength = 40;

};

