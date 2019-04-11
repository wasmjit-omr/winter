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

#ifndef WINTER_TYPE_HPP
#define WINTER_TYPE_HPP

#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

#include "wassert.hpp"

namespace winter {

struct FuncSig;

enum class PrimitiveValueType : uint8_t {
    I32 = 0x7f,
    I64 = 0x7e,
    F32 = 0x7d,
    F64 = 0x7c,
    FuncRef = 0x70
};

struct ValueType {
    PrimitiveValueType type;
    const void* extra;

    constexpr bool operator==(const ValueType& other) const {
        return type == other.type && extra == other.extra;
    }

    const FuncSig* func_sig() const {
        WASSERT(type == PrimitiveValueType::FuncRef, "Called ValueType::func_sig() on non-function type");
        return static_cast<const FuncSig*>(extra);
    }

    static bool is_assignable_to(const ValueType& dest, const ValueType& src);

    static constexpr ValueType i32() {
        return { PrimitiveValueType::I32, nullptr };
    }

    static constexpr ValueType i64() {
        return { PrimitiveValueType::I64, nullptr };
    }

    static constexpr ValueType f32() {
        return { PrimitiveValueType::F32, nullptr };
    }

    static constexpr ValueType f64() {
        return { PrimitiveValueType::F64, nullptr };
    }

    static constexpr ValueType func_ref(const FuncSig* sig = nullptr) {
        return { PrimitiveValueType::FuncRef, sig };
    }

    template <typename T>
    static constexpr ValueType for_type() {
        static_assert(sizeof(T) != sizeof(T), "Unsupported native type for WASM interop");
    }
};
static_assert(std::is_standard_layout<ValueType>::value, "ValueType must be standard layout");

template <>
constexpr ValueType ValueType::for_type<uint32_t>() {
    return ValueType::i32();
}

template <>
constexpr ValueType ValueType::for_type<int32_t>() {
    return ValueType::i32();
}

template <>
constexpr ValueType ValueType::for_type<uint64_t>() {
    return ValueType::i64();
}

template <>
constexpr ValueType ValueType::for_type<int64_t>() {
    return ValueType::i64();
}

template <>
constexpr ValueType ValueType::for_type<float>() {
    return ValueType::f32();
}

template <>
constexpr ValueType ValueType::for_type<double>() {
    return ValueType::f64();
}

union Value {
    uint32_t i32;
    uint64_t i64;
    float f32;
    float f64;
    void* ref;
};

struct TypedValue {
    ValueType type;
    Value val;
};

struct FuncSig {
    std::vector<ValueType> return_types;
    std::vector<ValueType> param_types;

    FuncSig() {}
    FuncSig(std::vector<ValueType> return_types, std::vector<ValueType> param_types)
        : return_types(std::move(return_types)), param_types(std::move(param_types)) {}
};

class TypeTable {
    std::vector<std::unique_ptr<FuncSig>> _sigs;

public:
    const FuncSig& sig(FuncSig sig) {
        return this->sig(std::move(sig.return_types), std::move(sig.param_types));
    }
    const FuncSig& sig(std::vector<ValueType> return_types, std::vector<ValueType> param_types);
};

} // namespace winter

#endif
