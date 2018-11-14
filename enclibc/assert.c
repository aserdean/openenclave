// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include <assert.h>
#include <openenclave/enclave.h>
#include <openenclave/internal/print.h>

void __enclibc_assert_fail(
    const char* expr,
    const char* file,
    int line,
    const char* function)
{
    oe_host_printf(
        "Assertion failed: %s (%s: %s: %d)\n", expr, file, function, line);
    oe_abort();
}