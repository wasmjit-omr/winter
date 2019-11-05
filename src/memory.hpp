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

/**
 * \brief A sentinel value used to represent that the maximum capacity of a memory is unbounded.
 */
constexpr NumPages WASM_UNLIMITED_PAGES = NumPages(static_cast<size_t>(-1));

/**
 * \brief A sentinel value returned if a linear memory could not be grown.
 */
constexpr NumPages WASM_ALLOCATE_FAILURE = NumPages(static_cast<size_t>(-1));

class Memory;

/**
 * \brief Represents the parameters of a Memory that has not yet been created.
 *
 * There are two different types of AbstractMemory:
 *
 * - An AbstractMemory with is_import set to false represents a memory which will be created as part
 *   of instantiating a WebAssembly module.
 * - An AbstractMemory with is_import set to true represents the parameters of a memory which will
 *   be linked from another module.
 *
 * \see Memory
 */
struct AbstractMemory {
    bool is_import; ///< Whether this linear memory will be imported from another module.

    bool is_shared; ///< Whether this linear memory will be shared between agents.
    NumPages initial_pages = NumPages(0); ///< The initial size of this linear memory (in WASM pages).
    NumPages max_pages = WASM_UNLIMITED_PAGES; ///< The maximum capacity of this linear memory (in WASM pages).

    AbstractMemory(bool is_import, bool is_shared, NumPages initial_pages, NumPages max_pages)
        : is_import(is_import), is_shared(is_shared), initial_pages(initial_pages), max_pages(max_pages) {}

    /**
     * \brief Creates an abstract memory representing a memory that will be imported from another
     *        module.
     *
     * \param[in] is_shared Whether the memory being imported will be shared between agents.
     * \param[in] initial_pages The minimum initial size of the imported memory (in WASM pages) that
     *                          the imported memory must have.
     * \param[in] max_pages The maximum capacity of the imported memory (in WASM pages) that the
     *                      imported memory must not exceed.
     */
    static AbstractMemory for_import(bool is_shared, NumPages initial_pages = NumPages(0), NumPages max_pages = WASM_UNLIMITED_PAGES) {
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

/**
 * \brief A WebAssembly linear memory.
 *
 * \warning For unshared linear memories, many operations produce undefined behaviour when executed
 *          while a WebAssembly agent is running which could access the memory, except from within
 *          the context of a host call on such an agent. Only shared memories are safe to access
 *          while WebAssembly code is running.
 */
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

    /**
     * \brief Gets a pointer to the internal information structure for this linear memory.
     *
     * The internal data structure contains basic information about this linear memory that must be
     * accessible to JITted code for performance reasons. This information is represented in a
     * standard-layout struct so that it can be read/written without needing to call C++ code.
     *
     * \warning This is not considered to be part of the public API of winter and is only for
     *          internal use by the VM. The layout of the internal struct is not stable and should
     *          not be relied upon.
     */
    MemoryInternal* internal() { return &_internal; }

    /**
     * \brief Gets the current size (in bytes) of this linear memory.
     *
     * The size of a linear memory can only ever be increased and can never decrease.
     */
    size_t size() const { return _internal.size; }

    /**
     * \brief Gets the current size (in WASM pages) of this linear memory.
     *
     * The size of a linear memory can only ever be increased and can never decrease.
     */
    NumPages size_pages() const { return NumPages(size() >> WASM_PAGE_SHIFT); }

    /**
     * \brief Gets the initial size (in WASM pages) of this linear memory.
     */
    NumPages initial_size_pages() const { return _initial_pages; }

    /**
     * \brief Gets the current capacity (in WASM pages) of this linear memory.
     *
     * The current capacity of a linear memory represents the maximum size to which the linear
     * memory can be grown without needing to allocate any new memory.
     */
    NumPages current_capacity_pages() const { return _internal.current_capacity_pages; }

    /**
     * \brief Gets the maximum capacity (in WASM pages) of this linear memory.
     */
    NumPages max_capacity_pages() const { return _internal.max_capacity_pages; }

    /**
     * \brief Checks whether this linear memory will never be reallocated.
     *
     * This method checks if the current capacity of this linear memory is equal to its maximum
     * capacity, meaning that the backing memory will never be reallocated.
     */
    bool is_at_max_capacity() const { return current_capacity_pages() == max_capacity_pages(); }

    /**
     * \brief Checks whether this linear memory can be shared between agents.
     */
    bool is_shared() const { return (_internal.flags & MEMORY_FLAG_SHARED) != 0; }

    /**
     * \brief Grows this linear memory by the given number of WASM pages.
     *
     * This method increases the size of this linear memory by the given number of WASM pages.
     *
     * If `size_pages() + new_pages` exceeds `max_capacity_pages()`, then the size of this linear
     * memory will not be grown and WASM_ALLOCATE_FAILURE will be returned.
     *
     * If `size_pages() + new_pages` exceeds `current_capacity_pages()`, then the backing memory
     * will be grown. This invalidates all pointers returned from Memory::data and Memory::ptr_to on
     * this linear memory. This is guaranteed never to happen on shared memories.
     *
     * \param[in] new_pages the number of new WASM pages that should be allocated.
     * \return the previous size of this linear memory in WASM pages or WASM_ALLOCATE_FAILURE.
     */
    NumPages grow(NumPages new_pages);

    /**
     * \brief Checks whether a given address is valid in this linear memory.
     *
     * This method determines whether a load or store of the given size at the given address would
     * be valid for this linear memory. Since a linear memory can only be grown and never shrunk, if
     * this method returns true for a given address and size, it is guaranteed to always return true
     * in the future for the same linear memory.
     *
     * \param[in] addr the address which should be checked.
     * \param[in] size the number of bytes after the given address that must be accessible.
     */
    bool is_valid_address(wptr_t addr, size_t size) const {
        size_t addr_s = static_cast<size_t>(addr);
        return (addr_s + size) >= addr_s && (addr_s + size) <= this->size();
    }

    /**
     * \brief Gets a pointer to an address in this linear memory.
     *
     * \warning Use of this function is strongly discouraged, as it can be quite difficult to use
     *          correctly. All pointers returned by this method are invalidated whenever a call to
     *          Memory::grow causes the backing memory to be reallocated.
     *
     * \param[in] addr the address to get a pointer to.
     * \param[in] size the size (in bytes) of the region that should be accessible by this pointer.
     */
    void* ptr_to(wptr_t addr, size_t size) {
        WASSERT(is_valid_address(addr, size), "Out-of-bounds address passed to Memory::ptr_to");
        return static_cast<void*>(static_cast<uint8_t*>(_internal.start) + static_cast<size_t>(addr));
    }

    /**
     * \brief Gets a pointer to an address in this linear memory.
     *
     * \warning Use of this function is strongly discouraged, as it can be quite difficult to use
     *          correctly. All pointers returned by this method are invalidated whenever a call to
     *          Memory::grow causes the backing memory to be reallocated.
     *
     * \param[in] addr the address to get a pointer to.
     * \param[in] size the size (in bytes) of the region that should be accessible by this pointer.
     */
    const void* ptr_to(wptr_t addr, size_t size) const {
        WASSERT(is_valid_address(addr, size), "Out-of-bounds address passed to Memory::ptr_to");
        return static_cast<const void*>(static_cast<uint8_t*>(_internal.start) + static_cast<size_t>(addr));
    }

    /**
     * \brief Gets a pointer to the start of the backing memory for this linear memory.
     *
     * \warning The pointer returned by this method is invalidated whenever a call to Memory::grow
     *          causes the backing memory to be reallocated.
     */
    void* data() { return _internal.start; }

    /**
     * \brief Gets a pointer to the start of the backing memory for this linear memory.
     *
     * \warning The pointer returned by this method is invalidated whenever a call to Memory::grow
     *          causes the backing memory to be reallocated.
     */
    const void* data() const { return _internal.start; }

    /**
     * \brief Attempts to read a number of bytes from this linear memory.
     *
     * \param[in] buf a pointer to a buffer to which data should be read.
     * \param[in] addr the address in this linear memory to start reading from.
     * \param[in] size the number of bytes to read.
     * \return true if the read was successful, false if the provided address was out-of-bounds.
     */
    bool load(void* buf, wptr_t addr, size_t size) const {
        if (is_valid_address(addr, size)) {
            std::memcpy(buf, ptr_to(addr, size), size);
            return true;
        } else {
            return false;
        }
    }

    /**
     * \brief Attempts to read a value from this linear memory.
     *
     * \warning This method is only intended to read values of a type that WebAssembly can write to
     *          linear memory in a single instruction (i.e. i32, i64, f32, and f64). There is no
     *          guarantee that the layout of any other type will match between the sandboxed
     *          WebAssembly code and the host environment, even if such a type is standard layout.
     *
     * \param[in] buf a pointer to which the value will be read.
     * \param[in] addr the address in this linear memory from which the value should be read.
     * \return true if the read was successful, false if the provided address was out-of-bounds.
     */
    template <typename T>
    bool load(T* buf, wptr_t addr) const {
        static_assert(std::is_standard_layout<T>::value, "Only standard layout types can be in WebAssembly linear memory");
        return load(static_cast<void*>(buf), addr, sizeof(T));
    }

    /**
     * \brief Attempts to write a number of bytes to this linear memory.
     *
     * \param[in] buf a pointer to a buffer containing the data to be written.
     * \param[in] addr the address in this linear memory to start writing to.
     * \param[in] size the number of bytes to write.
     * \return true if the write was successful, false if the provided address was out-of-bounds.
     */
    bool store(const void* buf, wptr_t addr, size_t size) {
        if (is_valid_address(addr, size)) {
            std::memcpy(ptr_to(addr, size), buf, size);
            return true;
        } else {
            return false;
        }
    }

    /**
     * \brief Attempts to write a value to this linear memory.
     *
     * \warning This method is only intended to write values of a type that WebAssembly can read
     *          from linear memory in a single instruction (i.e. i32, i64, f32, and f64). There is
     *          no guarantee that the layout of any other type will match between the sandboxed
     *          WebAssembly code and the host environment, even if such a type is standard layout.
     *
     * \param[in] buf a pointer to the value to be written.
     * \param[in] addr the address in this linear memory to which the value should be written.
     * \return true if the write was successful, false if the provided address was out-of-bounds.
     */
    template <typename T>
    bool store(const T* buf, wptr_t addr) {
        static_assert(std::is_standard_layout<T>::value, "Only standard layout types can be in WebAssembly linear memory");
        return store(static_cast<const void*>(buf), addr, sizeof(T));
    }

    /**
     * \brief Creates a new shared linear memory.
     *
     * \param[in] min the initial size of the linear memory (in WASM pages).
     * \param[in] max the maximum capacity of the linear memory (in WASM pages).
     */
    static std::shared_ptr<Memory> create_shared(NumPages min, NumPages max) {
        return std::shared_ptr<Memory>(new Memory(AbstractMemory(false, true, min, max)));
    }

    /**
     * \brief Creates a new unshared linear memory.
     *
     * \param[in] min the initial size of the linear memory (in WASM pages).
     * \param[in] max the maximum capacity of the linear memory (in WASM pages).
     */
    static std::shared_ptr<Memory> create_unshared(NumPages min, NumPages max) {
        return std::shared_ptr<Memory>(new Memory(AbstractMemory(false, false, min, max)));
    }
};

} // namespace winter

#endif
