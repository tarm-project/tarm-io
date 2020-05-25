/*----------------------------------------------------------------------------------------------
 *  Copyright (c) 2020 - present Alexander Voitenko
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *----------------------------------------------------------------------------------------------*/

#include "HashRange.h"

namespace tarm {
namespace io {

void hash_combine_impl(std::size_t& h, std::uint64_t k) {
    const std::uint64_t m = 0xc6a4a7935bd1e995ul;
    const int r = 47;

    k *= m;
    k ^= k >> r;
    k *= m;

    h ^= k;
    h *= m;

    // Completely arbitrary number, to prevent 0's from hashing to 0.
    h += 0xe6546b64;
}

} // namespace io
} // namespace tarm
