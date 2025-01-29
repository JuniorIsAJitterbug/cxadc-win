// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win-prop-page - Property Page for the cxadc-win driver
 *
 * Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>
 */

#include "pch.h"
#include "controls.h"

#include "device.h"

_Use_decl_annotations_
HRESULT cx_ctrls_refresh(
    HWND parent,
    PCX_PROP_DATA prop_data
)
{
    DEBUG_LOG("Refreshing controls");

    RETURN_HR_IF_FAILED(cx_ctrl_capture_state_refresh(parent, prop_data));
    RETURN_HR_IF_FAILED(cx_ctrl_ouflow_count_refresh(parent, prop_data));
    RETURN_HR_IF_FAILED(cx_ctrl_vmux_refresh(parent, prop_data));
    RETURN_HR_IF_FAILED(cx_ctrl_level_refresh(parent, prop_data));
    RETURN_HR_IF_FAILED(cx_ctrl_center_offset_refresh(parent, prop_data));
    RETURN_HR_IF_FAILED(cx_ctrl_tenbit_refresh(parent, prop_data));
    RETURN_HR_IF_FAILED(cx_ctrl_sixdb_refresh(parent, prop_data));

    return S_OK;
}

_Use_decl_annotations_
HRESULT cx_ctrls_init(
    HWND parent,
    PCX_PROP_DATA prop_data
)
{
    DEBUG_LOG("Initializing ctrls");

    HRESULT hr = S_OK;

    if (FAILED(hr = cx_ctrls_init_number_combobox(
        parent,
        IDC_COMBO_CONFIG_LEVEL,
        CX_CTRL_CONFIG_LEVEL_MIN,
        CX_CTRL_CONFIG_LEVEL_MAX)))
    {
        return hr;
    }

    if (FAILED(hr = cx_ctrls_init_number_combobox(
        parent,
        IDC_COMBO_CONFIG_CENTER_OFFSET,
        CX_CTRL_CONFIG_CENTER_OFFSET_MIN,
        CX_CTRL_CONFIG_CENTER_OFFSET_MAX)))
    {
        return hr;
    }

    // hide err text
    if (!ShowWindow(GetDlgItem(parent, IDC_EDIT_ERROR_MESSAGE), SW_HIDE))
    {
        return HRESULT_FROM_LASTERROR;
    }

    // set win32 path str
    if (!SetDlgItemTextW(parent, IDC_EDIT_INFO_PATH, prop_data->win32_path))
    {
        return HRESULT_FROM_LASTERROR;
    }

    cx_ctrl_capture_state_set(parent, prop_data->state.is_capturing);
    cx_ctrl_ouflow_count_set(parent, prop_data->state.ouflow_count);

    cx_ctrl_level_set(parent, prop_data->config.level);
    cx_ctrl_vmux_set(parent, prop_data->config.vmux);
    cx_ctrl_tenbit_set(parent, prop_data->config.tenbit);
    cx_ctrl_sixdb_set(parent, prop_data->config.sixdb);
    cx_ctrl_center_offset_set(parent, prop_data->config.center_offset);

    return hr;
}

_Use_decl_annotations_
HRESULT cx_ctrl_capture_state_refresh(
    HWND parent,
    PCX_PROP_DATA prop_data
)
{
    if (prop_data->state.is_capturing == prop_data->prev_state.is_capturing)
    {
        return S_FALSE;
    }

    DEBUG_LOG("is_capturing refresh, prev: %d, new: %d",
        prop_data->prev_state.is_capturing, prop_data->state.is_capturing);

    return cx_ctrl_capture_state_set(parent, prop_data->state.is_capturing);
}

_Use_decl_annotations_
HRESULT cx_ctrl_capture_state_set(
    HWND parent,
    BOOLEAN value
)
{
    DEBUG_LOG("capture state set, new: %d", value);

    if (!SetDlgItemTextW(parent, IDC_EDIT_INFO_CAPTURE, value ? L"Active" : L"Inactive"))
    {
        return HRESULT_FROM_LASTERROR;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT cx_ctrl_ouflow_count_refresh(
    HWND parent,
    PCX_PROP_DATA prop_data
)
{
    if (prop_data->state.ouflow_count == prop_data->prev_state.ouflow_count)
    {
        return S_FALSE;
    }

    DEBUG_LOG("ouflow_count refresh, prev: %d, new: %d",
        prop_data->prev_state.ouflow_count, prop_data->state.ouflow_count);

    return cx_ctrl_ouflow_count_set(parent, prop_data->state.ouflow_count);
}

_Use_decl_annotations_
HRESULT cx_ctrl_ouflow_count_set(
    HWND parent,
    ULONG value
)
{
    DEBUG_LOG("ouflow_count state set, new: %d", value);


    if (!SetDlgItemInt(parent, IDC_EDIT_INFO_OUFLOWS, value, TRUE))
    {
        return HRESULT_FROM_LASTERROR;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT cx_ctrls_init_number_combobox(
    HWND parent,
    INT ctrl_id,
    INT min_val,
    INT max_val
)
{
    HWND ctrl_handle = NULL;
    WCHAR tmp_str[128];

    if ((ctrl_handle = GetDlgItem(parent, ctrl_id)) == NULL)
    {
        return HRESULT_FROM_LASTERROR;
    }

    // populate items
    for (INT i = min_val; i <= max_val; i++)
    {

        if (_itow_s(i, tmp_str, _countof(tmp_str), 10) != 0)
        {
            return HRESULT_FROM_ERRNO;
        }

        if (ComboBox_AddString(ctrl_handle, tmp_str) == CB_ERR)
        {
            return E_FAIL;
        }

        if (ComboBox_SetItemData(ctrl_handle, i, i) == CB_ERR)
        {
            return E_FAIL;
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT cx_ctrl_level_refresh(
    HWND parent,
    PCX_PROP_DATA prop_data
)
{
    if (prop_data->config.level == prop_data->prev_config.level)
    {
        return S_FALSE;
    }

    DEBUG_LOG("level refresh, prev: %d, new: %d",
        prop_data->prev_config.level, prop_data->config.level);

    return cx_ctrl_level_set(parent, prop_data->config.level);
}

_Use_decl_annotations_
HRESULT cx_ctrl_level_set(
    HWND parent,
    ULONG value
)
{
    HWND ctrl_handle = NULL;

    if ((ctrl_handle = GetDlgItem(parent, IDC_COMBO_CONFIG_LEVEL)) == NULL)
    {
        return HRESULT_FROM_LASTERROR;
    }

    DEBUG_LOG("level set, new: %d", value);

    ComboBox_SetCurSel(ctrl_handle, value);

    if (ComboBox_GetCurSel(ctrl_handle) != value)
    {
        return E_FAIL;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT cx_ctrl_level_on_change(
    HWND ctrl_handle,
    PCX_PROP_DATA prop_data
)
{
    LRESULT new_val = ComboBox_GetItemData(ctrl_handle, ComboBox_GetCurSel(ctrl_handle));

    // check result for err
    if (new_val == CB_ERR)
    {
        return E_FAIL;
    }

    // only change if new value
    if ((DWORD)new_val == prop_data->config.level)
    {
        return S_FALSE;
    }

    DEBUG_LOG("level on_change, prev: %d, new: %d", prop_data->config.level, new_val);

    // set level ioctl
    return cx_ioctl_set(prop_data, CX_IOCTL_SET_LEVEL, (DWORD)new_val);
}

_Use_decl_annotations_
HRESULT cx_ctrl_vmux_refresh(
    HWND parent,
    PCX_PROP_DATA prop_data
)
{
    if (prop_data->config.vmux == prop_data->prev_config.vmux)
    {
        return S_FALSE;
    }

    DEBUG_LOG("vmux refresh, prev: %d, new: %d",
        prop_data->prev_config.vmux, prop_data->config.vmux);

    return cx_ctrl_vmux_set(parent, prop_data->config.vmux);
}

_Use_decl_annotations_
HRESULT cx_ctrl_vmux_set(
    HWND parent,
    ULONG value
)
{
    HWND ctrl_handle = NULL;

    switch (value)
    {
    case 0:
        ctrl_handle = GetDlgItem(parent, IDC_RADIO_CONFIG_VMUX_0);
        break;

    case 1:
        ctrl_handle = GetDlgItem(parent, IDC_RADIO_CONFIG_VMUX_1);
        break;

    case 2:
        ctrl_handle = GetDlgItem(parent, IDC_RADIO_CONFIG_VMUX_2);
        break;

    case 3:
        ctrl_handle = GetDlgItem(parent, IDC_RADIO_CONFIG_VMUX_3);
        break;
    }

    if (ctrl_handle == NULL)
    {
        return HRESULT_FROM_LASTERROR;
    }

    DEBUG_LOG("vmux set, new: %d", value);

    if (!CheckRadioButton(parent, IDC_RADIO_CONFIG_VMUX_0, IDC_RADIO_CONFIG_VMUX_3, GetDlgCtrlID(ctrl_handle)))
    {
        return HRESULT_FROM_LASTERROR;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT cx_ctrl_vmux_on_change(
    HWND parent,
    DWORD ctrl_id,
    PCX_PROP_DATA prop_data
)
{
    DWORD new_val = 0;

    // map control ids to values
    switch (ctrl_id)
    {
        case IDC_RADIO_CONFIG_VMUX_0:
            new_val = 0;
            break;

        case IDC_RADIO_CONFIG_VMUX_1:
            new_val = 1;
            break;

        case IDC_RADIO_CONFIG_VMUX_2:
            new_val = 2;
            break;

        case IDC_RADIO_CONFIG_VMUX_3:
            new_val = 3;
            break;

        default:
            DEBUG_ERROR("Unknown vmux control state");
            return E_UNEXPECTED;
    }

    DEBUG_LOG("vmux on_change, prev: %d, new: %d", prop_data->config.vmux, new_val);

    // set new checked
    RETURN_HR_IF_FAILED(cx_ctrl_vmux_set(parent, ctrl_id));

    // set vmux ioctl
    return cx_ioctl_set(prop_data, CX_IOCTL_SET_VMUX, new_val);
}

_Use_decl_annotations_
HRESULT cx_ctrl_tenbit_refresh(
    HWND parent,
    PCX_PROP_DATA prop_data
)
{
    if (prop_data->config.tenbit == prop_data->prev_config.tenbit)
    {
        return S_FALSE;
    }

    DEBUG_LOG("tenbit refresh, prev: %d, new: %d",
        prop_data->prev_config.tenbit, prop_data->config.tenbit);

    return cx_ctrl_tenbit_set(parent, prop_data->config.tenbit);
}

_Use_decl_annotations_
HRESULT cx_ctrl_tenbit_set(
    HWND parent,
    BOOLEAN value
)
{
    HWND ctrl_handle = NULL;

    switch (value)
    {
    case FALSE:
        ctrl_handle = GetDlgItem(parent, IDC_RADIO_CONFIG_8BIT);
        break;

    case TRUE:
        ctrl_handle = GetDlgItem(parent, IDC_RADIO_CONFIG_10BIT);
        break;

    default:
        DEBUG_ERROR("Unknown tenbit control state");
        return E_UNEXPECTED;
    }

    if (ctrl_handle == NULL)
    {
        return HRESULT_FROM_LASTERROR;
    }

    DEBUG_LOG("tenbit set, new: %d", value);

    if (!CheckRadioButton(parent, IDC_RADIO_CONFIG_8BIT, IDC_RADIO_CONFIG_10BIT, GetDlgCtrlID(ctrl_handle)))
    {
        return HRESULT_FROM_LASTERROR;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT cx_ctrl_tenbit_on_change(
    HWND parent,
    DWORD ctrl_id,
    PCX_PROP_DATA prop_data
)
{
    BOOLEAN new_val = 0;

    // map control ids to values
    switch (ctrl_id)
    {
        case IDC_RADIO_CONFIG_8BIT:
            new_val = FALSE;
            break;

        case IDC_RADIO_CONFIG_10BIT:
            new_val = TRUE;
            break;

        default:
            DEBUG_ERROR("unknown tenbit control");
            return E_UNEXPECTED;
    }

    DEBUG_LOG("tenbit on_change, prev: %d, new: %d", prop_data->config.tenbit, new_val);

    // set new checked
    RETURN_HR_IF_FAILED(cx_ctrl_tenbit_set(parent, new_val));

    // set tenbit ioctl
    return cx_ioctl_set(prop_data, CX_IOCTL_SET_TENBIT, new_val);
}

_Use_decl_annotations_
HRESULT cx_ctrl_sixdb_refresh(
    HWND parent,
    PCX_PROP_DATA prop_data
)
{
    if (prop_data->config.sixdb == prop_data->prev_config.sixdb)
    {
        return S_FALSE;
    }

    DEBUG_LOG("sixdb refresh, prev: %d, new: %d",
        prop_data->prev_config.sixdb, prop_data->config.sixdb);

    return cx_ctrl_sixdb_set(parent, prop_data->config.sixdb);
}

_Use_decl_annotations_
HRESULT cx_ctrl_sixdb_set(
    HWND parent,
    BOOLEAN value
)
{
    DEBUG_LOG("sixdb set, new: %d, new: %d", value);

    // (un)check ctrl
    if (!CheckDlgButton(parent, IDC_CHECK_CONFIG_SIXDB, value ? BST_CHECKED : BST_UNCHECKED))
    {
        return HRESULT_FROM_LASTERROR;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT cx_ctrl_sixdb_on_change(
    HWND ctrl_handle,
    PCX_PROP_DATA prop_data
)
{
    LRESULT new_val = Button_GetCheck(ctrl_handle);

    // check for valid state
    if (new_val != BST_CHECKED && new_val != BST_UNCHECKED)
    {
        return E_FAIL;
    }

    // only change if new value
    if ((DWORD)new_val == prop_data->config.sixdb)
    {
        return S_FALSE;
    }

    DEBUG_LOG("sixdb on_change, prev: %d, new: %d", prop_data->config.sixdb, new_val);

    // set sixdb ioctl
    return cx_ioctl_set(prop_data, CX_IOCTL_SET_SIXDB, (DWORD)new_val);
}

_Use_decl_annotations_
HRESULT cx_ctrl_center_offset_refresh(
    HWND parent,
    PCX_PROP_DATA prop_data
)
{
    if (prop_data->config.center_offset == prop_data->prev_config.center_offset)
    {
        return S_FALSE;
    }

    DEBUG_LOG("center_offset refresh, prev: %d, new: %d",
        prop_data->prev_config.center_offset, prop_data->config.center_offset);

    return cx_ctrl_center_offset_set(parent, prop_data->config.center_offset);
}

_Use_decl_annotations_
HRESULT cx_ctrl_center_offset_set(
    HWND parent,
    ULONG value
)
{
    HWND ctrl_handle = NULL;

    if ((ctrl_handle = GetDlgItem(parent, IDC_COMBO_CONFIG_CENTER_OFFSET)) == NULL)
    {
        return HRESULT_FROM_LASTERROR;
    }

    DEBUG_LOG("center_offset set, new: %d", value);

    ComboBox_SetCurSel(ctrl_handle, value);

    if (ComboBox_GetCurSel(ctrl_handle) != value)
    {
        return E_FAIL;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT cx_ctrl_center_offset_on_change(
    HWND ctrl_handle,
    PCX_PROP_DATA prop_data
)
{
    LRESULT new_val = ComboBox_GetItemData(ctrl_handle, ComboBox_GetCurSel(ctrl_handle));

    // check result for err
    if (new_val == CB_ERR)
    {
        return E_FAIL;
    }

    // only change if new value
    if ((DWORD)new_val == prop_data->config.center_offset)
    {
        return S_FALSE;
    }

    DEBUG_LOG("center_offset on_change, prev: %d, new: %d", prop_data->config.center_offset, new_val);

    // set center offset ioctl
    return cx_ioctl_set(prop_data, CX_IOCTL_SET_CENTER_OFFSET, (DWORD)new_val);
}

_Use_decl_annotations_
HRESULT cx_ctrl_error_set(
    HWND parent,
    LPCWSTR msg
)
{
    if (!SetDlgItemTextW(parent, IDC_EDIT_ERROR_MESSAGE, msg))
    {
        return HRESULT_FROM_LASTERROR;
    }

    if (!ShowWindow(GetDlgItem(parent, IDC_EDIT_ERROR_MESSAGE), SW_SHOW))
    {
        return HRESULT_FROM_LASTERROR;
    }

    return S_OK;
}

_Use_decl_annotations_
BOOL CALLBACK cx_ctrl_hide_cb(
    HWND ctrl_handle,
    LPARAM lparam
)
{
    UNREFERENCED_PARAMETER(lparam);

    return ShowWindow(ctrl_handle, SW_HIDE);
}
