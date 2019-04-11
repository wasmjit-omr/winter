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
#include "../module.hpp"

namespace {

class MockImportModule : public winter::ImportModule {
    std::map<std::string, std::unique_ptr<winter::LinkedFunc>> _funcs;
    std::map<std::string, std::shared_ptr<winter::Memory>> _memories;
public:
    MockImportModule(
        std::map<std::string, std::unique_ptr<winter::LinkedFunc>> funcs,
        std::map<std::string, std::shared_ptr<winter::Memory>> memories
    ) : _funcs(std::move(funcs)), _memories(std::move(memories)) {}

    winter::LinkedFunc* find_func(const std::string& name) const {
        auto it = _funcs.find(name);

        return it != _funcs.end() ? it->second.get() : nullptr;
    }

    winter::LinkedFunc* find_func(const winter::Import& import) const override {
        return find_func(import.name);
    }

    std::shared_ptr<winter::Memory> find_memory(const std::string& name) const {
        auto it = _memories.find(name);

        return it != _memories.end() ? it->second : nullptr;
    }

    std::shared_ptr<winter::Memory> find_memory(const winter::Import& import) const override {
        return find_memory(import.name);
    }

    static MockImportModule for_func(std::string name, std::unique_ptr<winter::LinkedFunc> func) {
        std::map<std::string, std::unique_ptr<winter::LinkedFunc>> funcs;
        funcs.emplace(std::move(name), std::move(func));

        return MockImportModule(std::move(funcs), {});
    }

    static MockImportModule for_memory(std::string name, std::shared_ptr<winter::Memory> memory) {
        std::map<std::string, std::shared_ptr<winter::Memory>> memories;
        memories.emplace(std::move(name), std::move(memory));

        return MockImportModule({}, std::move(memories));
    }

    static MockImportModule empty() {
        return MockImportModule({}, {});
    }
};

TEST(ModuleTest, EmptyModule) {
    winter::Environment env;
    winter::AbstractModule amod;
    winter::Module mod(amod, env);

    ASSERT_TRUE(mod.imports().empty());
    ASSERT_TRUE(mod.exports().empty());
    ASSERT_TRUE(mod.funcs().empty());
    ASSERT_TRUE(mod.memories().empty());

    auto modi = winter::ModuleInstance::instantiate(mod, winter::ImportEnvironment());

    ASSERT_TRUE(modi->exports().empty());
    ASSERT_TRUE(modi->funcs().empty());
    ASSERT_TRUE(modi->memories().empty());
}

TEST(ModuleTest, ImportFunction) {
    winter::Environment env;
    winter::AbstractModule amod;

    amod.imports().emplace_back(winter::Import("mod", "func", winter::ExportType::Func, 0));
    amod.funcs().emplace_back(winter::AbstractFunc::for_import(winter::FuncSig()));

    winter::Module mod(amod, env);

    ASSERT_EQ(1, mod.imports().size());
    ASSERT_EQ("mod", mod.imports()[0].module);
    ASSERT_EQ("func", mod.imports()[0].name);
    ASSERT_EQ(winter::ExportType::Func, mod.imports()[0].type);
    ASSERT_EQ(0, mod.imports()[0].idx);

    ASSERT_EQ(1, mod.funcs().size());
    ASSERT_FALSE(mod.funcs()[0]);

    auto mock_imod = MockImportModule::for_func("func", winter::LinkedFunc::mock(&env.types().sig(winter::FuncSig())));
    winter::ImportEnvironment ienv;
    ienv.add_module("mod", &mock_imod);

    auto modi = winter::ModuleInstance::instantiate(mod, ienv);

    ASSERT_EQ(mock_imod.find_func("func"), modi->funcs()[0]);
    ASSERT_EQ(mock_imod.find_func("func")->internal(), modi->internal()->func_table[0]);
}

TEST(ModuleTest, ImportInvalidFunction) {
    winter::Environment env;
    winter::AbstractModule amod;

    amod.imports().emplace_back(winter::Import("mod", "func", winter::ExportType::Func, 0));
    amod.funcs().emplace_back(winter::AbstractFunc::for_import(
        winter::FuncSig({ winter::ValueType::i32(), winter::ValueType::i32() }, { winter::ValueType::i32(), winter::ValueType::i32() })
    ));

    winter::Module mod(amod, env);

    auto mock_imod = MockImportModule::empty();
    winter::ImportEnvironment ienv;
    ienv.add_module("mod", &mock_imod);

    try {
        winter::ModuleInstance::instantiate(mod, ienv);
        FAIL() << "Expected winter::LinkError";
    } catch (winter::LinkError& e) {
        ASSERT_EQ("mod", e.import().module);
        ASSERT_EQ("func", e.import().name);
        ASSERT_EQ(winter::ExportType::Func, e.import().type);
        ASSERT_EQ(0, e.import().idx);
    }

    mock_imod = MockImportModule::for_func("func", winter::LinkedFunc::mock(
        &env.types().sig({ winter::ValueType::i32(), winter::ValueType::i32() }, { winter::ValueType::i32() })
    ));

    try {
        winter::ModuleInstance::instantiate(mod, ienv);
        FAIL() << "Expected winter::LinkError";
    } catch (winter::LinkError& e) {
        ASSERT_EQ("mod", e.import().module);
        ASSERT_EQ("func", e.import().name);
        ASSERT_EQ(winter::ExportType::Func, e.import().type);
        ASSERT_EQ(0, e.import().idx);
    }

    mock_imod = MockImportModule::for_func("func", winter::LinkedFunc::mock(
        &env.types().sig({ winter::ValueType::i32() }, { winter::ValueType::i32(), winter::ValueType::i32() })
    ));

    try {
        winter::ModuleInstance::instantiate(mod, ienv);
        FAIL() << "Expected winter::LinkError";
    } catch (winter::LinkError& e) {
        ASSERT_EQ("mod", e.import().module);
        ASSERT_EQ("func", e.import().name);
        ASSERT_EQ(winter::ExportType::Func, e.import().type);
        ASSERT_EQ(0, e.import().idx);
    }

    mock_imod = MockImportModule::for_func("func", winter::LinkedFunc::mock(
        &env.types().sig({ winter::ValueType::f32(), winter::ValueType::i32() }, { winter::ValueType::i32(), winter::ValueType::i32() })
    ));

    try {
        winter::ModuleInstance::instantiate(mod, ienv);
        FAIL() << "Expected winter::LinkError";
    } catch (winter::LinkError& e) {
        ASSERT_EQ("mod", e.import().module);
        ASSERT_EQ("func", e.import().name);
        ASSERT_EQ(winter::ExportType::Func, e.import().type);
        ASSERT_EQ(0, e.import().idx);
    }

    mock_imod = MockImportModule::for_func("func", winter::LinkedFunc::mock(
        &env.types().sig({ winter::ValueType::i32(), winter::ValueType::i32() }, { winter::ValueType::f32(), winter::ValueType::i32() })
    ));

    try {
        winter::ModuleInstance::instantiate(mod, ienv);
        FAIL() << "Expected winter::LinkError";
    } catch (winter::LinkError& e) {
        ASSERT_EQ("mod", e.import().module);
        ASSERT_EQ("func", e.import().name);
        ASSERT_EQ(winter::ExportType::Func, e.import().type);
        ASSERT_EQ(0, e.import().idx);
    }
}

TEST(ModuleTest, ExportFunction) {
    winter::Environment env;
    winter::AbstractModule amod;

    auto instrs = std::shared_ptr<const winter::InstructionStream>(new winter::InstructionStream({}));

    amod.exports().emplace_back(winter::Export("func", winter::ExportType::Func, 0));
    amod.funcs().emplace_back(winter::AbstractFunc(false, "func", instrs, winter::FuncSig()));

    winter::Module mod(amod, env);

    ASSERT_EQ(1, mod.exports().size());
    ASSERT_EQ("func", mod.exports()[0].name);
    ASSERT_EQ(winter::ExportType::Func, mod.exports()[0].type);
    ASSERT_EQ(0, mod.exports()[0].idx);

    ASSERT_EQ(1, mod.funcs().size());
    ASSERT_TRUE(mod.funcs()[0]);
    ASSERT_EQ("func", mod.funcs()[0]->debug_name());
    ASSERT_EQ(instrs, mod.funcs()[0]->instrs());
    ASSERT_EQ(&env.types().sig(winter::FuncSig()), &mod.funcs()[0]->signature());

    auto modi = winter::ModuleInstance::instantiate(mod, winter::ImportEnvironment());

    ASSERT_EQ(1, modi->funcs().size());
    ASSERT_TRUE(modi->funcs()[0]);
    ASSERT_EQ(mod.funcs()[0].get(), &modi->funcs()[0]->unlinked());
    ASSERT_EQ(mod.funcs()[0]->internal(), modi->funcs()[0]->internal()->unlinked);
    ASSERT_EQ(modi.get(), &modi->funcs()[0]->module());
    ASSERT_EQ(modi->internal(), modi->funcs()[0]->internal()->module);
    ASSERT_EQ(modi->funcs()[0], modi->funcs()[0]->internal()->container);
    ASSERT_EQ(modi->funcs()[0]->internal(), modi->internal()->func_table[0]);
    ASSERT_EQ(modi->funcs()[0], modi->find_func(winter::Import("mod", "func", winter::ExportType::Func, 0)));
}

TEST(ModuleTest, ImportMemory) {
    winter::Environment env;
    winter::AbstractModule amod;

    amod.imports().emplace_back(winter::Import("mod", "mem", winter::ExportType::Memory, 0));
    amod.memories().emplace_back(winter::AbstractMemory::for_import(false, winter::NumPages(5), winter::NumPages(10)));

    winter::Module mod(amod, env);

    ASSERT_EQ(1, mod.imports().size());
    ASSERT_EQ("mod", mod.imports()[0].module);
    ASSERT_EQ("mem", mod.imports()[0].name);
    ASSERT_EQ(winter::ExportType::Memory, mod.imports()[0].type);
    ASSERT_EQ(0, mod.imports()[0].idx);

    ASSERT_EQ(1, mod.memories().size());
    ASSERT_EQ(true, mod.memories()[0].is_import);
    ASSERT_EQ(false, mod.memories()[0].is_shared);
    ASSERT_EQ(5, mod.memories()[0].initial_pages.get());
    ASSERT_EQ(10, mod.memories()[0].max_pages.get());

    auto mock_imod = MockImportModule::for_memory("mem", winter::Memory::create_unshared(winter::NumPages(5), winter::NumPages(10)));
    winter::ImportEnvironment ienv;
    ienv.add_module("mod", &mock_imod);

    auto modi = winter::ModuleInstance::instantiate(mod, ienv);

    ASSERT_EQ(1, modi->memories().size());
    ASSERT_EQ(mock_imod.find_memory("mem"), modi->memories()[0]);
    ASSERT_EQ(mock_imod.find_memory("mem")->internal(), modi->internal()->memory_table[0]);
}

TEST(ModuleTest, ImportUnsharedInvalidMemory) {
    winter::Environment env;
    winter::AbstractModule amod;

    amod.imports().emplace_back(winter::Import("mod", "mem", winter::ExportType::Memory, 0));
    amod.memories().emplace_back(winter::AbstractMemory::for_import(false, winter::NumPages(5), winter::NumPages(10)));

    winter::Module mod(amod, env);

    auto mock_imod = MockImportModule::empty();
    winter::ImportEnvironment ienv;
    ienv.add_module("mod", &mock_imod);

    try {
        winter::ModuleInstance::instantiate(mod, ienv);
        FAIL() << "Expected winter::LinkError";
    } catch (winter::LinkError& e) {
        ASSERT_EQ("mod", e.import().module);
        ASSERT_EQ("mem", e.import().name);
        ASSERT_EQ(winter::ExportType::Memory, e.import().type);
        ASSERT_EQ(0, e.import().idx);
    }

    mock_imod = MockImportModule::for_memory("mem", winter::Memory::create_shared(winter::NumPages(5), winter::NumPages(10)));

    try {
        winter::ModuleInstance::instantiate(mod, ienv);
        FAIL() << "Expected winter::LinkError";
    } catch (winter::LinkError& e) {
        ASSERT_EQ("mod", e.import().module);
        ASSERT_EQ("mem", e.import().name);
        ASSERT_EQ(winter::ExportType::Memory, e.import().type);
        ASSERT_EQ(0, e.import().idx);
    }

    mock_imod = MockImportModule::for_memory("mem", winter::Memory::create_unshared(winter::NumPages(5), winter::NumPages(11)));

    try {
        winter::ModuleInstance::instantiate(mod, ienv);
        FAIL() << "Expected winter::LinkError";
    } catch (winter::LinkError& e) {
        ASSERT_EQ("mod", e.import().module);
        ASSERT_EQ("mem", e.import().name);
        ASSERT_EQ(winter::ExportType::Memory, e.import().type);
        ASSERT_EQ(0, e.import().idx);
    }

    mock_imod = MockImportModule::for_memory("mem", winter::Memory::create_unshared(winter::NumPages(4), winter::NumPages(10)));  

    try {
        winter::ModuleInstance::instantiate(mod, ienv);
        FAIL() << "Expected winter::LinkError";
    } catch (winter::LinkError& e) {
        ASSERT_EQ("mod", e.import().module);
        ASSERT_EQ("mem", e.import().name);
        ASSERT_EQ(winter::ExportType::Memory, e.import().type);
        ASSERT_EQ(0, e.import().idx);
    }  
}

TEST(ModuleTest, ImportSharedInvalidMemory) {
    winter::Environment env;
    winter::AbstractModule amod;

    amod.imports().emplace_back(winter::Import("mod", "mem", winter::ExportType::Memory, 0));
    amod.memories().emplace_back(winter::AbstractMemory::for_import(true, winter::NumPages(5), winter::NumPages(10)));

    winter::Module mod(amod, env);

    auto mock_imod = MockImportModule::empty();
    winter::ImportEnvironment ienv;
    ienv.add_module("mod", &mock_imod);

    try {
        winter::ModuleInstance::instantiate(mod, ienv);
        FAIL() << "Expected winter::LinkError";
    } catch (winter::LinkError& e) {
        ASSERT_EQ("mod", e.import().module);
        ASSERT_EQ("mem", e.import().name);
        ASSERT_EQ(winter::ExportType::Memory, e.import().type);
        ASSERT_EQ(0, e.import().idx);
    }

    mock_imod = MockImportModule::for_memory("mem", winter::Memory::create_unshared(winter::NumPages(5), winter::NumPages(10)));

    try {
        winter::ModuleInstance::instantiate(mod, ienv);
        FAIL() << "Expected winter::LinkError";
    } catch (winter::LinkError& e) {
        ASSERT_EQ("mod", e.import().module);
        ASSERT_EQ("mem", e.import().name);
        ASSERT_EQ(winter::ExportType::Memory, e.import().type);
        ASSERT_EQ(0, e.import().idx);
    }

    mock_imod = MockImportModule::for_memory("mem", winter::Memory::create_shared(winter::NumPages(5), winter::NumPages(11)));

    try {
        winter::ModuleInstance::instantiate(mod, ienv);
        FAIL() << "Expected winter::LinkError";
    } catch (winter::LinkError& e) {
        ASSERT_EQ("mod", e.import().module);
        ASSERT_EQ("mem", e.import().name);
        ASSERT_EQ(winter::ExportType::Memory, e.import().type);
        ASSERT_EQ(0, e.import().idx);
    }

    mock_imod = MockImportModule::for_memory("mem", winter::Memory::create_shared(winter::NumPages(4), winter::NumPages(10)));  

    try {
        winter::ModuleInstance::instantiate(mod, ienv);
        FAIL() << "Expected winter::LinkError";
    } catch (winter::LinkError& e) {
        ASSERT_EQ("mod", e.import().module);
        ASSERT_EQ("mem", e.import().name);
        ASSERT_EQ(winter::ExportType::Memory, e.import().type);
        ASSERT_EQ(0, e.import().idx);
    }  
}

TEST(ModuleTest, ExportUnsharedMemory) {
    winter::Environment env;
    winter::AbstractModule amod;

    amod.exports().emplace_back(winter::Export("mem", winter::ExportType::Memory, 0));
    amod.memories().emplace_back(winter::AbstractMemory(false, false, winter::NumPages(3), winter::NumPages(5)));

    winter::Module mod(amod, env);

    ASSERT_EQ(1, mod.exports().size());
    ASSERT_EQ("mem", mod.exports()[0].name);
    ASSERT_EQ(winter::ExportType::Memory, mod.exports()[0].type);
    ASSERT_EQ(0, mod.exports()[0].idx);

    ASSERT_EQ(1, mod.memories().size());
    ASSERT_EQ(false, mod.memories()[0].is_import);
    ASSERT_EQ(false, mod.memories()[0].is_shared);
    ASSERT_EQ(3, mod.memories()[0].initial_pages.get());
    ASSERT_EQ(5, mod.memories()[0].max_pages.get());

    auto modi = winter::ModuleInstance::instantiate(mod, winter::ImportEnvironment());

    ASSERT_EQ(1, modi->memories().size());
    ASSERT_TRUE(modi->memories()[0]);
    ASSERT_EQ(false, modi->memories()[0]->is_shared());
    ASSERT_EQ(3, modi->memories()[0]->initial_size_pages().get());
    ASSERT_EQ(5, modi->memories()[0]->max_capacity_pages().get());
    ASSERT_EQ(modi->memories()[0]->internal(), modi->internal()->memory_table[0]);
    ASSERT_EQ(modi->memories()[0], modi->find_memory(winter::Import("mod", "mem", winter::ExportType::Memory, 0)));
}

TEST(ModuleTest, ExportSharedMemory) {
    winter::Environment env;
    winter::AbstractModule amod;

    amod.exports().emplace_back(winter::Export("mem", winter::ExportType::Memory, 0));
    amod.memories().emplace_back(winter::AbstractMemory(false, true, winter::NumPages(3), winter::NumPages(5)));

    winter::Module mod(amod, env);

    ASSERT_EQ(1, mod.exports().size());
    ASSERT_EQ("mem", mod.exports()[0].name);
    ASSERT_EQ(winter::ExportType::Memory, mod.exports()[0].type);
    ASSERT_EQ(0, mod.exports()[0].idx);

    ASSERT_EQ(1, mod.memories().size());
    ASSERT_EQ(false, mod.memories()[0].is_import);
    ASSERT_EQ(true, mod.memories()[0].is_shared);
    ASSERT_EQ(3, mod.memories()[0].initial_pages.get());
    ASSERT_EQ(5, mod.memories()[0].max_pages.get());

    auto modi = winter::ModuleInstance::instantiate(mod, winter::ImportEnvironment());

    ASSERT_EQ(1, modi->memories().size());
    ASSERT_TRUE(modi->memories()[0]);
    ASSERT_EQ(true, modi->memories()[0]->is_shared());
    ASSERT_EQ(3, modi->memories()[0]->initial_size_pages().get());
    ASSERT_EQ(5, modi->memories()[0]->max_capacity_pages().get());
    ASSERT_EQ(modi->memories()[0]->internal(), modi->internal()->memory_table[0]);
    ASSERT_EQ(modi->memories()[0], modi->find_memory(winter::Import("mod", "mem", winter::ExportType::Memory, 0)));
}

TEST(ModuleTest, UnsharedMemoryNotShared) {
    winter::Environment env;
    winter::AbstractModule amod;

    amod.memories().emplace_back(winter::AbstractMemory(false, false, winter::NumPages(1), winter::NumPages(2)));

    winter::Module mod(amod, env);
    auto modi0 = winter::ModuleInstance::instantiate(mod, winter::ImportEnvironment());
    auto modi1 = winter::ModuleInstance::instantiate(mod, winter::ImportEnvironment());

    ASSERT_TRUE(modi0->memories()[0]);
    ASSERT_TRUE(modi1->memories()[0]);
    ASSERT_NE(modi0->memories()[0], modi1->memories()[1]);
}

TEST(ModuleTest, SharedMemoryIsShared) {
    winter::Environment env;
    winter::AbstractModule amod;

    amod.memories().emplace_back(winter::AbstractMemory(false, true, winter::NumPages(1), winter::NumPages(2)));

    winter::Module mod(amod, env);
    auto modi0 = winter::ModuleInstance::instantiate(mod, winter::ImportEnvironment());
    auto modi1 = winter::ModuleInstance::instantiate(mod, winter::ImportEnvironment());

    ASSERT_TRUE(modi0->memories()[0]);
    ASSERT_TRUE(modi1->memories()[0]);
    ASSERT_EQ(modi0->memories()[0], modi1->memories()[0]);
}

}
