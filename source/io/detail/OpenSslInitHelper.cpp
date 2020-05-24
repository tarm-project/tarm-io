/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <iostream>

namespace io {
namespace detail {

class OpenSslInitHelper {
public:
    OpenSslInitHelper() {
        ERR_load_crypto_strings();
        SSL_load_error_strings();
        const int result = SSL_library_init();
        if (!result) {
            std::cerr << "OpenSslInitHelper: Failed to init OpenSSL" << std::endl;
        }
    }
};

static OpenSslInitHelper openssl_init_helper;

} // namespace detail
} // namespace io
