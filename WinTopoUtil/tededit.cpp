#include "stdafx.h"

#include "tedvis.h"

#include <assert.h>
#include <math.h>
#include <CommCtrl.h>

#include "tededit.h"

///////////////////////////////////////////////////////////////////////////////
//

BOOL CVisualRect::IsIn(CVisualRect& rect) {
	return !(x() > rect.right()
			 || right() < rect.x()
			 || y() > rect.bottom()
			 || bottom() < rect.y());
}

///////////////////////////////////////////////////////////////////////////////
//
CVisualCoordinateTransform::CVisualCoordinateTransform()
	: m_xScale(1)
	, m_yScale(1)
	, m_xOffset(0)
	, m_yOffset(0) {

}

POINT CVisualCoordinateTransform::VisualToScreen(CVisualPoint & vis) {
	POINT pt;

	pt.x = LONG((vis.x() + m_xOffset) * m_xScale);
	pt.y = LONG((vis.y() + m_yOffset) * m_yScale);

	return pt;
}

RECT CVisualCoordinateTransform::VisualToScreen(CVisualRect & vis) {
	RECT rc;

	rc.left = LONG((vis.x() + m_xOffset) * m_xScale);
	rc.top = LONG((vis.y() + m_yOffset) * m_yScale);
	rc.right = LONG((vis.right() + m_xOffset) * m_xScale);
	rc.bottom = LONG((vis.bottom() + m_yOffset) * m_yScale);

	return rc;
}

CVisualPoint CVisualCoordinateTransform::ScreenToVisual(POINT & pt) {
	CVisualPoint vis(pt.x / m_xScale - m_xOffset, pt.y / m_yScale - m_yOffset);

	return vis;
}

void CVisualCoordinateTransform::SetPointOffset(double xOffset, double yOffset) {
	m_xOffset = xOffset;
	m_yOffset = yOffset;
}

///////////////////////////////////////////////////////////////////////////////
//
CVisualDrawContext::CVisualDrawContext(CVisualCoordinateTransform * pTransform)
	: m_pTransform(pTransform) {

}

CVisualDrawContext::~CVisualDrawContext() {

}

BOOL CVisualDrawContext::BeginPaint(HWND hWnd) {
	m_hWnd = hWnd;
	m_hdc = ::BeginPaint(hWnd, &m_ps);

	RECT rect;
	::GetClientRect(m_hWnd, &rect);

	m_hBgBuffer = ::CreateCompatibleDC(m_hdc);
	if (NULL == m_hBgBuffer) {
		return FALSE;
	}

	HBITMAP hBitmap = ::CreateCompatibleBitmap(m_hdc, rect.right - rect.left, rect.bottom - rect.top);
	if (NULL == hBitmap) {
		DeleteObject(m_hBgBuffer);
		return FALSE;
	}

	m_hOldBitmap = (HBITMAP)SelectObject(m_hBgBuffer, hBitmap);
	if (NULL == m_hOldBitmap) {
		DeleteObject(hBitmap);
		DeleteObject(m_hBgBuffer);
		return FALSE;
	}

	HBRUSH hWhiteBrush = ::CreateSolidBrush(RGB(255, 255, 255));
	if (NULL == hWhiteBrush) {
		SelectObject(m_hBgBuffer, m_hOldBitmap);
		DeleteObject(hBitmap);
		DeleteObject(m_hBgBuffer);
		return FALSE;
	}

	::FillRect(m_hBgBuffer, &rect, hWhiteBrush);
	DeleteObject(hWhiteBrush);

	return TRUE;
}

void CVisualDrawContext::EndPaint() {
	RECT rect;
	::GetClientRect(m_hWnd, &rect);

	::BitBlt(m_hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, m_hBgBuffer, 0, 0, SRCCOPY);
	::EndPaint(m_hWnd, &m_ps);

	HBITMAP hBitmap = (HBITMAP)SelectObject(m_hBgBuffer, m_hOldBitmap);
	DeleteObject(hBitmap);
	DeleteDC(m_hBgBuffer);
}

void CVisualDrawContext::PushState() {
	m_StateStack.AddHead(m_State);

	// cleanup brush. hdc should have selected brush. 
	// when we pop state and m_hOldBrush is not NULL - select old brush
	m_State.m_hNewBrush = NULL;
	m_State.m_hOldBrush = NULL;
	m_State.m_hNewPen = NULL;
	m_State.m_hOldPen = NULL;
}

void CVisualDrawContext::PopState() {
	if (m_State.m_hOldBrush) {
		SelectObject(m_hBgBuffer, m_State.m_hOldBrush);
	}

	if (m_State.m_hNewBrush) {
		DeleteObject(m_State.m_hNewBrush);
	}

	if (m_State.m_hOldPen) {
		SelectObject(m_hBgBuffer, m_State.m_hOldPen);
	}

	if (m_State.m_hNewPen) {
		DeleteObject(m_State.m_hNewPen);
	}

	m_State = m_StateStack.RemoveHead();
}

BOOL CVisualDrawContext::SelectSmallFont() {
	CAtlString strFontSize = LoadAtlString(IDS_FONT_SIZE_12);
	CAtlString strFontFace = LoadAtlString(IDS_FONT_FACE_ARIAL);

	LOGFONT lfLabelFont;
	lfLabelFont.lfHeight = _wtol(strFontSize);
	lfLabelFont.lfWidth = 0;
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

	HFONT hSmallFont = CreateFontIndirect(&lfLabelFont);
	if (NULL == hSmallFont) {
		return FALSE;
	}

	m_State.m_hOldFont = SelectObject(m_hBgBuffer, hSmallFont);
	if (NULL == m_State.m_hOldFont) {
		DeleteObject(hSmallFont);
		return FALSE;
	}

	return true;
}

void CVisualDrawContext::DeselectSmallFont() {
	HGDIOBJ hFont = SelectObject(m_hBgBuffer, m_State.m_hOldFont);
	DeleteObject(hFont);
}

BOOL CVisualDrawContext::SelectPen(COLORREF color, int width) {
	HPEN hOldPen;

	HPEN hNewPen = CreatePen(PS_SOLID, width, color);
	if (NULL == hNewPen) {
		return FALSE;
	}

	hOldPen = (HPEN)SelectObject(m_hBgBuffer, hNewPen);
	if (NULL == hOldPen) {
		DeleteObject(hNewPen);
		return FALSE;
	}

	// if we already have old - keep previous
	if (!m_State.m_hOldPen) {
		m_State.m_hOldPen = hOldPen;
	}

	if (m_State.m_hNewPen) {
		DeleteObject(m_State.m_hNewPen);
	}

	m_State.m_hNewPen = hNewPen;

	return TRUE;
}

BOOL CVisualDrawContext::SelectSolidBrush(COLORREF color) {
	HBRUSH hOldBrush;

	HBRUSH hNewBrush = CreateSolidBrush(color);
	if (NULL == hNewBrush) {
		return FALSE;
	}

	hOldBrush = (HBRUSH)SelectObject(m_hBgBuffer, hNewBrush);
	if (NULL == hOldBrush) {
		DeleteObject(hNewBrush);
		return FALSE;
	}

	// if we already have old - keep previous
	if (!m_State.m_hOldBrush) {
		m_State.m_hOldBrush = hOldBrush;
	}

	if (m_State.m_hNewBrush) {
		DeleteObject(m_State.m_hNewBrush);
	}

	m_State.m_hNewBrush = hNewBrush;

	return TRUE;
}

void CVisualDrawContext::MapPoint(CVisualPoint & vis, POINT & disp) {
	CVisualPoint v(vis);

	v.Add(m_State.m_xCoord, m_State.m_yCoord);
	disp = m_pTransform->VisualToScreen(v);
}

void CVisualDrawContext::MapRect(CVisualRect & vis, RECT & disp) {
	CVisualRect v(vis);

	v.Add(m_State.m_xCoord, m_State.m_yCoord);
	disp = m_pTransform->VisualToScreen(v);
}

void CVisualDrawContext::ShiftCoordinates(double xOffset, double yOffset) {
	m_State.m_xCoord += xOffset;
	m_State.m_yCoord += yOffset;
}

///////////////////////////////////////////////////////////////////////////////
//

void CVisualObject::Select(bool bIsSelected) {
	m_bIsSelected = bIsSelected;
}

bool CVisualObject::ContainsVisual(CVisualObject* pVisual) {
	if (this == pVisual) {
		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
//

// draw connector
void CVisualConnector::Draw(CVisualDrawContext & Ctx) {
	// set color
	if (IsSelected()) {
		Ctx.SelectPen(m_clrSelected, 2);
	} else {
		Ctx.SelectPen(m_clrLine, 1);
	}

	POINT left, right;
	Ctx.MapPoint(m_Left, left);
	Ctx.MapPoint(m_Right, right);

	MoveToEx(Ctx.DC(), left.x, left.y, NULL);
	LineTo(Ctx.DC(), right.x, right.y);
}

CVisualObject::CONNECTION_TYPE CVisualConnector::GetConnectionType() const {
	return CVisualObject::NONE;
}

void CVisualConnector::NotifyRemoved(CVisualObject* removed) {

}

bool CVisualConnector::IsDependent(CVisualObject* pOtherObj) const {
	return false;
}

BOOL CVisualConnector::HitTest(CVisualPoint & pt, CVisualObject ** ppObj) {
	double alpha = m_Right.x() - m_Left.x();
	double beta = m_Right.y() - m_Left.y();

	double t1 = (pt.x() - m_Left.x()) / alpha;
	double t2 = (pt.y() - m_Left.y()) / beta;

	if (fabs(t1 - t2) < .1 && t1 >= 0 && t1 <= 1) {
		*ppObj = this;
		return true;
	}

	return false;
}