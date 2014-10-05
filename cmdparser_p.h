/* Copyright 2013 MultiMC Contributors
 *
 * Authors: Orochimarufan <orochimarufan.x3@gmail.com>
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
 * ------------------------------------------------------------------------
 * 2013-12-13 Commandline Parser Rewrite (Orochimarufan)
 * 2014-10-04 Convert to STL
 */

#pragma once

#include "cmdparser.h"

namespace CmdParser {

enum class DefinitionType : int
{
    None,
    Switch,
    Option,
    Positional
};

struct ParameterDefinition
{
    DefinitionType type;
    String name;

    std::vector<String> aliases;
    std::vector<char> flags;

    String meta;
    String desc;

    bool terminal;

    Variant defaultValue;
};

typedef std::vector<ParameterDefinition *> DefinitionList;
typedef std::unordered_map<String, ParameterDefinition *> DefinitionMap;

class ParserPrivate
{
    friend class Parser;

    FlagStyle m_flagStyle;
    ArgumentStyle m_argStyle;

    DefinitionList m_definitions;
    DefinitionMap m_nameLookup;

    DefinitionList m_positionals;
    DefinitionList m_options;

    DefinitionMap m_longLookup;
    std::unordered_map<char, ParameterDefinition *> m_flagLookup;

    // Methods
    ParserPrivate(const FlagStyle &flagStyle, const ArgumentStyle &argStyle);

    ParameterDefinition *addDefinition(DefinitionType type, const String &name, const Variant &def);
    ParameterDefinition *lookup(String name);

    void getPrefix(String &opt, String &flag);

    ArgumentMap parse(const StringList &argv);

    void clear();
    ~ParserPrivate();
};

}

