/*
 * Copyright 2019 IBM Corp. and others
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "wassert.hpp"

namespace winter {

[[noreturn]] void assertion_failure(const char* file, int line, const char* message, ...) {
    va_list args;

    std::fprintf(stderr, "Assertion failed at %s:%d: ", file, line);

    va_start(args, message);
    std::vfprintf(stderr, message, args);
    va_end(args);

    std::fputc('\n', stderr);

    std::abort();
}

} // namespace winter
