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

#include "gtest/gtest.h"
#include "../memory.hpp"

namespace {

TEST(MemoryTest, ConstructUnshared) {
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(3));

    ASSERT_EQ(1, mem->initial_size_pages().get());
    ASSERT_EQ(3, mem->max_capacity_pages().get());
    ASSERT_FALSE(mem->is_shared());
    ASSERT_NE(nullptr, mem->data());
}

TEST(MemoryTest, ConstructShared) {
    auto mem = winter::Memory::create_shared(winter::NumPages(1), winter::NumPages(3));

    ASSERT_EQ(1, mem->initial_size_pages().get());
    ASSERT_EQ(3, mem->max_capacity_pages().get());
    ASSERT_TRUE(mem->is_shared());
    ASSERT_NE(nullptr, mem->data());
}

TEST(MemoryTest, Size) {
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(3));

    ASSERT_EQ(winter::WASM_PAGE_SIZE, mem->size());
    ASSERT_EQ(1, mem->size_pages().get());
}

TEST(MemoryTest, LoadZeroed) {
    auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[winter::WASM_PAGE_SIZE]);
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(1));

    memset(buf.get(), -1, winter::WASM_PAGE_SIZE);
    ASSERT_TRUE(mem->load(buf.get(), 0, winter::WASM_PAGE_SIZE));

    for (size_t i = 0; i < winter::WASM_PAGE_SIZE; i++) {
        ASSERT_EQ(0, buf.get()[i]);
    }
}

TEST(MemoryTest, LoadStoreAligned64) {
    uint64_t val = 0xdeadbeefcafebabe;
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(1));

    ASSERT_TRUE(mem->store(&val, 0));
    val = 0;
    ASSERT_TRUE(mem->load(&val, 0));

    ASSERT_EQ(0xdeadbeefcafebabe, val);
}

TEST(MemoryTest, LoadStoreUnaligned64) {
    uint64_t val = 0xdeadbeefcafebabe;
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(1));

    ASSERT_TRUE(mem->store(&val, 3));
    val = 0;
    ASSERT_TRUE(mem->load(&val, 3));

    ASSERT_EQ(0xdeadbeefcafebabe, val);
}

TEST(MemoryTest, LoadStoreAligned32) {
    uint32_t val = 0xdeadbeef;
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(1));

    ASSERT_TRUE(mem->store(&val, 0));
    val = 0;
    ASSERT_TRUE(mem->load(&val, 0));

    ASSERT_EQ(0xdeadbeef, val);
}

TEST(MemoryTest, LoadStoreUnaligned32) {
    uint32_t val = 0xdeadbeef;
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(1));

    ASSERT_TRUE(mem->store(&val, 3));
    val = 0;
    ASSERT_TRUE(mem->load(&val, 3));

    ASSERT_EQ(0xdeadbeef, val);
}

TEST(MemoryTest, LoadStoreAligned16) {
    uint16_t val = 0xdead;
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(1));

    ASSERT_TRUE(mem->store(&val, 0));
    val = 0;
    ASSERT_TRUE(mem->load(&val, 0));

    ASSERT_EQ(0xdead, val);
}

TEST(MemoryTest, LoadStoreUnaligned16) {
    uint16_t val = 0xdead;
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(1));

    ASSERT_TRUE(mem->store(&val, 3));
    val = 0;
    ASSERT_TRUE(mem->load(&val, 3));

    ASSERT_EQ(0xdead, val);
}

TEST(MemoryTest, LoadStore8) {
    uint8_t val = 0xde;
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(1));

    ASSERT_TRUE(mem->store(&val, 0));
    val = 0;
    ASSERT_TRUE(mem->load(&val, 0));

    ASSERT_EQ(0xde, val);
}

TEST(MemoryTest, LoadEndianness) {
    uint64_t val64;
    uint32_t val32;
    uint16_t val16;
    uint8_t val8 = 0xff;
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(1));

    ASSERT_TRUE(mem->store(&val8, 0));
    ASSERT_TRUE(mem->load(&val16, 0));
    ASSERT_EQ(0xff, val16);
    ASSERT_TRUE(mem->load(&val32, 0));
    ASSERT_EQ(0xff, val32);
    ASSERT_TRUE(mem->load(&val64, 0));
    ASSERT_EQ(0xff, val64);
}

TEST(MemoryTest, StoreEndianness) {
    uint64_t val64 = 0xff;
    uint32_t val32 = 0xff;
    uint16_t val16 = 0xff;
    uint8_t val8;
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(1));

    ASSERT_TRUE(mem->store(&val16, 0));
    ASSERT_TRUE(mem->load(&val8, 0));
    ASSERT_EQ(0xff, val8);
    ASSERT_TRUE(mem->store(&val32, 0));
    ASSERT_TRUE(mem->load(&val8, 0));
    ASSERT_EQ(0xff, val8);
    ASSERT_TRUE(mem->store(&val64, 0));
    ASSERT_TRUE(mem->load(&val8, 0));
    ASSERT_EQ(0xff, val8);
}

TEST(MemoryTest, BoundsCheck) {
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(3));

    ASSERT_TRUE(mem->is_valid_address(0, 4));
    ASSERT_TRUE(mem->is_valid_address(0, winter::WASM_PAGE_SIZE));
    ASSERT_FALSE(mem->is_valid_address(0, winter::WASM_PAGE_SIZE + 1));
    ASSERT_TRUE(mem->is_valid_address(winter::WASM_PAGE_SIZE - 4, 4));
    ASSERT_FALSE(mem->is_valid_address(winter::WASM_PAGE_SIZE - 3, 4));
    ASSERT_TRUE(mem->is_valid_address(winter::WASM_PAGE_SIZE, 0));
    ASSERT_FALSE(mem->is_valid_address(winter::WASM_PAGE_SIZE + 1, 0));
    ASSERT_FALSE(mem->is_valid_address(1, std::numeric_limits<size_t>::max()));
}

TEST(MemoryTest, GrowUnshared) {
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(3));

    ASSERT_EQ(1, mem->size_pages().get());
    ASSERT_EQ(1, mem->grow(winter::NumPages(0)).get());
    ASSERT_EQ(1, mem->grow(winter::NumPages(1)).get());
    ASSERT_EQ(2, mem->size_pages().get());
    ASSERT_EQ(winter::WASM_ALLOCATE_FAILURE.get(), mem->grow(winter::NumPages(2)).get());
    ASSERT_EQ(2, mem->size_pages().get());
    ASSERT_EQ(2, mem->grow(winter::NumPages(1)).get());
    ASSERT_EQ(3, mem->size_pages().get());
    ASSERT_EQ(winter::WASM_ALLOCATE_FAILURE.get(), mem->grow(winter::NumPages(1)).get());
    ASSERT_EQ(3, mem->grow(winter::NumPages(0)).get());

    ASSERT_EQ(1, mem->initial_size_pages().get());
}

TEST(MemoryTest, GrowUnsharedVeryLarge) {
    auto mem = winter::Memory::create_unshared(winter::NumPages(1), winter::NumPages(3));

    ASSERT_EQ(1, mem->size_pages().get());

    ASSERT_EQ(
        winter::WASM_ALLOCATE_FAILURE.get(),
        mem->grow(winter::NumPages(std::numeric_limits<size_t>::max())).get()
    );
    ASSERT_EQ(1, mem->size_pages().get());

    ASSERT_EQ(
        winter::WASM_ALLOCATE_FAILURE.get(),
        mem->grow(winter::NumPages(static_cast<size_t>(1) << (std::numeric_limits<size_t>::digits - 1))).get()
    );
    ASSERT_EQ(1, mem->size_pages().get());

    ASSERT_EQ(
        winter::WASM_ALLOCATE_FAILURE.get(),
        mem->grow(winter::NumPages(std::numeric_limits<size_t>::max() >> winter::WASM_PAGE_SHIFT)).get()
    );
    ASSERT_EQ(1, mem->size_pages().get());
}

}
