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

#ifndef WINTER_MEMORY_HPP
#define WINTER_MEMORY_HPP

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>

#include "strongtype.hpp"
#include "wassert.hpp"

namespace winter {

using wptr_t = uint32_t;

constexpr size_t WASM_PAGE_SHIFT = 16; // 64KiB pages
constexpr size_t WASM_PAGE_SIZE = 1 << WASM_PAGE_SHIFT;
constexpr size_t WASM_PAGE_MASK = ~(WASM_PAGE_SIZE - 1);

constexpr uint32_t MEMORY_FLAG_SHARED = (1 << 0);

using NumPages = NamedType<size_t, struct NamedPagesTag>;
static_assert(std::is_standard_layout<NumPages>::value, "NumPages must be transparent");
static_assert(sizeof(NumPages) == sizeof(size_t), "NumPages must be transparent");
static_assert(alignof(NumPages) == alignof(size_t), "NumPages must be transparent");

constexpr NumPages WASM_UNLIMITED_PAGES = NumPages(static_cast<size_t>(-1));
constexpr NumPages WASM_ALLOCATE_FAILURE = NumPages(static_cast<size_t>(-1));

class Memory;

struct AbstractMemory {
    bool is_import;

    bool is_shared;
    NumPages initial_pages = NumPages(0);
    NumPages max_pages = WASM_UNLIMITED_PAGES;

    AbstractMemory(bool is_import, bool is_shared, NumPages initial_pages, NumPages max_pages)
        : is_import(is_import), is_shared(is_shared), initial_pages(initial_pages), max_pages(max_pages) {}

    static AbstractMemory for_import(bool is_shared, NumPages initial_pages, NumPages max_pages) {
        return AbstractMemory(true, is_shared, initial_pages, max_pages);
    }
};

struct MemoryInternal {
    uint32_t flags = 0;

    void* start = nullptr;
    size_t size = 0;

    NumPages current_capacity_pages = NumPages(0);
    NumPages max_capacity_pages = NumPages(0);

    Memory* container;

    explicit MemoryInternal(Memory* container) : container(container) {}
    MemoryInternal(const MemoryInternal&) = delete;
    MemoryInternal(MemoryInternal&&) = delete;

    MemoryInternal& operator=(const MemoryInternal&) = delete;
    MemoryInternal& operator=(MemoryInternal&&) = delete;
};
static_assert(std::is_standard_layout<MemoryInternal>::value, "MemoryInternal must be standard layout");

class Memory {
    MemoryInternal _internal;
    NumPages _initial_pages;

    bool alloc_exactly(NumPages num_pages);
    bool alloc_at_least(NumPages num_pages);
public:
    explicit Memory(const AbstractMemory& abstract) : _internal(this), _initial_pages(abstract.initial_pages) {
        WASSERT(!abstract.is_import, "Memory created from unlinked AbstractMemory");
        WASSERT(!abstract.is_shared || abstract.max_pages != WASM_UNLIMITED_PAGES, "Shared memories cannot have unlimited capacity");

        _internal.max_capacity_pages = abstract.max_pages;
        if (abstract.max_pages != WASM_UNLIMITED_PAGES) {
            if (!alloc_exactly(abstract.max_pages))
                throw std::bad_alloc();
        } else {
            if (!alloc_at_least(abstract.initial_pages))
                throw std::bad_alloc();
        }

        if (abstract.is_shared)
            _internal.flags |= MEMORY_FLAG_SHARED;

        _internal.size = abstract.initial_pages.get() << WASM_PAGE_SHIFT;
    }
    Memory(const Memory&) = delete;
    Memory(Memory&&) = delete;
    ~Memory();

    Memory& operator=(const Memory&) = delete;
    Memory& operator=(Memory&&) = delete;

    MemoryInternal* internal() { return &_internal; }

    size_t size() const { return _internal.size; }
    NumPages size_pages() const { return NumPages(size() >> WASM_PAGE_SHIFT); }
    NumPages initial_size_pages() const { return _initial_pages; }

    NumPages current_capacity_pages() const { return _internal.current_capacity_pages; }
    NumPages max_capacity_pages() const { return _internal.max_capacity_pages; }

    bool is_at_max_capacity() const { return current_capacity_pages() == max_capacity_pages(); }
    bool is_shared() const { return (_internal.flags & MEMORY_FLAG_SHARED) != 0; }

    NumPages grow(NumPages new_pages);

    bool is_valid_address(wptr_t addr, size_t size) const {
        size_t addr_s = static_cast<size_t>(addr);
        return (addr_s + size) >= addr_s && (addr_s + size) <= this->size();
    }

    void* ptr_to(wptr_t addr, size_t size) {
        WASSERT(is_valid_address(addr, size), "Out-of-bounds address passed to Memory::ptr_to");
        return static_cast<void*>(static_cast<uint8_t*>(_internal.start) + static_cast<size_t>(addr));
    }
    const void* ptr_to(wptr_t addr, size_t size) const {
        WASSERT(is_valid_address(addr, size), "Out-of-bounds address passed to Memory::ptr_to");
        return static_cast<const void*>(static_cast<uint8_t*>(_internal.start) + static_cast<size_t>(addr));
    }

    void* data() { return _internal.start; }
    const void* data() const { return _internal.start; }

    bool load(void* buf, wptr_t addr, size_t size) const {
        if (is_valid_address(addr, size)) {
            std::memcpy(buf, ptr_to(addr, size), size);
            return true;
        } else {
            return false;
        }
    }

    template <typename T>
    bool load(T* buf, wptr_t addr) const {
        static_assert(std::is_standard_layout<T>::value, "Only standard layout types can be in WebAssembly linear memory");
        return load(static_cast<void*>(buf), addr, sizeof(T));
    }

    bool store(const void* buf, wptr_t addr, size_t size) {
        if (is_valid_address(addr, size)) {
            std::memcpy(ptr_to(addr, size), buf, size);
            return true;
        } else {
            return false;
        }
    }

    template <typename T>
    bool store(const T* buf, wptr_t addr) {
        static_assert(std::is_standard_layout<T>::value, "Only standard layout types can be in WebAssembly linear memory");
        return store(static_cast<const void*>(buf), addr, sizeof(T));
    }

    static std::shared_ptr<Memory> create_shared(NumPages min, NumPages max) {
        return std::shared_ptr<Memory>(new Memory(AbstractMemory(false, true, min, max)));
    }

    static std::shared_ptr<Memory> create_unshared(NumPages min, NumPages max) {
        return std::shared_ptr<Memory>(new Memory(AbstractMemory(false, false, min, max)));
    }
};

} // namespace winter

#endif
