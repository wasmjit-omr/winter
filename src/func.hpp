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

/**
 * \brief Represents a stream of WebAssembly instructions.
 */
class InstructionStream {
    std::vector<uint8_t> _stream;
public:
    explicit InstructionStream(std::vector<uint8_t>&& stream) : _stream(std::move(stream)) {}

    /**
     * \brief Gets the size (in bytes) of this instruction stream.
     */
    size_t size() const { return _stream.size(); }

    friend class InstructionCursor;
};

/**
 * \brief Represents a cursor for reading from an InstructionStream.
 */
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

    /**
     * \brief Gets the cursor's current offset (in bytes) in the instruction stream.
     */
    size_t offset() const { return _cursor - _stream->_stream.data(); }

    /**
     * \brief Jumps this cursor the specified number of bytes forward or backward.
     */
    void jump_relative(ssize_t off) {
        WASSERT(off >= begin() - _cursor, "Instruction cursor out-of-bounds");
        WASSERT(off <= end() - _cursor, "Instruction cursor out-of-bounds");

        _cursor += off;
    }

    /**
     * \brief Reads the next byte at this cursor as a uint8_t.
     */
    uint8_t read_u8() {
        WASSERT(_cursor != end(), "Instruction cursor out-of-bounds");

        return *(_cursor++);
    }
};

/**
 * \brief Represents a WebAssembly function which has not yet been created.
 *
 * There are two different kinds of AbstractFunc:
 *
 * - An AbstractFunc with is_import set to false represents a WebAssembly function which will be
 *   created when instantiating a module.
 * - An AbstractFunc with is_import set to true represents a function which will be imported from
 *   another module.
 *
 * \see AbstractModule
 * \see UnlinkedFunc
 * \see LinkedFunc
 */
struct AbstractFunc {
    bool is_import; ///< Whether this function will be imported from another module.

    /**
     * \brief The debug name of this function, or empty string if no debug name was provided.
     *
     * For functions which will be imported from another module, i.e. have is_import set to true,
     * this value will not be used and the debug name will be read from the function which ends up
     * being linked into this slot. In that case, this field will be the empty string.
     */
    std::string debug_name;

    /**
     * \brief The WebAssembly instructions constituting this function.
     *
     * For functions which will be imported from another module, i.e. have is_import set to true,
     * this field will not be populated and will instead be set to nullptr.
     */
    std::shared_ptr<const InstructionStream> instrs;
    FuncSig sig; ///< The signature of this function.

    AbstractFunc(bool is_import, std::string debug_name, std::shared_ptr<const InstructionStream> instrs, FuncSig sig)
        : is_import(is_import), debug_name(std::move(debug_name)), instrs(std::move(instrs)), sig(std::move(sig)) {}

    /**
     * \brief Creates an AbstractFunc for a function which will be imported from another module.
     *
     * \param[in] sig the function signature that the function being imported must have.
     */
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

/**
 * \brief Represents a WebAssembly function which has been partially instantiated.
 *
 * \see AbstractFunc
 * \see LinkedFunc
 */
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
    /**
     * \brief Gets a pointer to the internal information structure for this function.
     *
     * The internal data structure contains basic information about this function that must be
     * accessible to JITted code for performance reasons. This information is represented in a
     * standard-layout struct so that it can be read/written without needing to call C++ code.
     *
     * \warning This is not considered to be part of the public API of winter and is only for
     *          internal use by the VM. The layout of the internal struct is not stable and should
     *          not be relied upon.
     */
    UnlinkedFuncInternal* internal() { return &_internal; }

    /**
     * \brief Gets the function signature for calling this function.
     */
    const FuncSig& signature() const { return *_internal.sig; }

    /**
     * \brief Gets the debug name for this function or an empty string if no name was provided.
     */
    const std::string& debug_name() const { return _debug_name; }

    /**
     * \brief Gets the instruction stream for the WebAssembly instructions in this function.
     */
    std::shared_ptr<const InstructionStream> instrs() const { return _instrs; }

    /**
     * \brief Creates an UnlinkedFunc from an AbstractFunc.
     *
     * This function creates an unlinked instance of the given WebAssembly function. Any linked
     * versions of the returned function can only be called from the context of the provided
     * WebAssembly sandbox.
     */
    static std::shared_ptr<UnlinkedFunc> instantiate(const AbstractFunc& func, Environment& env);

    static std::shared_ptr<UnlinkedFunc> create_mock(const FuncSig* sig) {
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

/**
 * \brief Represents a WebAssembly function which is part of a fully instantiated module.
 *
 * \see UnlinkedFunc
 * \see ModuleInstance
 */
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
    /**
     * \brief Gets a pointer to the internal information structure for this function.
     *
     * The internal data structure contains basic information about this function that must be
     * accessible to JITted code for performance reasons. This information is represented in a
     * standard-layout struct so that it can be read/written without needing to call C++ code.
     *
     * \warning This is not considered to be part of the public API of winter and is only for
     *          internal use by the VM. The layout of the internal struct is not stable and should
     *          not be relied upon.
     */
    LinkedFuncInternal* internal() { return &_internal; }

    /**
     * \brief Gets the UnlinkedFunc that this function was created from.
     */
    UnlinkedFunc& unlinked() const { return *_unlinked; }

    /**
     * \brief Gets the module instance that this function is part of.
     */
    ModuleInstance& module() const { return *_module; }

    /**
     * \brief Creates a new LinkedFunc from an UnlinkedFunc.
     *
     * This function creates a linked version of the provided UnlinkedFunc using the provided
     * ModuleInstance for linking.
     */
    static std::unique_ptr<LinkedFunc> instantiate(std::shared_ptr<UnlinkedFunc> unlinked, ModuleInstance* module);

    static std::unique_ptr<LinkedFunc> create_mock(const FuncSig* sig) {
        auto func = std::unique_ptr<LinkedFunc>(new LinkedFunc());
        auto unlinked = UnlinkedFunc::create_mock(sig);

        func->_internal.unlinked = unlinked->internal();
        func->_internal.module = nullptr;
        func->_unlinked = std::move(unlinked);
        func->_module = nullptr;

        return func;
    }
};

} // namespace winter

#endif
