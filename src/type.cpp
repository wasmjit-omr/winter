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

#include <algorithm>

#include "type.hpp"

namespace winter {

bool ValueType::is_assignable_to(const ValueType& dest, const ValueType& src) {
    switch (dest.type) {
        case PrimitiveValueType::I32:
            return src.type == PrimitiveValueType::I32;
        case PrimitiveValueType::I64:
            return src.type == PrimitiveValueType::I64;
        case PrimitiveValueType::F32:
            return src.type == PrimitiveValueType::F32;
        case PrimitiveValueType::F64:
            return src.type == PrimitiveValueType::F64;
        case PrimitiveValueType::FuncRef: {
            if (src.type != PrimitiveValueType::FuncRef) {
                return false;
            }

            const FuncSig* dest_sig = dest.func_sig();
            const FuncSig* src_sig = src.func_sig();

            return !dest_sig || src_sig == dest_sig;
        }
    }

    WASSERT(false, "Invalid PrimitiveValueType 0x%x", static_cast<uint32_t>(dest.type));
}

const FuncSig& TypeTable::sig(std::vector<ValueType> return_types, std::vector<ValueType> param_types) {
    auto it = std::find_if(_sigs.begin(), _sigs.end(), [&](const std::unique_ptr<FuncSig>& sig) {
        return sig->return_types == return_types && sig->param_types == param_types;
    });

    if (it != _sigs.end()) {
        return **it;
    }

    auto sig = std::unique_ptr<FuncSig>(new FuncSig { std::move(return_types), std::move(param_types) });
    auto& sig_ref = *sig;

    _sigs.push_back(std::move(sig));
    return sig_ref;
}

} // namespace winter
