/*
 * capture-server - Windows port of cxadc_vhs_server
 *
 * Copyright (C) 2025 Jitterbug <jitterbug@posteo.co.uk>
 * Copyright (C) 2024 namazso <admin@namazso.eu>
 */

#pragma once

#define STRINGIFY_(v) #v
#define STRINGIFY(v) STRINGIFY_(v)

#ifdef _WIN32
    #define CXADC_VHS_SERVER_VERSION \
        STRINGIFY(CX_VERSION_MAJOR) "." \
        STRINGIFY(CX_VERSION_MINOR) "." \
        STRINGIFY(CX_VERSION_BUILD) "." \
        STRINGIFY(CX_VERSION_REVISION)
#else
    #define CXADC_VHS_SERVER_VERSION \
        STRINGIFY(CXADC_VHS_SERVER_MAJOR) "." \
        STRINGIFY(CXADC_VHS_SERVER_MINOR) "." \
        STRINGIFY(CXADC_VHS_SERVER_PATCH)
#endif
