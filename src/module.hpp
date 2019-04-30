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

#ifndef WINTER_MODULE_HPP
#define WINTER_MODULE_HPP

#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "environment.hpp"
#include "func.hpp"
#include "memory.hpp"

namespace winter {

enum class ExportType : uint8_t {
    Func = 0x00,
    Table = 0x01,
    Memory = 0x02,
    Global = 0x03
};

struct Export {
    std::string name;
    ExportType type;
    size_t idx;

    Export(std::string name, ExportType type, size_t idx)
        : name(std::move(name)), type(type), idx(idx) {}
};

struct Import {
    std::string module;
    std::string name;
    ExportType type;
    size_t idx;

    Import(std::string module, std::string name, ExportType type, size_t idx)
        : module(std::move(module)), name(std::move(name)), type(type), idx(idx) {}
};

class AbstractModule {
    std::vector<Import> _imports;
    std::vector<Export> _exports;

    std::vector<AbstractMemory> _memories;
    std::vector<AbstractFunc> _funcs;
public:
    std::vector<Import>& imports() { return _imports; }
    const std::vector<Import>& imports() const { return _imports; }

    std::vector<Export>& exports() { return _exports; }
    const std::vector<Export>& exports() const { return _exports; }

    std::vector<AbstractMemory>& memories() { return _memories; }
    const std::vector<AbstractMemory>& memories() const { return _memories; }

    std::vector<AbstractFunc>& funcs() { return _funcs; }
    const std::vector<AbstractFunc>& funcs() const { return _funcs; }
};

class Module {
    std::vector<Import> _imports;
    std::vector<Export> _exports;

    std::vector<AbstractMemory> _memories;
    std::vector<std::shared_ptr<Memory>> _shared_memories;

    std::vector<const FuncSig*> _import_func_sigs;
    std::vector<std::shared_ptr<UnlinkedFunc>> _funcs;

    Environment* _env;
public:
    Module() {}
    Module(const AbstractModule& abstract, Environment& env);

    Environment& env() const { return *_env; }

    const std::vector<Import>& imports() const { return _imports; }
    void add_import(Import i) {
        _imports.emplace_back(i);
    }

    const std::vector<Export>& exports() const { return _exports; }
    std::vector<Export>& exports() { return _exports; }
    void add_export(Export e) {
        _exports.emplace_back(e);
    }

    const std::vector<AbstractMemory>& memories() const { return _memories; }
    void add_memory(AbstractMemory mem) {
        if (mem.is_shared && !mem.is_import) {
            _shared_memories.emplace_back(new Memory(mem));
        } else {
            _shared_memories.emplace_back(nullptr);
        }

        _memories.emplace_back(mem);
    }

    const std::vector<std::shared_ptr<UnlinkedFunc>>& funcs() const { return _funcs; }
    void add_func(std::shared_ptr<UnlinkedFunc> func) {
        _import_func_sigs.emplace_back(nullptr);
        _funcs.emplace_back(func);
    }
    void add_imported_func(FuncSig* sig) {
        _import_func_sigs.emplace_back(sig);
        _funcs.emplace_back(nullptr);
    }

    friend class ModuleInstance;
};

class ImportModule {
public:
    virtual ~ImportModule() {}

    virtual LinkedFunc* find_func(const Import& import) const = 0;
    virtual std::shared_ptr<Memory> find_memory(const Import& import) const = 0;
};

class ImportMultiModule : public ImportModule {
    std::vector<ImportModule*> _modules;
public:
    explicit ImportMultiModule(std::vector<ImportModule*>&& modules) : _modules(std::move(modules)) {}

    LinkedFunc* find_func(const Import& import) const override;
    std::shared_ptr<Memory> find_memory(const Import& import) const override;
};

class ImportEnvironment {
    std::map<std::string, ImportModule*> _modules;
public:
    void add_module(std::string name, ImportModule* module) {
        _modules.emplace(name, module);
    }

    ImportModule* find_module(const Import& import) const {
        auto it = _modules.find(import.module);

        return it != _modules.end() ? it->second : nullptr;
    }

    LinkedFunc* find_func(const Import& import) const {
        auto module = find_module(import);
        return module ? module->find_func(import) : nullptr;
    }

    std::shared_ptr<Memory> find_memory(const Import& import) const {
        auto module = find_module(import);
        return module ? module->find_memory(import) : nullptr;
    }
};

struct ModuleInstanceInternal {
    MemoryInternal** memory_table = nullptr;
    LinkedFuncInternal** func_table = nullptr;

    ModuleInstance* container;

    ~ModuleInstanceInternal() {
        delete[] memory_table;
        delete[] func_table;
    }

    void init(size_t num_memories, size_t num_funcs) {
        memory_table = new MemoryInternal*[num_memories];
        func_table = new LinkedFuncInternal*[num_funcs];
    }
};
static_assert(std::is_standard_layout<ModuleInstanceInternal>::value, "ModuleInstanceInternal must be standard layout");

class ModuleInstance : public ImportModule {
    ModuleInstanceInternal _internal;

    std::vector<Export> _exports;

    std::vector<std::unique_ptr<LinkedFunc>> _owned_funcs;
    std::vector<LinkedFunc*> _funcs;

    std::vector<std::shared_ptr<Memory>> _memories;

    Environment* _env;

    ModuleInstance() {
        _internal.container = this;
    }
    ModuleInstance(const ModuleInstance&) = delete;
    ModuleInstance(ModuleInstance&&) = delete;

    ModuleInstance& operator=(const ModuleInstance&) = delete;
    ModuleInstance& operator=(ModuleInstance&&) = delete;
public:
    ModuleInstanceInternal* internal() { return &_internal; }

    Environment& env() const { return *_env; }

    const std::vector<Export>& exports() const { return _exports; }

    const std::vector<LinkedFunc*>& funcs() const { return _funcs; }
    const std::vector<std::shared_ptr<Memory>>& memories() const { return _memories; }

    const Export* find_export(const Import& import) const;
    LinkedFunc* find_func(const Import& import) const override;
    std::shared_ptr<Memory> find_memory(const Import& import) const override;

    static std::unique_ptr<ModuleInstance> instantiate(
        const Module& module, const ImportEnvironment& imports
    );
};

class LinkError : public std::exception {
    Import _import;
    const std::string _msg;
public:
    LinkError(Import i, std::string msg) noexcept : _import(std::move(i)), _msg(std::move(msg)) {}

    virtual const char* what() const noexcept { return _msg.c_str(); }

    const Import& import() const noexcept { return _import; }
};

} // namespace winter

#endif
