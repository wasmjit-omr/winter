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

#include "func.hpp"
#include "module.hpp"

namespace winter {

std::shared_ptr<UnlinkedFunc> UnlinkedFunc::instantiate(const AbstractFunc& func, Environment& env) {
    WASSERT(!func.is_import, "Attempt to create UnlinkedFunc from import before linking");

    auto unlinked = std::shared_ptr<UnlinkedFunc>(new UnlinkedFunc());

    unlinked->_internal.sig = &env.types().sig(func.sig);
    unlinked->_debug_name = func.debug_name;
    unlinked->_instrs = func.instrs;

    return unlinked;
}

std::unique_ptr<LinkedFunc> LinkedFunc::instantiate(std::shared_ptr<UnlinkedFunc> unlinked, ModuleInstance* module) {
    auto linked = std::unique_ptr<LinkedFunc>(new LinkedFunc());

    linked->_internal.unlinked = unlinked->internal();
    linked->_internal.module = module->internal();
    linked->_unlinked = std::move(unlinked);
    linked->_module = module;

    return linked;
}

} // namespace winter
