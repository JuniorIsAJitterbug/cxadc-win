// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win-prop-page - Property Page for the cxadc-win driver
 *
 * Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>
 */

#pragma once

HRESULT cx_ctrls_init(_In_ HWND parent, PCX_PROP_DATA prop_data);
HRESULT cx_ctrls_init_number_combobox(_In_ HWND parent, _In_ INT ctrl_id, _In_ INT min_val, _In_ INT max_val);
HRESULT cx_ctrls_refresh(_In_ HWND parent, _In_ PCX_PROP_DATA prop_data);

HRESULT cx_ctrl_capture_state_set(_In_ HWND parent, _In_ BOOLEAN value);
HRESULT cx_ctrl_capture_state_refresh(_In_ HWND parent, _In_ PCX_PROP_DATA prop_data);

HRESULT cx_ctrl_ouflow_count_refresh(_In_ HWND parent, _In_ PCX_PROP_DATA prop_data);
HRESULT cx_ctrl_ouflow_count_set(_In_ HWND parent, _In_ ULONG value);

HRESULT cx_ctrl_vmux_refresh(_In_ HWND parent, _In_ PCX_PROP_DATA prop_data);
HRESULT cx_ctrl_vmux_set(_In_ HWND parent, _In_ ULONG value);
HRESULT cx_ctrl_vmux_on_change(_In_ HWND parent, _In_ DWORD ctrl_id, _In_ PCX_PROP_DATA prop_data);

HRESULT cx_ctrl_tenbit_refresh(_In_ HWND parent, _In_ PCX_PROP_DATA prop_data);
HRESULT cx_ctrl_tenbit_set(_In_ HWND parent, _In_ BOOLEAN value);
HRESULT cx_ctrl_tenbit_on_change(_In_ HWND parent, _In_ DWORD ctrl_id, _In_ PCX_PROP_DATA prop_data);

HRESULT cx_ctrl_level_refresh(_In_ HWND parent, _In_ PCX_PROP_DATA prop_data);
HRESULT cx_ctrl_level_set(_In_ HWND parent, _In_ ULONG value);
HRESULT cx_ctrl_level_on_change(_In_ HWND ctrl_handle, _In_ PCX_PROP_DATA prop_data);

HRESULT cx_ctrl_sixdb_refresh(_In_ HWND parent, _In_ PCX_PROP_DATA prop_data);
HRESULT cx_ctrl_sixdb_set(_In_ HWND parent, _In_ BOOLEAN value);
HRESULT cx_ctrl_sixdb_on_change(_In_ HWND ctrl_handle, _In_ PCX_PROP_DATA prop_data);

HRESULT cx_ctrl_center_offset_refresh(_In_ HWND parent, _In_ PCX_PROP_DATA prop_data);
HRESULT cx_ctrl_center_offset_set(_In_ HWND parent, _In_ ULONG value);
HRESULT cx_ctrl_center_offset_on_change(_In_ HWND ctrl_handle, _In_ PCX_PROP_DATA prop_data);

HRESULT cx_ctrl_error_set(_In_ HWND parent, _In_ PCWSTR msg);
BOOL CALLBACK cx_ctrl_hide_cb(_In_ HWND ctrl_handle, _In_ LPARAM lparam);
