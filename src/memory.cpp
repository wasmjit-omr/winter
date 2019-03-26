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

#include "winter_config.h"

#ifdef WINTER_USE_MMAP
#include <sys/mman.h>
#endif

#include "memory.hpp"

namespace winter {

Memory::~Memory() {
#ifdef WINTER_USE_MMAP
    if (_data.start)
        munmap(_data.start, _data.current_capacity_pages.get() << WASM_PAGE_SHIFT);
#else
    std::free(_data.start);
#endif
}

bool Memory::alloc_exactly(NumPages num_pages) {
    WASSERT(!is_shared(), "Shared WebAssembly memory cannot be grown");
    WASSERT(num_pages >= _data.current_capacity_pages, "WebAssembly memory cannot be shrunk");
    WASSERT(num_pages <= _data.max_capacity_pages, "WebAssembly memory cannot grow beyond its max capacity");

    if (num_pages != _data.current_capacity_pages && num_pages != NumPages(0)) {
        size_t new_size = num_pages.get() << WASM_PAGE_SHIFT;

        // Detect when the allocation size is too large to fit in a size_t
        if (NumPages(new_size >> WASM_PAGE_SHIFT) != num_pages)
            return false;

#ifdef WINTER_USE_MMAP
        void* new_start = _data.start
            ? mremap(_data.start, _data.current_capacity_pages.get() << WASM_PAGE_SHIFT, new_size, MREMAP_MAYMOVE)
            : mmap(nullptr, new_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

        if (new_start == MAP_FAILED)
            return false;
#else
        void* new_start = std::realloc(_data.start, new_size);

        if (!new_start)
            return false;

        std::memset(
            static_cast<void*>(static_cast<uint8_t*>(new_start) + (_data.current_capacity_pages.get() << WASM_PAGE_SHIFT)),
            0,
            (num_pages - _data.current_capacity_pages).get() << WASM_PAGE_SHIFT
        );
#endif

        _data.start = new_start;
        _data.current_capacity_pages = NumPages(new_size >> WASM_PAGE_SHIFT);
    }

    return true;
}

bool Memory::alloc_at_least(NumPages num_pages) {
    if (num_pages <= _data.current_capacity_pages)
        return true;

    if (num_pages > _data.max_capacity_pages)
        return false;

    // TODO We should probably overallocate here
    return alloc_exactly(num_pages);
}

NumPages Memory::grow(NumPages new_pages) {
    NumPages old_size_pages = size_pages();
    if (new_pages == NumPages(0))
        return old_size_pages;

    if (is_shared()) {
        // TODO Implement this
        WASSERT(false, "Growing shared memory is not yet implemented");
    } else {
        NumPages new_size_pages = old_size_pages + new_pages;

        if (new_size_pages < old_size_pages || new_size_pages > max_capacity_pages())
            return WASM_ALLOCATE_FAILURE;

        if (new_size_pages > current_capacity_pages()) {
            if (!alloc_at_least(new_size_pages))
                return WASM_ALLOCATE_FAILURE;
        }

        _data.size = new_size_pages.get() << WASM_PAGE_SHIFT;
        return old_size_pages;
    }
}

} // namespace winter
