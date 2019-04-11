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

#ifndef WINTER_FUNC_HPP
#define WINTER_FUNC_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "environment.hpp"
#include "type.hpp"
#include "wassert.hpp"

namespace winter {

struct ModuleInstanceInternal;
class ModuleInstance;

struct UnlinkedFuncInternal;
class UnlinkedFunc;

struct LinkedFuncInternal;
class LinkedFunc;

using JitFunction = uint32_t (*)(LinkedFuncInternal* func);

class InstructionStream {
    std::vector<uint8_t> _stream;
public:
    explicit InstructionStream(std::vector<uint8_t>&& stream) : _stream(std::move(stream)) {}

    size_t size() const { return _stream.size(); }

    friend class InstructionCursor;
};

class InstructionCursor {
    const InstructionStream* _stream;
    const uint8_t* _cursor;

    const uint8_t* begin() const { return _stream->_stream.data(); }
    const uint8_t* end() const { return _stream->_stream.data() + _stream->size(); }
public:
    InstructionCursor(const InstructionStream* stream, size_t off) : _stream(stream) {
        WASSERT(off <= _stream->size(), "Instruction cursor out-of-bounds");
        _cursor = _stream->_stream.data() + off;
    }

    size_t offset() const { return _cursor - _stream->_stream.data(); }

    void jump_relative(ssize_t off) {
        WASSERT(off >= begin() - _cursor, "Instruction cursor out-of-bounds");
        WASSERT(off <= end() - _cursor, "Instruction cursor out-of-bounds");

        _cursor += off;
    }

    uint8_t read_u8() {
        WASSERT(_cursor != end(), "Instruction cursor out-of-bounds");

        return *(_cursor++);
    }
};

struct AbstractFunc {
    bool is_import;

    std::string debug_name;
    std::shared_ptr<const InstructionStream> instrs;
    FuncSig sig;

    AbstractFunc(bool is_import, std::string debug_name, std::shared_ptr<const InstructionStream> instrs, FuncSig sig)
        : is_import(is_import), debug_name(std::move(debug_name)), instrs(std::move(instrs)), sig(std::move(sig)) {}

    static AbstractFunc for_import(FuncSig sig) {
        return AbstractFunc(true, "", nullptr, std::move(sig));
    }
};

struct UnlinkedFuncInternal {
    JitFunction jit_fn = nullptr;
    const FuncSig* sig;

    UnlinkedFunc* container;
};
static_assert(std::is_standard_layout<UnlinkedFuncInternal>::value, "UnlinkedFuncInternal must be standard layout");

class UnlinkedFunc {
    UnlinkedFuncInternal _internal;
    std::string _debug_name;
    std::shared_ptr<const InstructionStream> _instrs;

    UnlinkedFunc() {
        _internal.container = this;
    }
    UnlinkedFunc(const UnlinkedFunc&) = delete;
    UnlinkedFunc(UnlinkedFunc&&) = delete;

    UnlinkedFunc& operator=(const UnlinkedFunc&) = delete;
    UnlinkedFunc& operator=(UnlinkedFunc&&) = delete;
public:
    UnlinkedFuncInternal* internal() { return &_internal; }

    const FuncSig& signature() const { return *_internal.sig; }
    const std::string& debug_name() const { return _debug_name; }
    std::shared_ptr<const InstructionStream> instrs() const { return _instrs; }

    static std::shared_ptr<UnlinkedFunc> instantiate(const AbstractFunc& func, Environment& env);
    static std::shared_ptr<UnlinkedFunc> mock(const FuncSig* sig) {
        auto func = std::shared_ptr<UnlinkedFunc>(new UnlinkedFunc());

        func->_internal.sig = sig;

        return func;
    }
};

struct LinkedFuncInternal {
    UnlinkedFuncInternal* unlinked;
    ModuleInstanceInternal* module;

    LinkedFunc* container;
};
static_assert(std::is_standard_layout<LinkedFuncInternal>::value, "LinkedFuncInternal must be standard layout");

class LinkedFunc {
    LinkedFuncInternal _internal;
    std::shared_ptr<UnlinkedFunc> _unlinked;
    ModuleInstance* _module;

    LinkedFunc() {
        _internal.container = this;
    }
    LinkedFunc(const LinkedFunc&) = delete;
    LinkedFunc(LinkedFunc&&) = delete;

    LinkedFunc& operator=(const LinkedFunc&) = delete;
    LinkedFunc& operator=(LinkedFunc&&) = delete;
public:
    LinkedFuncInternal* internal() { return &_internal; }

    UnlinkedFunc& unlinked() const { return *_unlinked; }
    ModuleInstance& module() const { return *_module; }

    static std::unique_ptr<LinkedFunc> instantiate(std::shared_ptr<UnlinkedFunc> unlinked, ModuleInstance* module);
    static std::unique_ptr<LinkedFunc> mock(const FuncSig* sig) {
        auto func = std::unique_ptr<LinkedFunc>(new LinkedFunc());
        auto unlinked = UnlinkedFunc::mock(sig);

        func->_internal.unlinked = unlinked->internal();
        func->_internal.module = nullptr;
        func->_unlinked = std::move(unlinked);
        func->_module = nullptr;

        return func;
    }
};

} // namespace winter

#endif
