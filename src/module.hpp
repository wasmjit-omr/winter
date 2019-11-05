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

/**
 * \brief Represents a type of export or import in a WebAssembly module.
 */
enum class ExportType : uint8_t {
    Func = 0x00, ///< An import/export for a function.
    Table = 0x01, ///< An import/export for a WebAssembly table.
    Memory = 0x02, ///< An import/export for a WebAssembly linear memory.
    Global = 0x03 ///< An import/export for a WebAssembly global.
};

/**
 * \brief Represents an export in a WebAssembly module.
 */
struct Export {
    std::string name; ///< The name of the export.
    ExportType type; ///< The type of object being exported.
    size_t idx; ///< The index of the exported object in the module's relevant table.

    Export(std::string name, ExportType type, size_t idx)
        : name(std::move(name)), type(type), idx(idx) {}
};

/**
 * \brief Represents an import in a WebAssembly module.
 */
struct Import {
    std::string module; ///< The name of the module to import from.
    std::string name; ///< The name of the export that should be imported.
    ExportType type; ///< The type of object to be imported.

    /**
     * \brief The index into which the imported object should be placed in this module's relevant
     *        table.
     */
    size_t idx;

    Import(std::string module, std::string name, ExportType type, size_t idx)
        : module(std::move(module)), name(std::move(name)), type(type), idx(idx) {}
};

/**
 * \brief Represents a module which has been type-checked but for which no runtime resources have
 *        been allocated.
 *
 * \see Module
 * \see ModuleInstance
 */
class AbstractModule {
    std::vector<Import> _imports;
    std::vector<Export> _exports;

    std::vector<AbstractMemory> _memories;
    std::vector<AbstractFunc> _funcs;
public:
    /**
     * \brief Gets a list of imports in this module.
     */
    std::vector<Import>& imports() { return _imports; }

    /**
     * \brief Gets a list of imports in this module.
     */
    const std::vector<Import>& imports() const { return _imports; }

    /**
     * \brief Gets a list of exports in this module.
     */
    std::vector<Export>& exports() { return _exports; }

    /**
     * \brief Gets a list of exports in this module.
     */
    const std::vector<Export>& exports() const { return _exports; }

    /**
     * \brief Gets a list of linear memories in this module.
     */
    std::vector<AbstractMemory>& memories() { return _memories; }

    /**
     * \brief Gets a list of linear memories in this module.
     */
    const std::vector<AbstractMemory>& memories() const { return _memories; }

    /**
     * \brief Gets a list of functions in this module.
     */
    std::vector<AbstractFunc>& funcs() { return _funcs; }

    /**
     * \brief Gets a list of functions in this module.
     */
    const std::vector<AbstractFunc>& funcs() const { return _funcs; }
};

/**
 * \brief Represents a module which has been type-checked and partially instantiated.
 *
 * Multiple module instances created from the same Module will share certain runtime structures
 * between each other:
 *
 * - Shared linear memories
 * - JIT-compiled code and other metadata for functions
 *
 * Partially instantiating a module will only create runtime data structures for such shared
 * resources.
 *
 * \see AbstractModule
 * \see ModuleInstance
 */
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

    /**
     * \brief Gets the WebAssembly environment this module can be used in.
     */
    Environment& env() const { return *_env; }

    /**
     * \brief Gets a list of unresolved imports in this module.
     */
    const std::vector<Import>& imports() const { return _imports; }

    /**
     * \brief Adds a new unresolved import to this module.
     */
    void add_import(Import i) {
        _imports.emplace_back(i);
    }

    /**
     * \brief Gets a list of exports in this module.
     */
    const std::vector<Export>& exports() const { return _exports; }

    /**
     * \brief Adds a new export to this module.
     */
    void add_export(Export e) {
        _exports.emplace_back(e);
    }

    /**
     * \brief Gets a list of linear memories in or imported by this module.
     */
    const std::vector<AbstractMemory>& memories() const { return _memories; }

    /**
     * \brief Adds a new linear memory to this module.
     *
     * If the linear memory being added is marked as being shared, memory for it will be allocated
     * immediately. Otherwise, no actual memory will be allocated until the module is instantiated.
     */
    void add_memory(AbstractMemory mem) {
        if (mem.is_shared && !mem.is_import) {
            _shared_memories.emplace_back(new Memory(mem));
        } else {
            _shared_memories.emplace_back(nullptr);
        }

        _memories.emplace_back(mem);
    }

    /**
     * \brief Gets a list of functions in or imported by this module.
     *
     * For functions being imported into this module, slots in this list will be set to nullptr as
     * placeholders.
     */
    const std::vector<std::shared_ptr<UnlinkedFunc>>& funcs() const { return _funcs; }

    /**
     * \brief Gets a list of function signatures for functions imported by this module.
     *
     * For functions in this module that are not imports, slots in this list will be set to nullptr.
     */
    const std::vector<const FuncSig*> import_func_sigs() const { return _import_func_sigs; }

    /**
     * \brief Adds a new function to this module.
     */
    void add_func(std::shared_ptr<UnlinkedFunc> func) {
        _import_func_sigs.emplace_back(nullptr);
        _funcs.emplace_back(func);
    }

    /**
     * \brief Adds a new placeholder function to this module to be used for an imported function.
     */
    void add_imported_func(FuncSig* sig) {
        _import_func_sigs.emplace_back(sig);
        _funcs.emplace_back(nullptr);
    }

    friend class ModuleInstance;
};

/**
 * \brief Represents an object which can be imported as a WebAssembly module.
 */
class ImportModule {
public:
    virtual ~ImportModule() {}

    /**
     * \brief Finds the function with a given name in this module or returns nullptr.
     * \throws LinkError if the export with the given name is not a function.
     */
    virtual LinkedFunc* find_func(const Import& import) const = 0;

    /**
     * \brief Finds the linear memory with a given name in this module or returns nullptr.
     * \throws LinkError if the export with the given name is not a linear memory.
     */
    virtual std::shared_ptr<Memory> find_memory(const Import& import) const = 0;
};

/**
 * \brief Represents a WebAssembly module which is a combination of other modules.
 *
 * When searching for exports, modules will be searched in the order in which they appear in the
 * list of modules. The first module which returns a valid export with the given name will be used.
 */
class ImportMultiModule : public ImportModule {
    std::vector<ImportModule*> _modules;
public:
    explicit ImportMultiModule(std::vector<ImportModule*>&& modules) : _modules(std::move(modules)) {}

    LinkedFunc* find_func(const Import& import) const override;
    std::shared_ptr<Memory> find_memory(const Import& import) const override;
};

/**
 * \brief Represents the set of WebAssembly modules presented to a module at link time.
 */
class ImportEnvironment {
    std::map<std::string, ImportModule*> _modules;
public:
    /**
     * \brief Adds a new module to the list of modules that are visible in this environment.
     *
     * \note If a module with the given name already exists, it will be overwritten by the new
     *       module. To combine multiple modules into one, consider using ImportMultiModule.
     */
    void add_module(std::string name, ImportModule* module) {
        _modules[name] = module;
    }

    /**
     * \brief Finds the module matching an import or returns nullptr if no such module exists.
     */
    ImportModule* find_module(const Import& import) const {
        auto it = _modules.find(import.module);

        return it != _modules.end() ? it->second : nullptr;
    }

    /**
     * \brief Finds the function matching an import or returns nullptr.
     * \throws LinkError if the export matching the given import is not a function.
     */
    LinkedFunc* find_func(const Import& import) const {
        auto module = find_module(import);
        return module ? module->find_func(import) : nullptr;
    }

    /**
     * \brief Finds the linear memory matching an import or returns nullptr.
     * \throws LinkError if the export matching the given import is not a linear memory.
     */
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

/**
 * \brief Represents a fully instantiated WebAssembly module which is ready for execution.
 *
 * \see Module
 */
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
    /**
     * \brief Gets a pointer to the internal information structure for this module instance.
     *
     * The internal data structure contains basic information about this module that must be
     * accessible to JITted code for performance reasons. This information is represented in a
     * standard-layout struct so that it can be read/written without needing to call C++ code.
     *
     * \warning This is not considered to be part of the public API of winter and is only for
     *          internal use by the VM. The layout of the internal struct is not stable and should
     *          not be relied upon.
     */
    ModuleInstanceInternal* internal() { return &_internal; }

    /**
     * \brief Gets the WebAssembly environment this module instance can be used in.
     */
    Environment& env() const { return *_env; }

    /**
     * \brief Gets a list of exports provided by this module instance.
     */
    const std::vector<Export>& exports() const { return _exports; }

    /**
     * \brief Gets a list of functions defined in or imported by this module instance.
     */
    const std::vector<LinkedFunc*>& funcs() const { return _funcs; }

    /**
     * \brief Gets a list of linear memories defined in or imported by this module instance.
     */
    const std::vector<std::shared_ptr<Memory>>& memories() const { return _memories; }

    /**
     * \brief Finds an exported item in this module instance corresponding to the given import.
     *
     * This function finds an item exported by this module instance that corresponds in name and
     * export type to the provided import. If no such export exists, nullptr is returned instead.
     */
    const Export* find_export(const Import& import) const;
    LinkedFunc* find_func(const Import& import) const override;
    std::shared_ptr<Memory> find_memory(const Import& import) const override;

    /**
     * \brief Fully instantiates and links a partially instantiated module.
     *
     * \warning All of the modules in the provided ImportEnvironment *must* belong to the same
     *          Environment as the given Module. Additionally, it is the responsibility of the
     *          caller to ensure that ModuleInstance objects are destroyed in the reverse order in
     *          which they were created to ensure that no ModuleInstance outlives a ModuleInstance
     *          that imports it. Failure to uphold these guarantees results in undefined behaviour.
     *
     * \throws LinkError if one of the imports in the module to instantiate cannot be satisfied in
     *                   the provided ImportEnvironment.
     */
    static std::unique_ptr<ModuleInstance> instantiate(
        const Module& module, const ImportEnvironment& imports
    );
};

/**
 * \brief Thrown when an error occurs when linking a WebAssembly module.
 */
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
