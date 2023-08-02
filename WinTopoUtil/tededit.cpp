#include "stdafx.h"

#include "tededit.h"

#include <dmo.h>
#include <evr.h>
#include <assert.h>

#include "tedvis.h"
#include "topoviewerwindow.h"
#include "tedmemo.h"
#include "dmoinfo.h"
#include "propertyview.h"
#include "mediaobj.h"
#include "Logger.h"

CLSID CLSID_AudioRenderActivate = { /* d23e6476-b104-4707-81cb-e1ca19a07016 */
	0xd23e6476,
	0xb104,
	0x4707,
	{ 0x81, 0xcb, 0xe1, 0xca, 0x19, 0xa0, 0x70, 0x16 }
};

CLSID CLSID_VideoRenderActivate = { /* d23e6477-b104-4707-81cb-e1ca19a07016 */
	0xd23e6477,
	0xb104,
	0x4707,
	{ 0x81, 0xcb, 0xe1, 0xca, 0x19, 0xa0, 0x70, 0x16 }
};

///////////////////////////////////////////////////////////////////////////////
// 
class CMoveComponentHandler : public CCommandHandler {
public:
	CMoveComponentHandler(CVisualTree* pTree, ITedPropertyController* pController);

	BOOL OnLButtonDown(CVisualObject* pObj, CVisualPoint& pt);
	BOOL OnLButtonUp(CVisualObject* pObj, CVisualPoint& pt);
	BOOL OnMouseMove(CVisualObject* pObj, CVisualPoint& pt);
	BOOL OnLButtonDoubleClick(CVisualObject* pObj, CVisualPoint& pt);
	BOOL OnFocus(CVisualObject* pObj);

	void SetEditable(BOOL fEditable);

private:
	HRESULT ShowOTA(IMFTopologyNode* pNode);

	BOOL m_fCapture;
	BOOL m_fEditable;

	// offset from the left/top of component to mouse
	CVisualPoint m_Offset;

	CVisualTree * m_pTree;
	CComPtr<ITedPropertyController> m_spController;
};

///////////////////////////////////////////////////////////////////////////////
// 
class CConnectPinHandler : public CCommandHandler {
public:
	CConnectPinHandler(CVisualTree* pTree, CTedTopologyEditor* pEditor, ITedPropertyController* pController);

	BOOL OnLButtonDown(CVisualObject* pObj, CVisualPoint& pt);
	BOOL OnLButtonUp(CVisualObject* pObj, CVisualPoint& pt);
	BOOL OnMouseMove(CVisualObject* pObj, CVisualPoint& pt);
	BOOL OnLButtonDoubleClick(CVisualObject* pObj, CVisualPoint& pt);
	BOOL OnFocus(CVisualObject* pObj);

	void SetEditable(BOOL fEditable);

protected:
	void RemoveOldConnectors(CVisualPin* pPin, CVisualPin* pOtherPin);
	void ShowMediaTypeProperties(IMFTopologyNode* pOutputNode, DWORD dwOutputPinIndex, IMFTopologyNode* pInputNode, DWORD dwInputPinIndex);
	CComPtr<IMFMediaType> GetNodeMFMediaType(IMFTopologyNode* pNode, DWORD dwPinIndex, bool fOutput);
	bool IsValidConnection(CVisualObject* pSourcePin, CVisualObject* pTargetPin);

private:
	BOOL m_fCapture;
	BOOL m_fEditable;

	CTedTopologyEditor * m_pEditor;
	CVisualTree * m_pTree;
	CComPtr<ITedPropertyController> m_spController;

	CVisualConnector * m_pNew;
	CVisualPin* m_pLastOverPin;
};

///////////////////////////////////////////////////////////////////////////////
//
CMoveComponentHandler::CMoveComponentHandler(CVisualTree* pTree, ITedPropertyController* pController)
	: m_fCapture(FALSE)
	, m_fEditable(TRUE)
	, m_pTree(pTree)
	, m_spController(pController) {

}

BOOL CMoveComponentHandler::OnLButtonDown(CVisualObject* pObj, CVisualPoint& pt) {
	m_Offset = pt;

	if (pObj->GetContainer()) {
		m_Offset.Add(-pObj->GetContainer()->Rect().x(), -pObj->GetContainer()->Rect().y());
	} else {
		m_Offset.Add(-pObj->Rect().x(), -pObj->Rect().y());
	}

	m_fCapture = TRUE;

	return TRUE;
}

BOOL CMoveComponentHandler::OnLButtonUp(CVisualObject* pObj, CVisualPoint& pt) {
	m_fCapture = FALSE;

	return FALSE;
}

BOOL CMoveComponentHandler::OnMouseMove(CVisualObject* pObj, CVisualPoint& pt) {
	if (!m_fCapture) {
		return FALSE;
	}

	if (m_fEditable) {
		if (pObj->GetContainer()) {
			pObj->GetContainer()->Move(pt.x() - m_Offset.x(), pt.y() - m_Offset.y());
		} else {
			pObj->Move(pt.x() - m_Offset.x(), pt.y() - m_Offset.y());
		}

		m_pTree->RouteAllConnectors();
	}

	return TRUE;
}

BOOL CMoveComponentHandler::OnLButtonDoubleClick(CVisualObject * pObj, CVisualPoint & pt) {
	return TRUE;
}

BOOL CMoveComponentHandler::OnFocus(CVisualObject* pObj) {
	HRESULT hr = S_OK;
	CTedTopologyNode* pNode = (CTedTopologyNode*)pObj->GetData();

	if (pNode->GetMFNodeCount() <= pObj->GetIndex()) return FALSE;

	if (m_spController.p) m_spController->ClearProperties();

	IMFTopologyNode* pMFNode = pNode->GetMFNode(pObj->GetIndex());

	MF_TOPOLOGY_TYPE nodeType;
	pMFNode->GetNodeType(&nodeType);

	if (m_spController.p) {
		CNodePropertyInfo* pNodePropertyInfo = new CNodePropertyInfo(pMFNode);
		m_spController->AddPropertyInfo(pNodePropertyInfo);

		// For source nodes, we also add presentation descriptor attributes
		if (nodeType == MF_TOPOLOGY_SOURCESTREAM_NODE) {
			CComPtr<IMFPresentationDescriptor> spPD;
			hr = pMFNode->GetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, IID_IMFPresentationDescriptor, (void**)&spPD);

			if (SUCCEEDED(hr)) {
				CAttributesPropertyInfo* pPDInfo = new CAttributesPropertyInfo(spPD.p, LoadAtlString(IDS_PD_ATTRIBS), TED_ATTRIBUTE_CATEGORY_PRESENTATIONDESCRIPTOR);
				m_spController->AddPropertyInfo(pPDInfo);
			}
		}
	}

	// For transforms and sinks, we also add output trust authority information
	if (nodeType == MF_TOPOLOGY_TRANSFORM_NODE || nodeType == MF_TOPOLOGY_OUTPUT_NODE) {
		IFC(ShowOTA(pMFNode));
	}
Cleanup:
	return TRUE;
}

void CMoveComponentHandler::SetEditable(BOOL fEditable) {
	m_fEditable = fEditable;
}

HRESULT CMoveComponentHandler::ShowOTA(IMFTopologyNode* pNode) {
	HRESULT hr = S_OK;

	CComPtr<IUnknown> spUnkObj;
	CComPtr<IMFTrustedOutput> spTrustedOutput;
	DWORD dwOTACount;

	IFC(pNode->GetObject(&spUnkObj));
	IFC(spUnkObj->QueryInterface(IID_IMFTrustedOutput, (void**)&spTrustedOutput));
	IFC(spTrustedOutput->GetOutputTrustAuthorityCount(&dwOTACount));

	CComPtr<IMFOutputTrustAuthority>* arrOTA = new CComPtr<IMFOutputTrustAuthority>[dwOTACount];
	CHECK_ALLOC(arrOTA);

	for (DWORD i = 0; i < dwOTACount; i++) {
		CComPtr<IMFOutputTrustAuthority> spOTA;

		spTrustedOutput->GetOutputTrustAuthorityByIndex(i, &arrOTA[i]);
	}

	if (m_spController.p) {
		COTAPropertyInfo* pOTAPropertyInfo = new COTAPropertyInfo(arrOTA, dwOTACount);
		if (NULL == pOTAPropertyInfo) {
			delete[] arrOTA;
			goto Cleanup;
		}

		m_spController->AddPropertyInfo(pOTAPropertyInfo);
	}

Cleanup:
	return hr;
}

///////////////////////////////////////////////////////////////////////////////
//
CConnectPinHandler::CConnectPinHandler(CVisualTree * pTree, CTedTopologyEditor* pEditor, ITedPropertyController* pController)
	: m_pTree(pTree)
	, m_fEditable(TRUE)
	, m_pEditor(pEditor)
	, m_spController(pController)
	, m_pNew(NULL)
	, m_pLastOverPin(NULL) {
	assert(m_pEditor != NULL);
	assert(m_pTree != NULL);
}

BOOL CConnectPinHandler::OnLButtonDown(CVisualObject * pObj, CVisualPoint & pt) {
	if (m_fEditable) {
		// Begin a connection
		m_fCapture = TRUE;

		CVisualPin* pPin = (CVisualPin*)pObj;

		m_pNew = new CVisualConnector;

		m_pTree->AddVisual(m_pNew);

		m_pNew->Left() = pPin->GetConnectorPoint();
		m_pNew->Right() = pPin->GetConnectorPoint();
	}

	return TRUE;
}

BOOL CConnectPinHandler::OnLButtonUp(CVisualObject* pObj, CVisualPoint& pt) {
	CVisualObject * pHitObj;

	assert(pObj != NULL);

	if (!m_fCapture || !m_fEditable) {
		return FALSE;
	}

	CVisualPin * pPin = (CVisualPin*)pObj;

	if (!m_pTree->HitTest(pt, &pHitObj)) {
		goto Cleanup;
	}

	if (!IsValidConnection(pObj, pHitObj)) {
		goto Cleanup;
	}

	CVisualPin* pOtherPin = (CVisualPin*)pHitObj;

	CVisualObject::CONNECTION_TYPE connType = pOtherPin->GetConnectionType();
	if (CVisualObject::INPUT == connType) {
		m_pNew->Right() = pOtherPin->GetConnectorPoint();
	} else {
		m_pNew->Left() = pOtherPin->GetConnectorPoint();

		// pPin needs to be the input pin
		CVisualPin* temp = pPin;
		pPin = pOtherPin;
		pOtherPin = temp;
	}

	assert(pPin != pOtherPin);

	CTedTopologyNode* outputterNode = (CTedTopologyNode*)(pPin->GetData());
	CTedTopologyNode* acceptorNode = (CTedTopologyNode*)(pOtherPin->GetData());

	m_pEditor->FullConnectNodes(outputterNode, pPin->GetPinId(), acceptorNode, pOtherPin->GetPinId());

Cleanup:

	if (NULL != m_pNew) {
		m_pTree->RemoveVisual(m_pNew);
		m_pNew = NULL;
	}

	if (m_pLastOverPin) {
		m_pLastOverPin->Highlight(false);
		m_pLastOverPin = NULL;
	}

	return FALSE;
}

BOOL CConnectPinHandler::OnMouseMove(CVisualObject * pObj, CVisualPoint & pt) {
	if (!m_fCapture || !m_fEditable) {
		return FALSE;
	}

	assert(m_pNew);

	CVisualPin* pPin = (CVisualPin*)pObj;

	if (pPin->GetConnectionType() == CVisualObject::INPUT) {
		m_pNew->Left() = pt;
	} else {
		m_pNew->Right() = pt;
	}

	CVisualObject* pOverObject;

	if (m_pLastOverPin) {
		m_pLastOverPin->Highlight(false);
		m_pLastOverPin = NULL;
	}

	if (m_pTree->HitTest(pt, &pOverObject)) {
		if (pOverObject->GetConnectionType() != CVisualObject::NONE && IsValidConnection(pPin, (CVisualPin*)pOverObject)) {
			CVisualPin* pOverPin = (CVisualPin*)pOverObject;

			pOverPin->Highlight(true);

			m_pLastOverPin = pOverPin;
		}
	}

	return TRUE;
}

BOOL CConnectPinHandler::OnLButtonDoubleClick(CVisualObject* pObj, CVisualPoint& pt) {
	return TRUE;
}

BOOL CConnectPinHandler::OnFocus(CVisualObject* pObj) {
	CVisualObject::CONNECTION_TYPE connType;
	connType = pObj->GetConnectionType();

	if (m_spController.p) {
		m_spController->ClearProperties();
	}

	// If we have a connection, we want to display the media types for it
	if (connType != CVisualObject::NONE) {
		CVisualPin* pPin = (CVisualPin*)pObj;

		CTedTopologyNode* pNode = (CTedTopologyNode*)(pPin->GetData());

		// For source streams, we also want to display stream descriptor attributes
		if (pNode->GetType() == CTedTopologyNode::TED_SOURCE_NODE && m_spController.p) {
			IMFTopologyNode* pMFNode = pNode->GetMFNode(pPin->GetPinId());

			CComPtr<IMFStreamDescriptor> spSD;
			pMFNode->GetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, IID_IMFStreamDescriptor, (void**)&spSD);

			CComPtr<IMFAttributes> spAttr = spSD.p;

			CAttributesPropertyInfo* pAttrInfo = new CAttributesPropertyInfo(spAttr, LoadAtlString(IDS_SD_ATTRIBS), TED_ATTRIBUTE_CATEGORY_STREAMDESCRIPTOR);
			if (m_spController.p) m_spController->AddPropertyInfo(pAttrInfo);
		}

		if (pPin->GetConnector() == NULL) {
			return true;
		}

		CTedTopologyConnection* pConn = NULL;
		CTedTopologyNode* pOtherNode = NULL;

		if (connType == CVisualObject::INPUT) {
			pConn = m_pEditor->FindUpstreamConnection(pNode->GetID(), pPin->GetPinId());
			assert(pConn);
			pOtherNode = m_pEditor->FindNode(pConn->GetOutputNodeID());
			assert(pOtherNode);

			CTedTopologyPin outputPin;
			CTedTopologyPin inputPin;

			pNode->GetPin(pConn->GetInputPinID(), inputPin, true);
			pOtherNode->GetPin(pConn->GetOutputPinID(), outputPin, false);

			IMFTopologyNode* pMFInputNode = inputPin.PNode();
			IMFTopologyNode* pMFOutputNode = outputPin.PNode();

			ShowMediaTypeProperties(pMFOutputNode, outputPin.Index(), pMFInputNode, inputPin.Index());
		} else {
			pConn = m_pEditor->FindDownstreamConnection(pNode->GetID(), pPin->GetPinId());
			assert(pConn);
			pOtherNode = m_pEditor->FindNode(pConn->GetInputNodeID());
			assert(pOtherNode);

			CTedTopologyPin outputPin;
			CTedTopologyPin inputPin;

			pOtherNode->GetPin(pConn->GetInputPinID(), inputPin, true);
			pNode->GetPin(pConn->GetOutputPinID(), outputPin, false);

			IMFTopologyNode* pMFInputNode = inputPin.PNode();
			IMFTopologyNode* pMFOutputNode = outputPin.PNode();

			ShowMediaTypeProperties(pMFOutputNode, outputPin.Index(), pMFInputNode, inputPin.Index());
		}
	}

	return TRUE;
}

void CConnectPinHandler::ShowMediaTypeProperties(IMFTopologyNode* pOutputNode, DWORD dwOutputPinIndex, IMFTopologyNode* pInputNode, DWORD dwInputPinIndex) {
	if (m_spController) {
		HRESULT hr;
		CComPtr<IMFMediaType> spOutputType = GetNodeMFMediaType(pOutputNode, dwOutputPinIndex, true);
		CComPtr<IMFMediaType> spInputType = GetNodeMFMediaType(pInputNode, dwInputPinIndex, false);

		if (spOutputType.p) {
			CComPtr<IMFAttributes> spOutAttr;
			hr = spOutputType->QueryInterface(IID_IMFAttributes, (void**)&spOutAttr);
			if (SUCCEEDED(hr)) {
				CAttributesPropertyInfo* pOutInfo = new CAttributesPropertyInfo(spOutAttr, LoadAtlString(IDS_MT_UPSTREAM), TED_ATTRIBUTE_CATEGORY_MEDIATYPE);
				if (pOutInfo) m_spController->AddPropertyInfo(pOutInfo);
			}
		}

		if (spInputType.p) {
			CComPtr<IMFAttributes> spInAttr;
			hr = spInputType->QueryInterface(IID_IMFAttributes, (void**)&spInAttr);
			if (SUCCEEDED(hr)) {
				CAttributesPropertyInfo* pInInfo = new CAttributesPropertyInfo(spInAttr, LoadAtlString(IDS_MT_DOWNSTREAM), TED_ATTRIBUTE_CATEGORY_MEDIATYPE);
				if (pInInfo) m_spController->AddPropertyInfo(pInInfo);
			}
		}
	}
}

CComPtr<IMFMediaType> CConnectPinHandler::GetNodeMFMediaType(IMFTopologyNode* pNode, DWORD dwPinIndex, bool fOutput) {
	MF_TOPOLOGY_TYPE nodeType;
	CComPtr<IMFMediaType> spType;

	pNode->GetNodeType(&nodeType);

	if (nodeType == MF_TOPOLOGY_SOURCESTREAM_NODE) {
		CComPtr<IMFStreamDescriptor> spSD;
		CComPtr<IMFMediaTypeHandler> spMTH;

		pNode->GetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, IID_IMFStreamDescriptor, (void**)&spSD);
		spSD->GetMediaTypeHandler(&spMTH);

		spMTH->GetCurrentMediaType(&spType);
	} else if (nodeType == MF_TOPOLOGY_TRANSFORM_NODE) {
		CComPtr<IUnknown> spTransformUnk;
		CComPtr<IMFTransform> spTransform;

		pNode->GetObject(&spTransformUnk);
		spTransformUnk->QueryInterface(IID_IMFTransform, (void**)&spTransform);

		if (fOutput) {
			spTransform->GetOutputCurrentType(dwPinIndex, &spType);
		} else {
			spTransform->GetInputCurrentType(dwPinIndex, &spType);
		}
	} else if (nodeType == MF_TOPOLOGY_OUTPUT_NODE) {
		HRESULT hr;
		CComPtr<IUnknown> spUnknown;
		CComPtr<IMFStreamSink> spStreamSink;

		pNode->GetObject(&spUnknown);

		hr = spUnknown->QueryInterface(IID_IMFStreamSink, (void**)&spStreamSink);

		if (SUCCEEDED(hr)) {
			CComPtr<IMFMediaTypeHandler> spMTH;

			spStreamSink->GetMediaTypeHandler(&spMTH);

			spMTH->GetCurrentMediaType(&spType);
		}

		if (NULL == spType.p) {
			pNode->GetInputPrefType(0, &spType);
		}
	}

	return spType;
}

bool CConnectPinHandler::IsValidConnection(CVisualObject* pSource, CVisualObject* pTarget) {
	CVisualObject::CONNECTION_TYPE connType = pSource->GetConnectionType();
	CVisualObject::CONNECTION_TYPE targetConnType = pTarget->GetConnectionType();

	if (CVisualObject::NONE == connType ||			/* don't process objects that cannot be connected */
		CVisualObject::NONE == targetConnType || 	/* ditto */
		targetConnType == connType || 				/* Don't connect two inputs or two outputs together */
		pSource == pTarget) {						/* Don't connect an object to itself */

		return false;
	}

	return true;
}

void CConnectPinHandler::SetEditable(BOOL fEditable) {
	m_fEditable = fEditable;
}

///////////////////////////////////////////////////////////////////////////////
//
CTedTopologyPin::CTedTopologyPin()
	: m_pNode(NULL)
	, m_nIndex(0) {

}

CTedTopologyPin::CTedTopologyPin(IMFTopologyNode * pNode, DWORD nIndex)
	: m_pNode(pNode)
	, m_nIndex(nIndex) {

}

CTedTopologyPin::~CTedTopologyPin() {

}

///////////////////////////////////////////////////////////////////////////////
//
CTedTopologyConnection::CTedTopologyConnection(int nOutputNodeID, int nOutputPinID, int nInputNodeID, int nInputPinID)
	: m_nOutputNodeID(nOutputNodeID), m_nOutputPinID(nOutputPinID), m_nInputNodeID(nInputNodeID), m_nInputPinID(nInputPinID) {
	assert(m_nOutputNodeID >= 0);
	assert(m_nOutputPinID >= 0);
	assert(m_nInputNodeID >= 0);
	assert(m_nInputPinID >= 0);
}

CTedTopologyConnection::CTedTopologyConnection(CTedConnectionMemo* pMemo)
	: m_nOutputNodeID(pMemo->m_nOutputNodeID)
	, m_nOutputPinID(pMemo->m_nOutputPinID)
	, m_nInputNodeID(pMemo->m_nInputNodeID)
	, m_nInputPinID(pMemo->m_nInputPinID) {

}

int CTedTopologyConnection::GetOutputNodeID() const {
	return m_nOutputNodeID;
}

int CTedTopologyConnection::GetInputNodeID() const {
	return m_nInputNodeID;
}

int CTedTopologyConnection::GetOutputPinID() const {
	return m_nOutputPinID;
}

int CTedTopologyConnection::GetInputPinID() const {
	return m_nInputPinID;
}

CTedConnectionMemo* CTedTopologyConnection::CreateMemo() const {
	return new CTedConnectionMemo(m_nOutputNodeID, m_nOutputPinID, m_nInputNodeID, m_nInputPinID);
}

///////////////////////////////////////////////////////////////////////////////
// CTedTopologyNode

// Initialization //

int CTedTopologyNode::ms_nNextID = 0;

CTedTopologyNode::CTedTopologyNode() {
	m_nID = ms_nNextID++;
}

CTedTopologyNode::~CTedTopologyNode() {

}

HRESULT CTedTopologyNode::Init(const CAtlStringW& label, bool fAutoInserted) {
	HRESULT hr = S_OK;

	m_pVisual = new CVisualNode(label, fAutoInserted);
	CHECK_ALLOC(m_pVisual);

	m_pVisual->Move(20, 20);
	m_pVisual->SetData((LONG_PTR)this);

	m_strLabel = label;

Cleanup:
	return hr;
}

HRESULT CTedTopologyNode::InitContainer(const CAtlStringW& label, bool fAutoinserted) {
	HRESULT hr = S_OK;

	m_pVisual = new CVisualContainer(label);
	CHECK_ALLOC(m_pVisual);

	m_pVisual->Move(20, 20);
	m_pVisual->SetData((LONG_PTR)this);

	m_strLabel = label;

Cleanup:
	return hr;
}

void CTedTopologyNode::Init(CTedNodeMemo* pMemo) {
	Init(pMemo->m_strLabel);

	m_nID = pMemo->m_nID;

	if (ms_nNextID <= m_nID) {
		ms_nNextID = m_nID + 1;
	}

	m_pVisual->Move(pMemo->m_x, pMemo->m_y);
}

void CTedTopologyNode::InitContainer(CTedNodeMemo* pMemo) {
	InitContainer(pMemo->m_strLabel);

	m_nID = pMemo->m_nID;

	if (ms_nNextID <= m_nID) {
		ms_nNextID = m_nID + 1;
	}

	m_pVisual->Move(pMemo->m_x, pMemo->m_y);
}

/* Accessors */
HWND CTedTopologyNode::GetVideoWindow() const {
	return NULL;
}

CVisualComponent* CTedTopologyNode::GetVisual() const {
	return m_pVisual;
}

CVisualPin* CTedTopologyNode::GetVisualInputPin(int pinID) {
	return ((CVisualComponent*)m_pVisual)->GetInputPin(pinID);
}

CVisualPin* CTedTopologyNode::GetVisualOutputPin(int pinID) {
	return ((CVisualComponent*)m_pVisual)->GetOutputPin(pinID);
}

int CTedTopologyNode::GetID() const {
	return m_nID;
}

HRESULT CTedTopologyNode::GetNodeID(DWORD dwIndex, TOPOID& NodeID) {
	HRESULT hr = S_OK;

	CComPtr<IMFTopologyNode> spNode = GetMFNode(dwIndex);

	IFC(spNode->GetTopoNodeID(&NodeID));

Cleanup:
	return hr;
}

CAtlStringW CTedTopologyNode::GetLabel() const {
	return m_strLabel;
}