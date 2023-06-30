#pragma once

#define WM_MF_TOPOLOGYSET   (WM_APP + 1)
#define WM_MF_SESSIONENDED   (WM_APP + 2)
#define WM_MF_SESSIONPLAY   (WM_APP + 3)
#define WM_SPLITTERSIZE (WM_APP + 4)
#define WM_MF_HANDLE_UNTRUSTED_COMPONENT (WM_APP + 5)
#define WM_MF_HANDLE_PROTECTED_CONTENT (WM_APP + 6)
#define WM_MF_HANDLE_INDIVIDUALIZATION (WM_APP + 7)
#define WM_MF_TOPOLOGYREADY (WM_APP + 8)
#define WM_MF_CAPABILITIES_CHANGED (WM_APP + 9)

#define FACILITY_TED 255

#define TED_E_TRANSCODE_PROFILES_FILE_INVALID MAKE_HRESULT(1, FACILITY_TED, 1)
#define TED_E_INVALID_TRANSCODE_PROFILE MAKE_HRESULT(1, FACILITY_TED, 2)