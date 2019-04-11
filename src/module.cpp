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

#include <algorithm>
#include <sstream>

#include "module.hpp"

namespace winter {

Module::Module(const AbstractModule& abstract, Environment& env)
    : _imports(abstract.imports()), _exports(abstract.exports()), _memories(abstract.memories()), _env(&env) {
    _import_func_sigs.resize(abstract.funcs().size());
    std::transform(abstract.funcs().begin(), abstract.funcs().end(), _import_func_sigs.begin(),
        [&](const AbstractFunc& func) {
            return func.is_import ? &env.types().sig(func.sig) : nullptr;
        }
    );

    _funcs.resize(abstract.funcs().size());
    std::transform(abstract.funcs().begin(), abstract.funcs().end(), _funcs.begin(),
        [&](const AbstractFunc& func) {
            return func.is_import ? nullptr : UnlinkedFunc::instantiate(func, env);
        }
    );

    _shared_memories.resize(_memories.size());
    std::transform(_memories.begin(), _memories.end(), _shared_memories.begin(),
        [&](const AbstractMemory& mem) {
            return (mem.is_shared && !mem.is_import) ? std::shared_ptr<Memory>(new Memory(mem)) : nullptr;
        }
    );
}

LinkedFunc* ImportMultiModule::find_func(const Import& import) const {
    for (ImportModule* module : _modules) {
        LinkedFunc* func = module->find_func(import);
        if (func)
            return func;
    }

    return nullptr;
}

std::shared_ptr<Memory> ImportMultiModule::find_memory(const Import& import) const {
    for (ImportModule* module : _modules) {
        std::shared_ptr<Memory> mem = module->find_memory(import);
        if (mem)
            return mem;
    }

    return nullptr;
}

const Export* ModuleInstance::find_export(const Import& import) const {
    auto it = std::find_if(_exports.begin(), _exports.end(), [&](const Export& e) {
        return e.name == import.name;
    });

    return it != _exports.end() ? &*it : nullptr;
}

static const char* export_type_str(ExportType t) {
    switch (t) {
        case ExportType::Func:
            return "function";
        case ExportType::Table:
            return "table";
        case ExportType::Memory:
            return "memory";
        case ExportType::Global:
            return "global";
        default:
            return "???";
    }
}

LinkedFunc* ModuleInstance::find_func(const Import& import) const {
    const Export* e = find_export(import);

    if (!e)
        return nullptr;

    if (e->type != ExportType::Func) {
        throw LinkError(import, ([&]() {
            std::ostringstream ss;
            ss << "Imported " << import.module << "." << import.name << " has wrong type: expected function, but found "
               << export_type_str(e->type);
            return ss.str();
        })());
    }

    return _funcs[e->idx];
}

std::shared_ptr<Memory> ModuleInstance::find_memory(const Import& import) const {
    const Export* e = find_export(import);

    if (!e)
        return nullptr;

    if (e->type != ExportType::Memory) {
        throw LinkError(import, ([&]() {
            std::ostringstream ss;
            ss << "Imported " << import.module << "." << import.name << " has wrong type: expected memory, but found "
               << export_type_str(e->type);
            return ss.str();
        })());
    }

    return _memories[e->idx];
}

std::unique_ptr<ModuleInstance> ModuleInstance::instantiate(const Module& module, const ImportEnvironment& imports) {
    auto instance = std::unique_ptr<ModuleInstance>(new ModuleInstance());
    instance->_internal.init(module._memories.size(), module._funcs.size());
    instance->_exports = module._exports;
    instance->_env = module._env;

    instance->_funcs.resize(module._funcs.size());
    instance->_memories.resize(module._memories.size());

    for (const Import& i : module._imports) {
        switch (i.type) {
            case ExportType::Func: {
                WASSERT(i.idx < instance->_funcs.size(), "Import to out-of-bounds index");
                WASSERT(!instance->_funcs[i.idx], "Multiple imports to same index");

                LinkedFunc* func = imports.find_func(i);

                if (!func) {
                    throw LinkError(i, ([&]() {
                        std::ostringstream ss;
                        ss << "Imported function " << i.module << "." << i.name << " not found";
                        return ss.str();
                    })());
                } else if (&func->unlinked().signature() != module._import_func_sigs[i.idx]) {
                    throw LinkError(i, ([&]() {
                        std::ostringstream ss;
                        ss << "Imported function " << i.module << "." << i.name << " has wrong signature";
                        return ss.str();
                    })());
                }

                instance->_internal.func_table[i.idx] = func->internal();
                instance->_funcs[i.idx] = func;

                break;
            }
            case ExportType::Memory: {
                WASSERT(i.idx < instance->_memories.size(), "Import to out-of-bounds index");
                WASSERT(!instance->_memories[i.idx], "Multiple imports to same index");

                std::shared_ptr<Memory> mem = imports.find_memory(i);

                if (!mem) {
                    throw LinkError(i, ([&]() {
                        std::ostringstream ss;
                        ss << "Imported memory " << i.module << "." << i.name << " not found";
                        return ss.str();
                    })());
                } else if (mem->is_shared() != module._memories[i.idx].is_shared) {
                    if (mem->is_shared()) {
                        throw LinkError(i, ([&]() {
                            std::ostringstream ss;
                            ss << "Imported memory " << i.module << "." << i.name << " was shared, but was imported as unshared";
                            return ss.str();
                        })());
                    } else {
                        throw LinkError(i, ([&]() {
                            std::ostringstream ss;
                            ss << "Imported memory " << i.module << "." << i.name << " was unshared, but was imported as shared";
                            return ss.str();
                        })());
                    }
                } else if (mem->initial_size_pages() < module._memories[i.idx].initial_pages) {
                    throw LinkError(i, ([&]() {
                        std::ostringstream ss;
                        ss << "Imported memory " << i.module << "." << i.name << " is smaller than the import's minimum size ("
                           << mem->initial_size_pages().get() << " pages < " << module._memories[i.idx].initial_pages.get()
                           << " pages)";
                        return ss.str();
                    })());
                } else if (mem->max_capacity_pages() > module._memories[i.idx].max_pages) {
                    if (mem->max_capacity_pages() == WASM_UNLIMITED_PAGES) {
                        throw LinkError(i, ([&]() {
                            std::ostringstream ss;
                            ss << "Imported memory " << i.module << "." << i.name << " has a larger max size than the import's maximum size ("
                               << "unlimited pages > " << module._memories[i.idx].max_pages.get() << " pages)";
                            return ss.str();
                        })());
                    } else {
                        throw LinkError(i, ([&]() {
                            std::ostringstream ss;
                            ss << "Imported memory " << i.module << "." << i.name << " has a larger max size than the import's maximum size ("
                               << mem->max_capacity_pages().get() << " pages  > " << module._memories[i.idx].max_pages.get()
                               << " pages)";
                            return ss.str();
                        })());
                    }
                }

                instance->_internal.memory_table[i.idx] = mem->internal();
                instance->_memories[i.idx] = std::move(mem);

                break;
            }
            default:
                WASSERT(false, "Unknown import type");
        }
    }

    for (size_t i = 0; i < module._funcs.size(); i++) {
        const auto& func = module._funcs[i];

        if (func) {
            WASSERT(!instance->_funcs[i], "Import overwrote defined function");

            auto linked_func = LinkedFunc::instantiate(module._funcs[i], instance.get());

            instance->_internal.func_table[i] = linked_func->internal();
            instance->_funcs[i] = linked_func.get();
            instance->_owned_funcs.emplace_back(std::move(linked_func));
        } else {
            WASSERT(instance->_funcs[i], "Missing import for function");
        }
    }

    for (size_t i = 0; i < module._memories.size(); i++) {
        const auto& memory = module._memories[i];

        if (!memory.is_import) {
            WASSERT(!instance->_memories[i], "Import overwrote defined memory");
            if (memory.is_shared) {
                WASSERT(module._shared_memories[i], "Shared memory not created before module instantiation time");

                instance->_internal.memory_table[i] = module._shared_memories[i]->internal();
                instance->_memories[i] = module._shared_memories[i];
            } else {
                WASSERT(!module._shared_memories[i], "Unshared memory created before module instantiation time");

                auto mem = std::shared_ptr<Memory>(new Memory(memory));

                instance->_internal.memory_table[i] = mem->internal();
                instance->_memories[i] = std::move(mem);
            }
        } else {
            WASSERT(instance->_memories[i], "Missing import for memory");
        }
    }

    return instance;
}

} // namespace winter
