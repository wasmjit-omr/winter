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

#ifndef WINTER_ENVIRONMENT_HPP
#define WINTER_ENVIRONMENT_HPP

#include "type.hpp"

namespace winter {

/**
 * \brief Represents a single WebAssembly sandboxed environment.
 *
 * Each Environment represents a completely isolated WebAssembly sandbox. Each sandbox operates
 * entirely independently from all others. Because of this, there are several restrictions on
 * sharing between WebAssembly code instantiated in different sandboxes:
 *
 * - WebAssembly code from one sandbox cannot directly call WebAssembly code in a different sandbox
 * - References can only be used in the WebAssembly sandbox in which they were created
 * - Linear memory cannot be shared between WebAssembly code in different sandboxes, even if it is
 *   marked as shared
 */
class Environment {
    TypeTable _types;
public:
    /**
     * \brief Gets the TypeTable used by WebAssembly code in this sandbox.
     */
    const TypeTable& types() const { return _types; }

    /**
     * \brief Gets the TypeTable used by WebAssembly code in this sandbox.
     */
    TypeTable& types() { return _types; }
};

} // namespace winter

#endif
