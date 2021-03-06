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

#ifndef WINTER_WASSERT_HPP
#define WINTER_WASSERT_HPP

#define WASSERT(condition, ...)                                           \
    if (!(condition))                                                     \
        do {                                                              \
            ::winter::assertion_failure(__FILE__, __LINE__, __VA_ARGS__); \
        } while (0)

namespace winter {

[[noreturn]] void assertion_failure(const char* file, int line, const char* message, ...);

} // namespace winter

#endif
