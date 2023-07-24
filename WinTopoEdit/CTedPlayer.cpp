#include "stdafx.h"

#include <mferror.h>
#include <initguid.h>

#include "CTedPlayer.h"

EXTERN_GUID(CLSID_CResamplerMediaObject, 0xf447b69e, 0x1884, 0x4a7e, 0x80, 0x55, 0x34, 0x6f, 0x74, 0xd6, 0xed, 0xb3);

CTedMediaFileRenderer::CTedMediaFileRenderer(LPCWSTR szFileName, ITedVideoWindowHandler *pVideoWindowHandler, HRESULT &hr)
		: m_szFileName(szFileName) 
		, m_spVideoWindowHandler(pVideoWindowHandler) {
	hr = MFCreateTopoLoader(&m_spTopoLoader);
}

HRESULT CTedMediaFileRenderer::Load(IMFTopology **ppOutputTopology) {
	HRESULT hr;

	CComPtr<IMFTopology> spPartialTopology;

	IFC(CreatePartialTopology(&spPartialTopology));
	IFC(m_spTopoLoader->Load(spPartialTopology, ppOutputTopology, NULL));

Cleanup:
	return hr;
}

HRESULT CTedMediaFileRenderer::CreatePartialTopology(IMFTopology **ppPartialTopology) {
	HRESULT hr;

	IMFTopology* pPartialTopology;
	CComPtr<IMFMediaSource> spSource;

	IFC(MFCreateTopology(&pPartialTopology));
	IFC(CreateSource(&spSource));
	IFC(BuildTopologyFromSource(pPartialTopology, spSource));

	*ppPartialTopology = pPartialTopology;

Cleanup:
	return hr;
}

HRESULT CTedMediaFileRenderer::CreateSource(IMFMediaSource **ppSource) {
	HRESULT hr;

	CComPtr<IMFSourceResolver> spSourceResolver;
	CComPtr<IUnknown> spSourceUnk;
	IMFMediaSource *pSource;

	IFC(MFCreateSourceResolver(&spSourceResolver));

	MF_OBJECT_TYPE ObjectType;
	IFC(spSourceResolver->CreateObjectFromURL(m_szFileName, MF_RESOLUTION_MEDIASOURCE, NULL, &ObjectType, &spSourceUnk));
	hr = spSourceUnk->QueryInterface(IID_IMFMediaSource, (void **)&pSource);
	if (hr == E_NOINTERFACE) {
		hr = MF_E_UNSUPPORTED_BYTESTREAM_TYPE;
	}

	IFC(hr);

	*ppSource = pSource;

Cleanup:
	return hr;
}

// Given a source, connect each stream to a renderer for its media type
HRESULT CTedMediaFileRenderer::BuildTopologyFromSource(IMFTopology* pTopology, IMFMediaSource* pSource) {
	HRESULT hr;

	CComPtr<IMFPresentationDescriptor> spPD;

	IFC(pSource->CreatePresentationDescriptor(&spPD));

	DWORD cSourceStreams = 0;
	IFC(spPD->GetStreamDescriptorCount(&cSourceStreams));
	for (DWORD i = 0; i < cSourceStreams; i++) {
		CComPtr<IMFStreamDescriptor> spSD;
		CComPtr<IMFTopologyNode> spNode;
		CComPtr<IMFTopologyNode> spRendererNode;
		BOOL fSelected = FALSE;

		IFC(spPD->GetStreamDescriptorByIndex(i, &fSelected, &spSD));

		IFC(MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &spNode));
		IFC(spNode->SetUnknown(MF_TOPONODE_SOURCE, pSource));
		IFC(spNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, spPD));
		IFC(spNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, spSD));
		IFC(pTopology->AddNode(spNode));

		IFC(CreateRendererForStream(spSD, &spRendererNode));
		IFC(spNode->ConnectOutput(0, spRendererNode, 0));
		IFC(pTopology->AddNode(spRendererNode));
	}

Cleanup:
	return hr;
}

// Create a renderer for the media type on the given stream descriptor
HRESULT CTedMediaFileRenderer::CreateRendererForStream(IMFStreamDescriptor *pSD, IMFTopologyNode** ppRendererNode) {
	HRESULT hr;

	CComPtr<IMFMediaTypeHandler> spMediaTypeHandler;
	CComPtr<IMFActivate> spRendererActivate;
	CComPtr<IMFMediaSink> spRendererSink;
	CComPtr<IMFStreamSink> spRendererStreamSink;

	IMFTopologyNode *pRendererNode;
	GUID gidMajorType;

	IFC(MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &pRendererNode));
	IFC(pSD->GetMediaTypeHandler(&spMediaTypeHandler));
	IFC(spMediaTypeHandler->GetMajorType(&gidMajorType));

	if (gidMajorType == MFMediaType_Audio) {
		IFC(MFCreateAudioRendererActivate(&spRendererActivate));
		IFC(spRendererActivate->ActivateObject(IID_IMFMediaSink, (void **)&spRendererSink));
		IFC(spRendererSink->GetStreamSinkById(0, &spRendererStreamSink));
		IFC(pRendererNode->SetObject(spRendererStreamSink));
	} else if (gidMajorType == MFMediaType_Video) {
		HWND hVideoWindow;
		IFC(m_spVideoWindowHandler->GetVideoWindow((LONG_PTR *)&hVideoWindow));
		IFC(MFCreateVideoRendererActivate(hVideoWindow, &spRendererActivate));
		IFC(spRendererActivate->ActivateObject(IID_IMFMediaSink, (void **)&spRendererSink));
		IFC(pRendererNode->SetObject(spRendererStreamSink));
	} else {
		// Don't have renderers for any other major types
	}

	*ppRendererNode = pRendererNode;

Cleanup:
	return hr;
}

///////////////////////////////////////////////////////////////
//
CTedPlayer::CTedPlayer(CTedMediaEventHandler* pMediaEventHandler, IMFContentProtectionManager* pCPM)
	: m_cRef(1)
	, m_pMediaEventHandler(pMediaEventHandler)
	, m_pCPM(pCPM)
	, m_bIsPlaying(false)
	, m_bIsPaused(false) 
	, m_fTopologySet(false)
	, m_LastSeqID(0)
	, m_hnsMediastartOffset(0)
	, m_fPendingClearCustomTopoloader(false)
	, m_fPendingProtectedCustomTopoloader(false) {
	m_pCPM->AddRef();
}

CTedPlayer::~CTedPlayer() {
	HRESULT hr;

	if (m_pCPM) {
		m_pCPM->Release();
	}

	for (size_t i = 0; i < m_aTopologies.GetCount(); i++) {
		CComPtr<IMFCollection> spSourceNodeCollection;
		IMFTopology* pTopology = m_aTopologies.GetAt(i);

		hr = pTopology->GetSourceNodeCollection(&spSourceNodeCollection);
		if (FAILED(hr)) {
			pTopology->Release();
			continue ;
		}

		DWORD cElements = 0;
		spSourceNodeCollection->GetElementCount(&cElements);
		for (DWORD j = 0; j < cElements; j++) {
			CComPtr<IUnknown> spSourceNodeUnk;
			CComPtr<IMFTopologyNode> spSourceNode;
			CComPtr<IMFMediaSource> spSource;

			hr = spSourceNodeCollection->GetElement(j, &spSourceNodeUnk);
			if (FAILED(hr)) {
				continue ;
			}

			hr = spSourceNodeUnk->QueryInterface(IID_IMFTopologyNode, (void **)&spSourceNode);
			if (FAILED(hr)) {
				continue ;
			}

			hr = spSourceNode->GetUnknown(MF_TOPONODE_SOURCE, IID_IMFMediaSource, (void **)&spSource);
			if (FAILED(hr)) {
				continue ;
			}

			spSource->Shutdown();
		}

		CComPtr<IMFCollection> spOutputNodeCollection;
		hr = pTopology->GetOutputNodeCollection(&spOutputNodeCollection);
		if (FAILED(hr)) {
			pTopology->Release();
			continue ;
		}

		cElements = 0;
		spOutputNodeCollection->GetElementCount(&cElements);
		for (DWORD j = 0; j < cElements; j++) {
			CComPtr<IUnknown> spSinkNodeUnk;
			CComPtr<IMFTopologyNode> spSinkNode;
			CComPtr<IUnknown> spStreamSinkUnk;
			CComPtr<IMFStreamSink> spStreamSink;
			CComPtr<IMFMediaSink> spSink;

			hr = spOutputNodeCollection->GetElement(j, &spSinkNodeUnk);
			if (FAILED(hr)) {
				continue ;
			}

			hr = spSinkNodeUnk->QueryInterface(IID_IMFTopologyNode, (void **)&spSinkNode);
			if (FAILED(hr)) {
				continue;
			}

			hr = spSinkNode->GetObject(&spStreamSinkUnk);
			if (FAILED(hr)) {
				continue;
			}

			hr = spStreamSinkUnk->QueryInterface(IID_IMFStreamSink, (void **)&spSink);
			if (FAILED(hr)) {
				continue;
			}

			spSink->Shutdown();
		}

		pTopology->Release();
	}

	if (m_spClearSeesion) {
		m_spClearSeesion->Shutdown();
	}

	if (m_spProtectedSession) {
		m_spProtectedSession->Shutdown();
	}
}