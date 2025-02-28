// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * cxadc-win-prop-page - Property Page for the cxadc-win driver
 *
 * Copyright (C) 2024-2025 Jitterbug <jitterbug@posteo.co.uk>
 */

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <windowsx.h>
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>
#include <crtdbg.h>

#include <stdlib.h>

#include "cx_config.h"
#include "cx_ctl_codes.h"
#include "common.h"
#include "resource.h"
