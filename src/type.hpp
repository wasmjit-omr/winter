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

/**
 * \brief Represents a primitive WebAssembly type.
 *
 * This enumeration represents the "primitive" part of a WebAssembly type. Namely, the part of a
 * type that is not dynamically controlled by WebAssembly code. For instance, the primitive i32 type
 * can be represented solely as a primitive type, since the WebAssembly code does not have any
 * control over the semantics of that type.
 */
enum class PrimitiveValueType : uint8_t {
    I32 = 0x7f,    ///< The WebAssembly i32 type.
    I64 = 0x7e,    ///< The WebAssembly i64 type.
    F32 = 0x7d,    ///< The WebAssembly f32 type.
    F64 = 0x7c,    ///< The WebAssembly f64 type.
    FuncRef = 0x70 ///< A WebAssembly funcref type.
};

/**
 * \brief Represents a full WebAssembly type.
 */
struct ValueType {
    PrimitiveValueType type; ///< The primitive part of this WebAssembly type.

    /**
     * \brief A field whose interpretation depends on ValueType::type.
     *
     * This field contains the dynamically-controlled part of a WebAssembly type which cannot be
     * represented as a simple enumeration constant. The interpretation of this field's value
     * depends on the value of ValueType::type:
     *
     * - PrimitiveValueType::I32: this field should be nullptr.
     * - PrimitiveValueType::I64: this field should be nullptr.
     * - PrimitiveValueType::F32: this field should be nullptr.
     * - PrimitiveValueType::F64: this field should be nullptr.
     * - PrimitiveValueType::FuncRef: this field should contain a pointer to a FuncSig representing
     *   the signature of the typed function reference, or nullptr for an untyped function
     *   reference.
     */
    const void* extra;

    constexpr bool operator==(const ValueType& other) const {
        return type == other.type && extra == other.extra;
    }

    /**
     * \brief Gets the function signature of a function type.
     *
     * This function gets the function signature of a type whose primitive part is
     * PrimitiveValueType::FuncRef. It is invalid to call this function on a type whose primitive
     * part does not represent a function reference.
     *
     * \return a pointer to the function signature of a typed function reference. For an untyped
     *         function reference (i.e. the funcref type), returns nullptr instead.
     */
    const FuncSig* func_sig() const {
        WASSERT(type == PrimitiveValueType::FuncRef, "Called ValueType::func_sig() on non-function type");
        return static_cast<const FuncSig*>(extra);
    }

    /**
     * \brief Determines whether two types are compatible for assignment.
     *
     * This functions checks whether a value of type src can be put into a variable of type dest.
     *
     * \param[in] dest the type of the variable being assigned to.
     * \param[in] src the type of the value being written.
     * \return true if the assignment is valid, false otherwise.
     */
    static bool is_assignable_to(const ValueType& dest, const ValueType& src);

    /**
     * \brief Creates the primitive WebAssembly i32 type.
     */
    static constexpr ValueType i32() {
        return { PrimitiveValueType::I32, nullptr };
    }

    /**
     * \brief Creates the primitive WebAssembly i64 type.
     */
    static constexpr ValueType i64() {
        return { PrimitiveValueType::I64, nullptr };
    }

    /**
     * \brief Creates the primitive WebAssembly f32 type.
     */
    static constexpr ValueType f32() {
        return { PrimitiveValueType::F32, nullptr };
    }

    /**
     * \brief Creates the primitive WebAssembly f64 type.
     */
    static constexpr ValueType f64() {
        return { PrimitiveValueType::F64, nullptr };
    }

    /**
     * \brief Creates a WebAssembly type for a function reference.
     *
     * \warning The function signature being passed in must be a pointer as returned from
     *          TypeTable::sig for type equality checking to work correctly. Do not pass a pointer
     *          to a function signature which is not part of a TypeTable.
     *
     * \param[in] sig the function signature of a typed function reference. nullptr to create an
     *                untyped function reference.
     */
    static constexpr ValueType func_ref(const FuncSig* sig = nullptr) {
        return { PrimitiveValueType::FuncRef, sig };
    }

    /**
     * \brief Returns a primitive WebAssembly type corresponding to the given C++ type.
     */
    template <typename T>
    static constexpr ValueType for_type() = delete;
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

/**
 * \brief Represents an untagged WebAssembly value.
 *
 * Such a value has no associated type information, so it is necessary to know the type from some
 * other source in order to know which of the fields of this union should be read/written.
 */
union Value {
    uint32_t i32; ///< This value as a WebAssembly i32.
    uint64_t i64; ///< This value as a WebAssembly i64.
    float f32; ///< This value as a WebAssembly f32.
    float f64; ///< This value as a WebAssembly f64.
    void* ref; ///< This value as a WebAssembly reference.
};

/**
 * \brief Represents a type-tagged WebAssembly value.
 * 
 * Such a value includes type information alongside it that dictates what WebAssembly type the value
 * in question represents, so no other source of information is needed to determine how to interpret
 * the data.
 */
struct TypedValue {
    ValueType type; ///< The type of this WebAssembly value
    Value val; ///< The actual WebAssembly value itself
};

/**
 * \brief Represents the signature of a WebAssembly function.
 *
 * Note that after module instantiation, FuncSigs will be deduplicated within the context of a
 * single winter::Environment. This means that function signatures can usually be checked for
 * equality by simply comparing pointer values rather than having to examine the types in the
 * signature itself.
 */
struct FuncSig {
    /**
     * \brief The types of WebAssembly values returned by this function.
     *
     * For functions returning multiple WebAssembly values, the types will be in the order they
     * would be written in the textual WebAssembly representation. In other words, the values will
     * be pushed onto the stack in the same order as this list of types.
     */
    std::vector<ValueType> return_types;

    /**
     * \brief The types of WebAssembly values taken by this function as parameters.
     *
     * For functions having multiple parameters, the types will be in the order they would be
     * written in the textual WebAssembly representation. In other words, the values will be popped
     * from the stack in the reverse of the order of this list of types.
     */
    std::vector<ValueType> param_types;

    FuncSig() {}
    FuncSig(std::vector<ValueType> return_types, std::vector<ValueType> param_types)
        : return_types(std::move(return_types)), param_types(std::move(param_types)) {}
};

/**
 * \brief A table of deduplicated WebAssembly type information.
 *
 * This table is designed to deduplicate dynamic parts of WebAssembly types. Any two types returned
 * from the same TypeTable can be checked for equality by simply comparing pointers.
 */
class TypeTable {
    std::vector<std::unique_ptr<FuncSig>> _sigs;

public:
    /**
     * \brief Deduplicates a function signature, returning a reference to the canonical version.
     */
    const FuncSig& sig(FuncSig sig) {
        return this->sig(std::move(sig.return_types), std::move(sig.param_types));
    }

    /**
     * \brief Creates or finds a function signature with the given return and parameter types,
     *        returning a reference to the canonical version.
     */
    const FuncSig& sig(std::vector<ValueType> return_types, std::vector<ValueType> param_types);
};

} // namespace winter

#endif
