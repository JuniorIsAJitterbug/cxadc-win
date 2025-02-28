// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win-prop-page - Property Page for the cxadc-win driver
 *
 * Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>
 */

#pragma once

BOOL APIENTRY DllMain(_In_ HANDLE module, _In_ DWORD reason, _In_ LPVOID reserved);
BOOL APIENTRY cx_prop_page_provider(_In_ PSP_PROPSHEETPAGE_REQUEST prop_page_req, _In_ LPFNADDPROPSHEETPAGE add_fn, _In_ LPARAM param);
UINT CALLBACK cx_dlg_callback(_In_ HWND dlg, _In_ UINT msg, _In_ LPPROPSHEETPAGEW ps_page);

INT_PTR CALLBACK cx_dlg_proc(_In_ HWND dlg, _In_ UINT msg, _In_ WPARAM wparam, _In_ LPARAM lparam);
BOOL WINAPI cx_wm_initdialog(_In_ HWND dlg, _In_ HWND focus, _In_ LPARAM lparam);
VOID WINAPI cx_wm_command(_In_ HWND dlg, _In_ INT ctrl_id, _In_ HWND dlg_ctrl, _In_ UINT notif_id);
LRESULT WINAPI cx_wm_ctlcolor(_In_ HWND dlg, _In_ HDC hdc, _In_ HWND dlg_child, _In_ INT type);
VOID CALLBACK cx_wm_destroy(_In_ HWND dlg);

HRESULT cx_init_crtdbg();

VOID cx_timer_refresh_cb(_In_ HWND dlg, _In_ UINT msg, _In_ UINT_PTR id, _In_ DWORD tc);
VOID cx_set_error(_In_ HWND dlg, _In_ PCWSTR msg);
