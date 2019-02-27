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

#include "binary-reader.hpp"
#include "wassert.hpp"

#include <memory>
#include <string>
#include <vector>

#include "wabt/src/option-parser.h"

namespace winter {

struct Options {
    std::string modulePath;
};

} // namespace winter

static wabt::Features s_features; // static so we access at option processing time

static winter::Options parseOptions(int argc, char** argv) {
    winter::Options options;

    wabt::OptionParser parser("winter", "   executes a WebAssembly module\n");

    s_features.AddOptions(&parser);

    parser.AddArgument("filename",
                       wabt::OptionParser::ArgumentCount::One,
                       [&options](char const* arg) { options.modulePath = arg; });

    parser.Parse(argc, argv);

    return options;
}

static int ProgramMain(const winter::Options& options) {
    std::vector<uint8_t> data_buffer;

    auto result = wabt::ReadFile(options.modulePath, &data_buffer);
    WASSERT(result == wabt::Result::Ok, "Failed to load data from file");

    auto binaryReader = winter::BinaryReader {};
    const auto kReadDebugNames = true;
    const auto kStopOnFirstError = true;
    const auto kFailOnCustomSectionError = true;
    wabt::ReadBinaryOptions readerOptions(s_features,
                                          nullptr,
                                          kReadDebugNames,
                                          kStopOnFirstError,
                                          kFailOnCustomSectionError);
    result = ReadBinary(data_buffer.data(), data_buffer.size(), &binaryReader, readerOptions);
    WASSERT(result == wabt::Result::Ok, "Failed to read binary data");

    return 0;
}

int main(int argc, char** argv) {
    auto options = parseOptions(argc, argv);
    return ProgramMain(options);
}
