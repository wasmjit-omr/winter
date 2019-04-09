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

#ifndef WINTER_STRONGTYPE_HPP
#define WINTER_STRONGTYPE_HPP

#include <type_traits>

namespace winter {

// Based on https://www.fluentcpp.com/2016/12/08/strong-types-for-strong-interfaces/
template <typename T, typename Tag>
class NamedType {
    T _value;
public:
    constexpr explicit NamedType(const T& value) : _value(value) {}
    constexpr explicit NamedType(T&& value) : _value(value) {}

    T& get() { return _value; }
    constexpr const T& get() const { return _value; }
};

template <typename T, typename Tag>
constexpr bool operator==(const NamedType<T, Tag>& lhs, const NamedType<T, Tag>& rhs) {
    return lhs.get() == rhs.get();
}

template <typename T, typename Tag>
constexpr bool operator!=(const NamedType<T, Tag>& lhs, const NamedType<T, Tag>& rhs) {
    return lhs.get() != rhs.get();
}

template <typename T, typename Tag>
constexpr bool operator<(const NamedType<T, Tag>& lhs, const NamedType<T, Tag>& rhs) {
    return lhs.get() < rhs.get();
}

template <typename T, typename Tag>
constexpr bool operator<=(const NamedType<T, Tag>& lhs, const NamedType<T, Tag>& rhs) {
    return lhs.get() <= rhs.get();
}

template <typename T, typename Tag>
constexpr bool operator>(const NamedType<T, Tag>& lhs, const NamedType<T, Tag>& rhs) {
    return lhs.get() > rhs.get();
}

template <typename T, typename Tag>
constexpr bool operator>=(const NamedType<T, Tag>& lhs, const NamedType<T, Tag>& rhs) {
    return lhs.get() >= rhs.get();
}

template <typename T, typename Tag>
constexpr NamedType<T, Tag> operator+(const NamedType<T, Tag>& lhs, const NamedType<T, Tag>& rhs) {
    return NamedType<T, Tag>(lhs.get() + rhs.get());
}

template <typename T, typename Tag>
constexpr NamedType<T, Tag> operator-(const NamedType<T, Tag>& lhs, const NamedType<T, Tag>& rhs) {
    return NamedType<T, Tag>(lhs.get() - rhs.get());
}

} // namespace winter

#endif
