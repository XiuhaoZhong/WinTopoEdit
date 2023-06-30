#include "CTedPlayer.h"

#include <mferror.h>
#include <initguid.h>

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
