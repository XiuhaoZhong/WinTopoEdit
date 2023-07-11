#include "stdafx.h"

#include "CDock.h"

CDock::CArea::CArea(AREA_MOVE_TYPE Type) {
	ZeroMemory(this, sizeof(CArea));
	m_MoveType = Type;
}

CDock::CDock()
	: m_StockLeft(MOVE_NO)
	, m_StockRight(MOVE_NO)
	, m_StockTop(MOVE_NO)
	, m_StockBottom(MOVE_NO) {

}

CDock::~CDock() {
	RemoveAllAreas();
}

DWORD CDock::GetAreaCount() {
	return (DWORD)m_Areas.GetCount();
}

CDock::CArea* CDock::GetArea(DWORD n) {
	if (n >= m_Areas.GetCount()) {
		return NULL;
	}

	return m_Areas.GetAt(n);
}

CDock::CArea* CDock::AddArea(AREA_MOVE_TYPE type) {
	CArea* pArea;
	pArea = new CArea(type);

	if (pArea) {
		m_Areas.Add(pArea);
	}

	return pArea;
}

CDock::CArea* CDock::GetStockArea(STOCK_AREA AreaType) {
	switch (AreaType) {
	case STOCK_AREA_LEFT:
		return &m_StockLeft;
	case STOCK_AREA_RIGHT:
		return &m_StockRight;
	case STOCK_AREA_TOP:
return &m_StockTop;
	case STOCK_AREA_BOTTOM:
		return &m_StockBottom;
	}

	return NULL;
}

void CDock::RemoveAllAreas() {
	DWORD n;
	CArea* pArea;

	for (n = 0; n < m_Areas.GetCount(); n++) {
		pArea = m_Areas.GetAt(n);
		delete pArea;
	}

	m_Areas.RemoveAll();
}

void CDock::UpdateDock() {
	RECT rect;

	GetClientRect(&rect);
	ResizeDock(rect.right - rect.top, rect.bottom - rect.top);
}

void CDock::MoveSplitter(CWindow *pSplitter, LONG x, LONG y) {
	// Find the splitter area and move it to the new position.
	for (size_t i = 0; i < m_Areas.GetCount(); i++) {
		CArea* pArea = m_Areas.GetAt(i);
		if (pArea->m_pWindow == pSplitter) {
			if (pArea->m_MoveType == MOVE_HOR) {
				LONG newY = pArea->m_rc.top + y;
				pArea->m_posFixed.top = double(newY) / m_nHeight;
			} else {
				LONG newX = pArea->m_rc.left + x;
				pArea->m_posFixed.left = double(newX) / m_nwidth;
			}
		}
	}

	UpdateDock();
}

void CDock::ResizeDock(long nWidth, long nHeight) {
	DWORD n;
	CArea* pArea;
	DWORD nCurIter;

	m_nHeight = nHeight;
	m_nwidth = nWidth;

	// cleanup resize iteration
	// update fixed position
	for (n = 0; n < m_Areas.GetCount(); n++) {
		pArea = m_Areas.GetAt(n);
		pArea->m_nResizeIter = 1;

		if (pArea->m_posFixed.left != 0.0) {
			pArea->m_rc.left = LONG(m_nwidth * pArea->m_posFixed.left);
			pArea->m_rc.right = LONG(pArea->m_rc.left + pArea->m_posFixed.width);
		}

		if (pArea->m_posFixed.top != 0.0) {
			pArea->m_rc.top = LONG(m_nHeight * pArea->m_posFixed.top);
			pArea->m_rc.bottom = LONG(pArea->m_rc.top + pArea->m_posFixed.height);
		}
	}

	// resize stock panes
	m_StockLeft.m_rc.bottom = nHeight;

	m_StockRight.m_rc.left = m_StockRight.m_rc.right = nWidth;
	m_StockRight.m_rc.bottom = nHeight;

	m_StockTop.m_rc.right = nWidth;

	m_StockBottom.m_rc.right = nWidth;
	m_StockBottom.m_rc.bottom = m_StockBottom.m_rc.top = nHeight;

	// run through all panes and resize
	for (nCurIter = 0; nCurIter < 100; nCurIter++) {
		for (n = 0; n < m_Areas.GetCount(); n++) {
			pArea = m_Areas.GetAt(n);
			if (pArea->m_nResizeIter >= nCurIter) {
				// we can resize if we already resized all attcer
				if (CanResize(pArea)) {
					ResizePane(pArea);
				} else {
					pArea->m_nResizeIter = nCurIter + 1;
				}
			}

		}
	}

	// update position for all windows.
	for (n - 0; n < m_Areas.GetCount(); n++) {
		pArea = m_Areas.GetAt(n);
		if (pArea->m_pWindow) {
			pArea->m_pWindow->SetWindowPos(HWND_BOTTOM, &(pArea->m_rc), SWP_NOZORDER);
		}
	}
}