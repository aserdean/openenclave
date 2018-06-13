// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifndef _OE_SGXLOAD_H
#define _OE_SGXLOAD_H

#include <openenclave/host.h>
#include <openenclave/internal/sgxcreate.h>
#include <openenclave/internal/sgxtypes.h>

OE_EXTERNC_BEGIN

#define OE_SGX_NO_DEVICE_HANDLE -1

OE_INLINE bool oe_sgx_is_simulation_load_context(
    const oe_sgx_load_context_t* context)
{
    return (context && (context->attributes & OE_ENCLAVE_FLAG_SIMULATE));
}

OE_INLINE bool oe_sgx_is_debug_load_context(
    const oe_sgx_load_context_t* context)
{
    return (context && (context->attributes & OE_ENCLAVE_FLAG_DEBUG));
}

oe_result_t oe_sgx_create_enclave(
    oe_sgx_load_context_t* context,
    uint64_t enclaveSize,
    uint64_t* enclaveAddr);

oe_result_t oe_sgx_load_enclave_data(
    oe_sgx_load_context_t* context,
    uint64_t base,
    uint64_t addr,
    uint64_t src,
    uint64_t flags,
    bool extend);

oe_result_t oe_sgx_initialize_enclave(
    oe_sgx_load_context_t* context,
    uint64_t addr,
    const oe_sgx_enclave_properties_t* properties,
    OE_SHA256* mrenclave);

OE_EXTERNC_END

#endif /* _OE_SGXLOAD_H */