#pragma once

#include "stdafx.h"

#include "tedutil.h"

class CTedTranscodeTopologyBuilder {
public:
	CTedTranscodeTopologyBuilder(LPCWSTR szSource, HRESULT *phr);
	~CTedTranscodeTopologyBuilder();

	size_t GetPropertyCount();
	size_t GetProfileCount();
	CAtlStringW GetPropertyName(size_t iElement);

	HRESULT BuildTranscodeTopology(const CAtlStringW &strProfileName, LPCWSTR szOutputFilePath, IMFTopology **ppTopology);

protected:
	struct StringGuidMap {
		LPCWSTR szName;
		GUID gidValue;
	};

	enum EAttributeType {
		AttributeType_UINT32,
		AttributeType_String,
		AttributeType_Ratio,
		AttributeType_AudioSubType,
		AttributeType_VideoSubType,
		AttributeType_ContainerType,
	};

	struct StringAttributeMap {
		LPCWSTR szName;
		LPCWSTR szSecondName;
		GUID gidAttribute;
		EAttributeType eAttributeType;
	};

	// String -> GUID conversion functions.
	HRESULT StringToContainerType(LPCWSTR szName, GUID* pgidContainerType);
	HRESULT StringToAudioSubType(LPCWSTR szSubType, GUID* pgidSubType);
	HRESULT StringToVideoSubType(LPCWSTR szSubType, GUID* pgidSubType);
	HRESULT StringToAMFormatType(LPCWSTR szSubType, GUID* pgidSubType);
	HRESULT FindGuidInMap(LPCWSTR szName, GUID* pgidValue, const StringGuidMap* pMap, DWORD cMapElements);

	// Attribute loading functions.
	HRESULT LoadContainerAttributes(ITedDataLoader *pLoader, IMFTranscodeProfile *pProfile);
	HRESULT LoadAttributes(ITedDataLoader *pLoader, IMFAttributes* pAttributes, const StringAttributeMap* pMap, size_t cMapElements);
	HRESULT TryLoadUINT32Attribute(ITedDataLoader *pLoader, LPCWSTR szName, IMFAttributes* pAttributes, GUID gidAttributeName);
	HRESULT TryLoadRatioAttribute(ITedDataLoader *pLoader, LPCWSTR szNameFirst, LPCWSTR szNameSecond, IMFAttributes *pAttributes, GUID gidAttributeName);
	HRESULT TryLoadStringAttribte(ITedDataLoader *pLoader, LPCWSTR szName, IMFAttributes* pAttributes, GUID gidAttributeName);
	HRESULT TryLoadAudioSubTypeAttribute(ITedDataLoader *pLoader, LPCWSTR szName, IMFAttributes *pAttributes, GUID gidAttributeName);
	HRESULT TryLoadVideoSubTypeAttribute(ITedDataLoader *pLoader, LPCWSTR szName, IMFAttributes *pAttributes, GUID gidAttributeName);

	// Source attribute copying.
	HRESULT FilPropertyWithSourceType(IMFTranscodeProfile *pProfile);
	HRESULT GetSourceMeidaType(REFGUID gidMajorType, IMFMediaType **ppMediaType);
	HRESULT CopyDesireAttributes(IMFMediaType *pSourceType, IMFAttributes *pTargetAttributes, const StringAttributeMap* pMap, size_t cMapElement);

private:
	const static LPCWSTR m_kszTranscodeProfileFile;
	
	const static StringGuidMap m_kaAudioSubTypeMap[];
	const static StringGuidMap m_kaVideoSubTypeMap[];
	const static StringGuidMap m_kaContainerMap[];

	const static StringAttributeMap m_kaContainerAttributeMap[];
	const static StringAttributeMap m_kaAudioAttributeMap[];
	const static StringAttributeMap m_kaVideoAttributeMap[];

	CAtlArray<CAtlStringW> m_arrProfileNames;
	CInterfaceArray<IMFTranscodeProfile> m_arrProfiles;

	CComPtr<IMFMediaSource> m_spSource;

};

