// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win-prop-page - Property Page for the cxadc-win driver
 *
 * Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>
 */

#include "pch.h"
#include "prop_page.h"

#include "controls.h"
#include "device.h"

#pragma comment(lib, "comctl32.lib")

HINSTANCE g_instance = NULL;

_Use_decl_annotations_
BOOL APIENTRY DllMain(
    HANDLE module,
    DWORD reason,
    LPVOID reserved
)
{
    UNREFERENCED_PARAMETER(reason);
    UNREFERENCED_PARAMETER(reserved);

    g_instance = (HINSTANCE)module;
    return TRUE;
}

_Use_decl_annotations_
BOOL APIENTRY cx_prop_page_provider(
    PSP_PROPSHEETPAGE_REQUEST psp_req,
    LPFNADDPROPSHEETPAGE add_psp_fn,
    LPARAM lparam
)
{
    HPROPSHEETPAGE hps_page = NULL;
    PCX_PROP_DATA prop_data = NULL;

    if (psp_req->PageRequested != SPPSR_ENUM_ADV_DEVICE_PROPERTIES)
    {
        goto error_1;
    }

    PWCHAR device_id = NULL;

    if (FAILED(cx_get_device_id(psp_req->DeviceInfoSet, psp_req->DeviceInfoData, &device_id)))
    {
        // non-cx device?
        goto error_1;
    }

    if (wcsstr(device_id, DEVICE_ID_STR) == NULL)
    {
        // non-video function
        _HeapFree(device_id);
        goto error_1;
    }

    if ((prop_data = _HeapAllocZero(sizeof(CX_PROP_DATA))) == NULL)
    {
        goto error_2;
    }

    prop_data->dev_path = NULL;

    if (FAILED(cx_get_device_path(device_id, &prop_data->dev_path)))
    {
        goto error_3;
    }

    PROPSHEETPAGE ps_page =
    {
        .dwSize = sizeof(PROPSHEETPAGE),
        .dwFlags = PSP_USECALLBACK | PSP_HASHELP,
        .hInstance = g_instance,
        .pszTemplate = MAKEINTRESOURCEW(DLG_CXADCWIN),
        .lParam = (LPARAM)prop_data,
        .pfnDlgProc = cx_dlg_proc,
        .pfnCallback = cx_dlg_callback
    };

    if ((hps_page = CreatePropertySheetPageW(&ps_page)) == NULL)
    {
        goto error_3;
    }

    if (!add_psp_fn(hps_page, lparam))
    {
        goto error_4;
    }

    return TRUE;

error_4:
    DestroyPropertySheetPage(hps_page);

error_3:
    _HeapFree(prop_data->dev_path);
    _HeapFree(prop_data);

error_2:
    DEBUG_ERROR("Error creating property sheet");

error_1:
    return FALSE;
}

_Use_decl_annotations_
INT_PTR CALLBACK cx_dlg_proc(
    HWND dlg,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch (msg)
    {
        HANDLE_MSG(dlg, WM_INITDIALOG, cx_wm_initdialog);
        HANDLE_MSG(dlg, WM_COMMAND, cx_wm_command);
        HANDLE_MSG(dlg, WM_CTLCOLORDLG, cx_wm_ctlcolor);
        HANDLE_MSG(dlg, WM_CTLCOLORSTATIC, cx_wm_ctlcolor);
        HANDLE_MSG(dlg, WM_DESTROY, cx_wm_destroy);
    }

    return FALSE;
}

_Use_decl_annotations_
BOOL WINAPI cx_wm_initdialog(
    HWND dlg,
    HWND dlg_focus,
    LPARAM lparam
)
{
    DEBUG_LOG("WM_INITDIALOG");

    UNREFERENCED_PARAMETER(dlg);
    UNREFERENCED_PARAMETER(dlg_focus);

    LPPROPSHEETPAGEW ps_page = (LPPROPSHEETPAGEW)lparam;
    PCX_PROP_DATA prop_data = (PCX_PROP_DATA)ps_page->lParam;

    if (prop_data == 0)
    {
        return FALSE;
    }

    HRESULT hr = S_OK;

    // err if return is 0 AND last error is !0
    SetLastError(ERROR_SUCCESS);

    if (SetWindowLongPtrW(dlg, DWLP_USER, (LONG_PTR)prop_data) == 0 && GetLastError() != ERROR_SUCCESS)
    {
        hr = HRESULT_FROM_LASTERROR;
        goto exit;
    }

    // init state/config
    if (FAILED(hr = cx_init_prop_data(prop_data)))
    {
        goto exit;
    }

    // init ctrls
    if (FAILED(hr = cx_ctrls_init(dlg, prop_data)))
    {
        goto exit;
    }

    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-settimer#remarks
    BOOL timer_exc_suppression = FALSE;
    SetUserObjectInformationW(
        GetCurrentProcess(),
        UOI_TIMERPROC_EXCEPTION_SUPPRESSION,
        &timer_exc_suppression,
        sizeof(timer_exc_suppression));

    // create refresh timer
    if (SetTimer(dlg, IDT_TIMER_REFRESH, 1000, cx_timer_refresh_cb) == 0)
    {
        hr = HRESULT_FROM_LASTERROR;
        goto exit;
    }

    return FALSE;

exit:
    if (FAILED(hr))
    {
        DEBUG_ERROR("Error initializing dialog, hr=%08X", hr);
        cx_set_error(dlg, L"Error initializing dialog");
    }

    return FALSE;
}

_Use_decl_annotations_
VOID WINAPI cx_wm_command(
    HWND dlg,
    INT ctrl_id,
    HWND dlg_ctrl,
    UINT notif_id
)
{
    PCX_PROP_DATA prop_data = NULL;
    HRESULT hr = S_OK;

    if ((prop_data = (PCX_PROP_DATA)GetWindowLongPtrW(dlg, DWLP_USER)) == 0)
    {
        hr = HRESULT_FROM_LASTERROR;
        goto exit;
    }

    switch (ctrl_id)
    {
    case IDC_COMBO_CONFIG_LEVEL:
        if (notif_id == CBN_SELCHANGE)
        {
            hr = cx_ctrl_level_on_change(dlg_ctrl, prop_data);
        }
        break;

    case IDC_COMBO_CONFIG_CENTER_OFFSET:
        if (notif_id == CBN_SELCHANGE)
        {
            hr = cx_ctrl_center_offset_on_change(dlg_ctrl, prop_data);
        }
        break;

    case IDC_CHECK_CONFIG_SIXDB:
        if (notif_id == BN_CLICKED)
        {
            hr = cx_ctrl_sixdb_on_change(dlg_ctrl, prop_data);
        }
        break;

    case IDC_RADIO_CONFIG_8BIT:
    case IDC_RADIO_CONFIG_10BIT:
        if (notif_id == BN_CLICKED)
        {
            hr = cx_ctrl_tenbit_on_change(dlg, ctrl_id, prop_data);
        }
        break;

    case IDC_RADIO_CONFIG_VMUX_0:
    case IDC_RADIO_CONFIG_VMUX_1:
    case IDC_RADIO_CONFIG_VMUX_2:
    case IDC_RADIO_CONFIG_VMUX_3:
        if (notif_id == BN_CLICKED)
        {
            hr = cx_ctrl_vmux_on_change(dlg, ctrl_id, prop_data);
        }
        break;
    }

exit:
    if (FAILED(hr))
    {
        DEBUG_ERROR("Error updating controls %d, hr=%08X", ctrl_id, hr);
        cx_set_error(dlg, L"Error updating controls.");
    }
}

_Use_decl_annotations_
LRESULT WINAPI cx_wm_ctlcolor(
    HWND dlg,
    HDC hdc,
    HWND dlg_child,
    INT type
)
{
    UNREFERENCED_PARAMETER(dlg);

    // match bg color to other device manager tabs
    switch (type)
    {
    case CTLCOLOR_DLG:
        return (LRESULT)CreateSolidBrush(BG_COLOR);

    case CTLCOLOR_STATIC:
        switch (GetDlgCtrlID(dlg_child))
        {
        case IDC_EDIT_INFO_PATH:
        case IDC_EDIT_INFO_CAPTURE:
        case IDC_EDIT_INFO_OUFLOWS:
            SetTextColor(hdc, FG_COLOR);
            SetBkMode(hdc, TRANSPARENT);
            SetBkColor(hdc, BG_COLOR);
            break;

        case IDC_EDIT_ERROR_MESSAGE:
            SetTextColor(hdc, RGB(255, 0, 0));
            SetBkMode(hdc, TRANSPARENT);
            SetBkColor(hdc, BG_COLOR);
            break;

        default:
            HDC dlgc_hdc = GetDC(dlg_child);

            SetTextColor(dlgc_hdc, FG_COLOR);
            SetBkMode(dlgc_hdc, TRANSPARENT);
            SetBkColor(dlgc_hdc, BG_COLOR);

            ReleaseDC(dlg_child, dlgc_hdc);
            break;
        }

        return (LRESULT)CreateSolidBrush(BG_COLOR);
    }

    return (LRESULT)0;
}

_Use_decl_annotations_
VOID CALLBACK cx_wm_destroy(
    HWND dlg
)
{
    DEBUG_LOG("WM_DESTROY called");

    KillTimer(dlg, IDT_TIMER_REFRESH);
    SetWindowLongPtrW(dlg, DWLP_USER, 0);
    PostQuitMessage(0);
}

_Use_decl_annotations_
UINT CALLBACK cx_dlg_callback(
    HWND dlg,
    UINT msg,
    LPPROPSHEETPAGEW ps_page
)
{
    UNREFERENCED_PARAMETER(dlg);

    if (msg == PSPCB_RELEASE)
    {
        DEBUG_LOG("Freeing data due to PSPCB_RELEASE");
        PCX_PROP_DATA prop_data = (PCX_PROP_DATA)ps_page->lParam;

        if (prop_data != NULL)
        {
            _SafeHeapFree(prop_data->dev_path);
            _SafeHeapFree(prop_data);
        }
    }

    return 1;
}

_Use_decl_annotations_
VOID cx_timer_refresh_cb(
    HWND dlg,
    UINT msg,
    UINT_PTR id,
    DWORD tc
)
{
    UNREFERENCED_PARAMETER(msg);
    UNREFERENCED_PARAMETER(id);
    UNREFERENCED_PARAMETER(tc);

    PCX_PROP_DATA prop_data = (PCX_PROP_DATA)GetWindowLongPtrW(dlg, DWLP_USER);

    DEBUG_LOG("Refresh timer callback called");

    if (prop_data == 0 || FAILED(cx_init_prop_data(prop_data)))
    {
        cx_set_error(dlg, L"Unable to get prop data for device.");
        return;
    }

    if (FAILED(cx_ctrls_refresh(dlg, prop_data)))
    {
        cx_set_error(dlg, L"Unable to refresh controls.");
    }
}

HRESULT cx_init_crtdbg()
{
#pragma warning(push)
#pragma warning(disable: 4127 6285) // disable warning about constant for non-debug builds
    if (_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG) == -1 ||
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG) == -1 ||
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG) == -1
        )
    {
#pragma warning(pop)
        return HRESULT_FROM_ERRNO;
    }

    return S_OK;
}

_Use_decl_annotations_
VOID cx_set_error(
    HWND dlg,
    PCWSTR msg
)
{
    DEBUG_LOG("Showing error message");

    EnumChildWindows(dlg, cx_ctrl_hide_cb, 0);
    KillTimer(dlg, IDT_TIMER_REFRESH);
    cx_ctrl_error_set(dlg, msg);
}
