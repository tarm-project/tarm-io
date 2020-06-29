/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "UTCommon.h"

#include "global/Version.h"

#include <openssl/ssl.h>

#include <vector>

struct VersionTest : public testing::Test,
                     public LogRedirector {

};

TEST_F(VersionTest, tls) {
#if OPENSSL_VERSION_NUMBER < 0x1000100fL // 1.0.1
    ASSERT_EQ(io::net::TlsVersion::V1_0, io::global::min_supported_tls_version());
    ASSERT_EQ(io::net::TlsVersion::V1_0, io::global::max_supported_tls_version());
#elif OPENSSL_VERSION_NUMBER < 0x1010100fL // 1.1.1
    ASSERT_EQ(io::net::TlsVersion::V1_0, io::global::min_supported_tls_version());
    ASSERT_EQ(io::net::TlsVersion::V1_2, io::global::max_supported_tls_version());
#else
    ASSERT_EQ(io::net::TlsVersion::V1_0, io::global::min_supported_tls_version());
    ASSERT_EQ(io::net::TlsVersion::V1_3, io::global::max_supported_tls_version());
#endif
}

TEST_F(VersionTest, dtls) {
#if OPENSSL_VERSION_NUMBER < 0x1000200fL // 1.0.2
    ASSERT_EQ(io::net::DtlsVersion::V1_0, io::global::min_supported_dtls_version());
    ASSERT_EQ(io::net::DtlsVersion::V1_0, io::global::max_supported_dtls_version());
#else
    ASSERT_EQ(io::net::DtlsVersion::V1_0, io::global::min_supported_dtls_version());
    ASSERT_EQ(io::net::DtlsVersion::V1_2, io::global::max_supported_dtls_version());
#endif
}
