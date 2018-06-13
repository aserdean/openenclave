// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#ifdef OE_BUILD_ENCLAVE

#define Memset oe_memset
#define Memcpy oe_memcpy
#define Memcmp oe_memcmp

#define GetReport oe_get_report

#define VerifyReport oe_verify_report

#define TEST_FCN OE_ECALL

#else

#define Memset memset
#define Memcpy memcpy
#define Memcmp memcmp

// The host side API requires the enclave to be passed in.

oe_enclave_t* g_Enclave = NULL;

#define GetReport(opt, rd, rds, op, ops, rb, rbs) \
    oe_get_report(g_Enclave, opt, rd, rds, op, ops, rb, rbs)

#define VerifyReport(rpt, rptSize, pr) \
    oe_verify_report(g_Enclave, rpt, rptSize, pr)

#define TEST_FCN

#endif

/*
 * g_UniqueID is populated from the first call to oe_parse_report.
 * The enclave's uniqueID is asserted to not change subsequently.
 */
uint8_t g_UniqueID[32];

uint8_t g_AuthorID[32] = {0xca, 0x9a, 0xd7, 0x33, 0x14, 0x48, 0x98, 0x0a,
                          0xa2, 0x88, 0x90, 0xce, 0x73, 0xe4, 0x33, 0x63,
                          0x83, 0x77, 0xf1, 0x79, 0xab, 0x44, 0x56, 0xb2,
                          0xfe, 0x23, 0x71, 0x93, 0x19, 0x3a, 0x8d, 0xa};

uint8_t g_ProductID[16] = {0};

static bool CheckReportData(
    uint8_t* reportBuffer,
    uint32_t reportSize,
    const uint8_t* reportData,
    uint32_t reportDataSize)
{
    oe_report_t parsedReport = {0};
    OE_TEST(oe_parse_report(reportBuffer, reportSize, &parsedReport) == OE_OK);

    return (Memcmp(parsedReport.reportData, reportData, reportDataSize) == 0);
}

static void ValidateReport(
    uint8_t* reportBuffer,
    uint32_t reportSize,
    bool remote,
    const uint8_t* reportData,
    uint32_t reportDataSize)
{
    sgx_quote_t* sgxQuote = NULL;
    sgx_report_t* sgxReport = NULL;

    oe_report_t parsedReport = {0};

    static bool firstTime = true;

    OE_TEST(oe_parse_report(reportBuffer, reportSize, &parsedReport) == OE_OK);

    /* Validate header. */
    OE_TEST(parsedReport.type == OE_ENCLAVE_TYPE_SGX);
    OE_TEST(Memcmp(parsedReport.reportData, reportData, reportDataSize) == 0);

    /* Validate pointer fields. */
    if (remote)
    {
        sgxQuote = (sgx_quote_t*)reportBuffer;
        OE_TEST(reportSize >= sizeof(sgx_quote_t));

        OE_TEST(
            parsedReport.reportData == sgxQuote->report_body.reportData.field);
        OE_TEST(parsedReport.reportDataSize == sizeof(sgx_report_data_t));
        OE_TEST(parsedReport.enclaveReport == (uint8_t*)&sgxQuote->report_body);
        OE_TEST(parsedReport.enclaveReportSize == sizeof(sgx_report_body_t));
    }
    else
    {
        OE_TEST(reportSize == sizeof(sgx_report_t));
        sgxReport = (sgx_report_t*)reportBuffer;

        OE_TEST(parsedReport.reportData == sgxReport->body.reportData.field);
        OE_TEST(parsedReport.reportDataSize == sizeof(sgx_report_data_t));
        OE_TEST(parsedReport.enclaveReport == (uint8_t*)&sgxReport->body);
        OE_TEST(parsedReport.enclaveReportSize == sizeof(sgx_report_body_t));
    }

    /* Validate identity. */
    OE_TEST(parsedReport.identity.idVersion == 0x0);
    OE_TEST(parsedReport.identity.securityVersion == 0x0);

    OE_TEST(parsedReport.identity.attributes & OE_REPORT_ATTRIBUTES_DEBUG);

    OE_TEST(
        !(parsedReport.identity.attributes & OE_REPORT_ATTRIBUTES_RESERVED));

    OE_TEST(
        (bool)(parsedReport.identity.attributes & OE_REPORT_ATTRIBUTES_REMOTE) ==
        remote);

    if (firstTime)
    {
        Memcpy(
            g_UniqueID,
            parsedReport.identity.uniqueID,
            sizeof(parsedReport.identity.uniqueID));

        firstTime = false;
    }

    OE_TEST(
        Memcmp(
            parsedReport.identity.uniqueID,
            g_UniqueID,
            sizeof(parsedReport.identity.uniqueID)) == 0);

    OE_TEST(
        Memcmp(
            parsedReport.identity.authorID,
            g_AuthorID,
            sizeof(parsedReport.identity.authorID)) == 0);

    OE_TEST(
        Memcmp(
            parsedReport.identity.productID,
            g_ProductID,
            sizeof(parsedReport.identity.productID)) == 0);
}

TEST_FCN void TestLocalReport(void* args_)
{
    sgx_target_info_t* targetInfo = (sgx_target_info_t*)args_;

    uint32_t reportDataSize = 0;
    uint8_t reportData[OE_REPORT_DATA_SIZE];
    for (uint32_t i = 0; i < OE_REPORT_DATA_SIZE; ++i)
        reportData[i] = i;

    const uint8_t zeros[OE_REPORT_DATA_SIZE] = {0};

    uint32_t reportSize = 1024;
    uint8_t reportBuffer[1024];

    uint8_t optParams[sizeof(sgx_target_info_t)];
    for (uint32_t i = 0; i < sizeof(optParams); ++i)
        optParams[i] = 0;

    /*
     * Post conditions:
     *     1. On a successful call, the returned report size must always be
     *        sizeof(sgx_report_t);
     *     2. Report must contain specified report data or zeros as report data.
     */

    /*
     * Report data parameters scenarios on enclave side:
     *      1. Report data can be NULL.
     *      2. Report data can be < OE_REPORT_DATA_SIZE
     *      3. Report data can be OE_REPORT_DATA_SIZE
     *      4. Report data cannot exceed OE_REPORT_DATA_SIZE
     *
     * Report data must always be null on host side.
     */
    {
#ifdef OE_BUILD_ENCLAVE
        oe_result_t expectedResult = OE_OK;
#else
        oe_result_t expectedResult = OE_INVALID_PARAMETER;
#endif

        reportSize = 1024 * 1024;
        OE_TEST(
            GetReport(0, NULL, 0, NULL, 0, reportBuffer, &reportSize) == OE_OK);

        if (expectedResult == OE_OK)
        {
            ValidateReport(
                reportBuffer, reportSize, false, zeros, OE_REPORT_DATA_SIZE);
        }

        reportSize = 1024 * 1024;
        reportDataSize = 16;
        OE_TEST(
            GetReport(
                0,
                reportData,
                reportDataSize,
                NULL,
                0,
                reportBuffer,
                &reportSize) == expectedResult);
        if (expectedResult == OE_OK)
        {
            ValidateReport(
                reportBuffer, reportSize, false, reportData, reportDataSize);

            OE_TEST(
                CheckReportData(
                    reportBuffer, reportSize, reportData, reportDataSize + 1) ==
                false);
        }

        reportSize = 1024 * 1024;
        reportDataSize = OE_REPORT_DATA_SIZE;
        OE_TEST(
            GetReport(
                0,
                reportData,
                reportDataSize,
                NULL,
                0,
                reportBuffer,
                &reportSize) == expectedResult);

        if (expectedResult == OE_OK)
        {
            ValidateReport(
                reportBuffer, reportSize, false, reportData, reportDataSize);
        }

        reportSize = 1024 * 1024;
        reportDataSize = OE_REPORT_DATA_SIZE + 1;
        OE_TEST(
            GetReport(
                0,
                reportData,
                reportDataSize,
                NULL,
                0,
                reportBuffer,
                &reportSize) == OE_INVALID_PARAMETER);
    }

    /*
     * optParams scenarios:
     *     1. If optParams is not null, optParamsSize must be
     * sizeof(sgx_target_info_t)
     *     2. Otherwise, both must be null/0.
     *     3. optParams can be zeroed out target info.
     *     4. optParams can be a valid target info.
     */
    {
        reportSize = 1024 * 1024;
        OE_TEST(
            GetReport(
                0,
                NULL,
                0,
                NULL,
                sizeof(optParams),
                reportBuffer,
                &reportSize) == OE_INVALID_PARAMETER);
        OE_TEST(
            GetReport(0, NULL, 0, optParams, 5, reportBuffer, &reportSize) ==
            OE_INVALID_PARAMETER);

        reportSize = 1024 * 1024;
        OE_TEST(
            GetReport(0, NULL, 0, NULL, 0, reportBuffer, &reportSize) == OE_OK);
        ValidateReport(
            reportBuffer, reportSize, false, zeros, OE_REPORT_DATA_SIZE);

        reportSize = 1024 * 1024;
        OE_TEST(
            GetReport(
                0,
                NULL,
                0,
                optParams,
                sizeof(sgx_target_info_t),
                reportBuffer,
                &reportSize) == OE_OK);
        OE_TEST(reportSize == sizeof(sgx_report_t));
        ValidateReport(
            reportBuffer, reportSize, false, zeros, OE_REPORT_DATA_SIZE);

        reportSize = 1024 * 1024;
        OE_TEST(
            GetReport(
                0,
                NULL,
                0,
                targetInfo,
                sizeof(optParams),
                reportBuffer,
                &reportSize) == OE_OK);
        OE_TEST(reportSize == sizeof(sgx_report_t));
        ValidateReport(
            reportBuffer, reportSize, false, zeros, OE_REPORT_DATA_SIZE);
    }

    /*
     * OE_SMALL_BUFFER scenarios:
     *     a. NULL buffer
     *     b. Size too small.
     */
    {
        reportSize = 1024 * 1204;
        OE_TEST(
            GetReport(0, NULL, 0, NULL, 0, NULL, &reportSize) ==
            OE_BUFFER_TOO_SMALL);
        OE_TEST(reportSize == sizeof(sgx_report_t));

        reportSize = 1;
        OE_TEST(
            GetReport(0, NULL, 0, NULL, 0, reportBuffer, &reportSize) ==
            OE_BUFFER_TOO_SMALL);
        OE_TEST(reportSize == sizeof(sgx_report_t));
    }
}

TEST_FCN void TestRemoteReport(void* args_)
{
    uint32_t reportDataSize = 0;
    uint8_t reportData[OE_REPORT_DATA_SIZE];
    for (uint32_t i = 0; i < OE_REPORT_DATA_SIZE; ++i)
        reportData[i] = i;

    const uint8_t zeros[OE_REPORT_DATA_SIZE] = {0};

    uint8_t reportBuffer[OE_MAX_REPORT_SIZE];
    uint32_t reportSize = sizeof(reportBuffer);

    uint8_t optParams[sizeof(sgx_target_info_t)];
    for (uint32_t i = 0; i < sizeof(optParams); ++i)
        optParams[i] = 0;

    uint32_t options = OE_REPORT_OPTIONS_REMOTE_ATTESTATION;
    /*
     * Post conditions:
     *     1. Report must contain specified report data or zeros as report data.
     */

    /*
     * Report data parameters scenarios on enclave side:
     *      a. Report data can be NULL.
     *      b. Report data can be < OE_REPORT_DATA_SIZE
     *      c. Report data can be OE_REPORT_DATA_SIZE
     *      d. Report data cannot exceed OE_REPORT_DATA_SIZE
     *
     * Report data must always be null on host side.
     */
    {
#ifdef OE_BUILD_ENCLAVE
        oe_result_t expectedResult = OE_OK;
#else
        oe_result_t expectedResult = OE_INVALID_PARAMETER;
#endif

        reportSize = sizeof(reportBuffer);
        OE_TEST(
            GetReport(options, NULL, 0, NULL, 0, reportBuffer, &reportSize) ==
            OE_OK);
        ValidateReport(
            reportBuffer, reportSize, true, zeros, OE_REPORT_DATA_SIZE);

        reportSize = sizeof(reportBuffer);
        reportDataSize = 16;
        OE_TEST(
            GetReport(
                options,
                reportData,
                reportDataSize,
                NULL,
                0,
                reportBuffer,
                &reportSize) == expectedResult);
        if (expectedResult == OE_OK)
        {
            ValidateReport(
                reportBuffer, reportSize, true, reportData, reportDataSize);
            OE_TEST(
                CheckReportData(
                    reportBuffer, reportSize, reportData, reportDataSize + 1) ==
                false);
        }

        reportSize = sizeof(reportBuffer);
        reportDataSize = OE_REPORT_DATA_SIZE;
        OE_TEST(
            GetReport(
                options,
                reportData,
                reportDataSize,
                NULL,
                0,
                reportBuffer,
                &reportSize) == expectedResult);
        if (expectedResult == OE_OK)
        {
            ValidateReport(
                reportBuffer, reportSize, true, reportData, reportDataSize);
        }

        reportSize = sizeof(reportBuffer);
        reportDataSize = OE_REPORT_DATA_SIZE + 1;
        OE_TEST(
            GetReport(
                options,
                reportData,
                reportDataSize,
                NULL,
                0,
                reportBuffer,
                &reportSize) == OE_INVALID_PARAMETER);
    }

    /*
     * optParams scenarios:
     *     1. Both optParams and optParamsSize must be NULL/0.
     */
    {
        reportSize = sizeof(reportBuffer);
        OE_TEST(
            GetReport(
                options,
                NULL,
                0,
                NULL,
                sizeof(optParams),
                reportBuffer,
                &reportSize) == OE_INVALID_PARAMETER);
        OE_TEST(
            GetReport(
                options, NULL, 0, optParams, 5, reportBuffer, &reportSize) ==
            OE_INVALID_PARAMETER);
    }

    /*
     * OE_SMALL_BUFFER scenarios:
     *     a. NULL buffer
     *     b. Size too small.
     */
    {
        reportSize = sizeof(reportBuffer);

        OE_TEST(
            GetReport(options, NULL, 0, NULL, 0, NULL, &reportSize) ==
            OE_BUFFER_TOO_SMALL);

        // Assert that with the returned reportSize buffer can be created.
        OE_TEST(
            GetReport(options, NULL, 0, NULL, 0, reportBuffer, &reportSize) ==
            OE_OK);

        reportSize = 1;
        OE_TEST(
            GetReport(options, NULL, 0, NULL, 0, reportBuffer, &reportSize) ==
            OE_BUFFER_TOO_SMALL);

        // Assert that with the returned reportSize buffer can be created.
        OE_TEST(
            GetReport(options, NULL, 0, NULL, 0, reportBuffer, &reportSize) ==
            OE_OK);
    }
}

TEST_FCN void TestParseReportNegative(void* args_)
{
    uint8_t reportBuffer[OE_MAX_REPORT_SIZE] = {0};
    oe_report_t parsedReport = {0};

    // 1. Null report passed in.
    OE_TEST(oe_parse_report(NULL, 0, &parsedReport) == OE_INVALID_PARAMETER);

    // 2. Report size less than size of sgx_report_t.
    OE_TEST(
        oe_parse_report(
            reportBuffer, sizeof(sgx_report_t) - 1, &parsedReport) ==
        OE_INVALID_PARAMETER);

    // 3. Report size greater than size of sgx_report_t but less than
    // sizeof(sgx_quote_t)
    OE_TEST(
        oe_parse_report(reportBuffer, sizeof(sgx_quote_t) - 1, &parsedReport) ==
        OE_INVALID_PARAMETER);

    // 4. NULL parsedReport passed in.
    OE_TEST(
        oe_parse_report(reportBuffer, sizeof(sgx_quote_t), NULL) ==
        OE_INVALID_PARAMETER);
}

// Use the current enclave itself as the target enclave.
static void GetSGXTargetInfo(sgx_target_info_t* sgxTargetInfo)
{
    sgx_report_t report = {0};
    uint32_t reportSize = sizeof(sgx_report_t);

    OE_TEST(
        GetReport(0, NULL, 0, NULL, 0, (uint8_t*)&report, &reportSize) ==
        OE_OK);

    Memset(sgxTargetInfo, 0, sizeof(*sgxTargetInfo));
    Memcpy(
        sgxTargetInfo->mrenclave,
        report.body.mrenclave,
        sizeof(sgxTargetInfo->mrenclave));
    Memcpy(
        &sgxTargetInfo->attributes,
        &report.body.attributes,
        sizeof(sgxTargetInfo->attributes));
    Memcpy(
        &sgxTargetInfo->misc_select,
        &report.body.miscselect,
        sizeof(sgxTargetInfo->attributes));
}

TEST_FCN void TestLocalVerifyReport(void* args_)
{
    uint8_t targetInfo[sizeof(sgx_target_info_t)];
    uint32_t targetInfoSize = sizeof(targetInfo);

    uint8_t report[sizeof(sgx_report_t)] = {0};
    uint32_t reportSize = sizeof(report);
    sgx_target_info_t* tamperedTargetInfo = NULL;

    uint8_t reportData[sizeof(sgx_report_data_t)];
    for (uint32_t i = 0; i < sizeof(reportData); ++i)
    {
        reportData[i] = i;
    }

    GetSGXTargetInfo((sgx_target_info_t*)targetInfo);

    // 1. Report with no custom report data.
    OE_TEST(
        GetReport(
            0, NULL, 0, targetInfo, targetInfoSize, report, &reportSize) ==
        OE_OK);
    OE_TEST(VerifyReport(report, reportSize, NULL) == OE_OK);

// 2. Report with full custom report data.
#ifdef OE_BUILD_ENCLAVE
    OE_TEST(
        GetReport(
            0,
            reportData,
            sizeof(reportData),
            targetInfo,
            targetInfoSize,
            report,
            &reportSize) == OE_OK);
    OE_TEST(VerifyReport(report, reportSize, NULL) == OE_OK);

    // 3. Report with partial custom report data.
    OE_TEST(
        GetReport(
            0,
            reportData,
            sizeof(reportData) / 2,
            targetInfo,
            targetInfoSize,
            report,
            &reportSize) == OE_OK);
    OE_TEST(VerifyReport(report, reportSize, NULL) == OE_OK);
#endif

    // 4. Negative case.

    // Tamper with the target info.
    tamperedTargetInfo = (sgx_target_info_t*)targetInfo;
    tamperedTargetInfo->mrenclave[0]++;

    OE_TEST(
        GetReport(
            0, NULL, 0, targetInfo, targetInfoSize, report, &reportSize) ==
        OE_OK);
    OE_TEST(VerifyReport(report, reportSize, NULL) == OE_VERIFY_FAILED);
}

TEST_FCN void TestRemoteVerifyReport(void* args_)
{
    uint8_t reportBuffer[OE_MAX_REPORT_SIZE] = {0};
    uint32_t reportSize = sizeof(reportBuffer);

    uint8_t reportData[sizeof(sgx_report_data_t)];
    uint32_t reportDataSize = sizeof(reportData);

    for (uint32_t i = 0; i < sizeof(reportData); ++i)
    {
        reportData[i] = i;
    }

    uint32_t options = OE_REPORT_OPTIONS_REMOTE_ATTESTATION;

    OE_UNUSED(reportDataSize);
    /*
     * Report data parameters scenarios on enclave side:
     *      a. Report data can be NULL.
     *      b. Report data can be < OE_REPORT_DATA_SIZE
     *      c. Report data can be OE_REPORT_DATA_SIZE
     * On host side, report data must be null.
     */
    {
        reportSize = sizeof(reportBuffer);
        OE_TEST(
            GetReport(options, NULL, 0, NULL, 0, reportBuffer, &reportSize) ==
            OE_OK);
        OE_TEST(VerifyReport(reportBuffer, reportSize, NULL) == OE_OK);

#if OE_BUILD_ENCLAVE
        reportSize = sizeof(reportBuffer);
        reportDataSize = 16;
        OE_TEST(
            GetReport(
                options,
                reportData,
                reportDataSize,
                NULL,
                0,
                reportBuffer,
                &reportSize) == OE_OK);
        OE_TEST(VerifyReport(reportBuffer, reportSize, NULL) == OE_OK);

        reportSize = sizeof(reportBuffer);
        reportDataSize = OE_REPORT_DATA_SIZE;
        OE_TEST(
            GetReport(
                options,
                reportData,
                reportDataSize,
                NULL,
                0,
                reportBuffer,
                &reportSize) == OE_OK);
        OE_TEST(VerifyReport(reportBuffer, reportSize, NULL) == OE_OK);
#endif
    }
}