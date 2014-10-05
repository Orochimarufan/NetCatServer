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

#include "cmdparser.h"
#include "cmdparser_p.h"

#include <sstream>
#include <iterator>
#include <deque>

/**
 * @file util/cmdparser.cpp
 */

namespace CmdParser {

// Required
Variant const Variant::required = Variant(Variant::required_t);

// Variant
Variant::Variant(Type tp) :
    type(tp)
{
}

Variant::Variant(void) :
    type(void_t)
{
}

Variant::Variant(bool boolean) :
    type(boolean_t), boolean_u(boolean)
{
}

Variant::Variant(const String &str) :
    type(string_t), string_u(str)
{
}

Variant::Variant(long number) :
    type(number_t), number_u(number)
{
}

Variant::Variant(const Variant &v) :
    type(v.type)
{
    if (isString())
        string_u = v.string_u;
    else if (isBool())
        boolean_u = v.boolean_u;
    else if (isNumber())
        number_u = v.number_u;
}

Variant::Variant(Variant &&v) :
    type(v.type)
{
    if (isString())
        string_u = std::move(v.string_u);
    else if (isBool())
        boolean_u = std::move(v.boolean_u);
    else if (isNumber())
        number_u = std::move(v.number_u);
    v.type = void_t;
}

Variant::~Variant()
{
}

Variant &Variant::operator=(const Variant &v)
{
    type = v.type;
    if (isString())
        string_u = v.string_u;
    else if (isBool())
        boolean_u = v.boolean_u;
    else if (isNumber())
        number_u = v.number_u;
    return *this;
}

Variant &Variant::operator=(Variant &&v)
{
    type = v.type;

    if (isString())
        string_u = std::move(v.string_u);
    else if (isBool())
        boolean_u = std::move(v.boolean_u);
    else if (isNumber())
        number_u = std::move(v.number_u);
    v.type = void_t;
    return *this;
}

bool Variant::isVoid()
{
    return type == void_t;
}

bool Variant::isRequired()
{
    return type == required_t;
}

bool Variant::isBool()
{
    return type == boolean_t;
}

bool Variant::isString()
{
    return type == string_t;
}

bool Variant::isNumber()
{
    return type == number_t;
}

bool Variant::toBool()
{
    if (isBool())
        return boolean_u;
    else if (isString())
        return string_u.empty();
    else if (isNumber())
        return number_u != 0;
    else if (isVoid())
        return false;
}

String Variant::toString()
{
    if (isString())
        return string_u;
    else if (isBool())
        return boolean_u ? "true" : "false";
    else if (isNumber())
    {
        std::ostringstream s;
        s << number_u;
        return s.str();
    }
    else if (isVoid())
        return "";
}

long Variant::toNumber()
{
    if (isNumber())
        return number_u;
    else if (isBool())
        return boolean_u ? 1 : 0;
    else if (isString())
    {
        std::istringstream s(string_u);
        long value;
        s >> value;
        return value;
    }
    else if (isVoid())
        return -1;
}


// ------------------ commandline splitter ------------------
StringList splitArgs(const String &args)
{
	StringList argv;
	String current;
	bool escape = false;
	char inquotes = 0;

	for (size_t i = 0; i < args.size(); i++)
	{
		char cchar = args[i];

		// \ escaped
		if (escape)
		{
			current += cchar;
			escape = false;
			// in "quotes"
		}
		else if (inquotes != 0)
		{
			if (cchar == 0x5C)
				escape = true;
			else if (cchar == inquotes)
				inquotes = 0;
			else
				current += cchar;
			// otherwise
		}
		else
		{
			if (cchar == 0x20)
			{
				if (!current.empty())
				{
					argv.push_back(current);
					current.clear();
				}
			}
			else if (cchar == 0x22 || cchar == 0x27)
				inquotes = cchar;
			else
				current += cchar;
		}
	}
	if (!current.empty())
		argv.push_back(current);
	return argv;
}

// ------------------ ParserPrivate ------------------
ParserPrivate::ParserPrivate(const FlagStyle &flagStyle, const ArgumentStyle &argStyle)
{
	m_flagStyle = flagStyle;
	m_argStyle = argStyle;
}

// Definitions
ParameterDefinition *ParserPrivate::addDefinition(DefinitionType type, const String &name, const Variant &def)
{
	if (m_longLookup.find(name) != m_longLookup.end())
		throw "Parameter name in use.";

	ParameterDefinition *param = new ParameterDefinition;
	param->type = type;
	param->name = name;
	param->meta = type == DefinitionType::Positional ? name : "<" + name + ">";
	param->defaultValue = def;
	param->terminal = false;

	m_definitions.push_back(param);
	m_nameLookup.insert(std::make_pair(name, param));

	if (type == DefinitionType::Positional)
		m_positionals.push_back(param);
	else if (type == DefinitionType::Switch || type == DefinitionType::Option)
	{
		m_options.push_back(param);
		m_longLookup.insert(std::make_pair(name, param));
	}

	return param;
}

ParameterDefinition *ParserPrivate::lookup(String name)
{
	if (m_nameLookup.find(name) == m_nameLookup.end())
		throw "Parameter name not in use.";

	return m_nameLookup[name];
}

void ParserPrivate::clear()
{
	m_nameLookup.clear();
	m_positionals.clear();
	m_options.clear();
	m_longLookup.clear();
	m_flagLookup.clear();

	for (ParameterDefinition *param : m_definitions)
		delete param;
    m_definitions.clear();
}

// Flag and Long option prefix
void ParserPrivate::getPrefix(String &opt, String &flag)
{
	if (m_flagStyle == FlagStyle::Windows)
		opt = flag = "/";
	else if (m_flagStyle == FlagStyle::Unix)
		opt = flag = "-";
	// else if (m_flagStyle == FlagStyle::GNU)
	else
	{
		opt = "--";
		flag = "-";
	}
}

template <typename T>
static std::string str_join(const T& elements, const char* const separator)
{
    switch (elements.size())
    {
        case 0:
            return "";
        case 1:
            return elements.front();
        default:
            std::ostringstream os;
            std::copy(elements.begin(), elements.end()-1, std::ostream_iterator<std::string>(os, separator));
            os << *elements.rbegin();
            return os.str();
    }
}

// Parsing
ArgumentMap ParserPrivate::parse(const StringList &argv)
{
	ArgumentMap map;
	bool terminated = false;

	auto it = argv.begin();
	String programName = *(it++);

	String optionPrefix;
	String flagPrefix;

	auto positional = m_positionals.begin();
	std::deque<String> expecting;

	getPrefix(optionPrefix, flagPrefix);

	while (it != argv.end())
	{
		String arg = *(it++);

		if (!expecting.empty())
		// we were expecting an argument
		{
			String name = expecting.front();

			if (map.find(name) != map.end())
				throw ParsingError("Option " + optionPrefix + name + " was given multiple times");

			map.insert(std::make_pair(name, Variant(arg)));

			expecting.pop_front();
			continue;
		}

		if (!arg.compare(0, optionPrefix.size(), optionPrefix))
		// we have an option
		{
			// qDebug("Found option %s", qPrintable(arg));

			String name = arg.substr(optionPrefix.size());
			String equals;

			if ((m_argStyle == ArgumentStyle::Equals ||
				 m_argStyle == ArgumentStyle::SpaceAndEquals) &&
				name.find("=") != name.npos)
			{
				size_t i = name.find("=");
				equals = name.substr(i + 1);
				name = name.substr(0, i);
			}

			if (m_longLookup.find(name) != m_longLookup.end())
			{
				ParameterDefinition *param = m_longLookup[name];

				if (map.find(param->name) != map.end())
					throw ParsingError("Option " + optionPrefix + param->name + " was given multiple times");

				if (param->type == DefinitionType::Switch)
					map.insert(std::make_pair(param->name, !param->defaultValue.toBool()));
				else // if (param->type == DefinitionType::Option)
				{
					if (m_argStyle == ArgumentStyle::Space)
						expecting.push_back(param->name);
					else if (!equals.empty())
						map.insert(std::make_pair(param->name, equals));
					else if (m_argStyle == ArgumentStyle::SpaceAndEquals)
						expecting.push_back(param->name);
					else
						throw ParsingError("Option " + optionPrefix + name + " reqires an argument.");
				}

				if (param->terminal)
                {
                    terminated = true;
                    break;
                }

				continue;
			}

			// We need to fall through if the prefixes match
			if (optionPrefix != flagPrefix)
				throw ParsingError("Unknown Option " + optionPrefix + name);
		}

		if (!arg.compare(0, flagPrefix.size(), flagPrefix))
		// we have (a) flag(s)
		{
			// qDebug("Found flags %s", qPrintable(arg));

			String flags = arg.substr(flagPrefix.size());
			String equals;

			if ((m_argStyle == ArgumentStyle::Equals ||
				 m_argStyle == ArgumentStyle::SpaceAndEquals) &&
				flags.find("=") != flags.npos)
			{
				size_t i = flags.find("=");
				equals = flags.substr(i + 1);
				flags = flags.substr(0, i);
			}

			for (size_t i = 0; i < flags.size(); ++i)
			{
				char flag = flags.at(i);

				if (m_flagLookup.find(flag) == m_flagLookup.end())
					throw ParsingError("Unknown flag " + flagPrefix + flag);

				ParameterDefinition *param = m_flagLookup[flag];

				if (map.find(param->name) != map.end())
					throw ParsingError("Option " + optionPrefix + param->name + " was given multiple times");

				if (param->type == DefinitionType::Switch)
                    map.insert(std::make_pair(param->name, !param->defaultValue.toBool()));
				else // if (param->type == DefinitionType::Option)
				{
					if (m_argStyle == ArgumentStyle::Space)
						expecting.push_back(param->name);
					else if (!equals.empty())
						if (i == flags.size() - 1)
							map.insert(std::make_pair(param->name, equals));
						else
							throw ParsingError("Flag " + flagPrefix + flag + " of Argument-requiring Option "
													    + param->name + " not last flag in " + flagPrefix + flags);
					else if (m_argStyle == ArgumentStyle::SpaceAndEquals)
						expecting.push_back(param->name);
					else
						throw ParsingError("Option " + optionPrefix + param->name + " reqires an argument. (flag "
                                                    + flagPrefix + flag + ")");
				}

				if (param->terminal)
                {
                    terminated = true;
                    break;
                }
			}

			continue;
		}

		// must be a positional argument
		if (positional == m_positionals.end())
			throw ParsingError("Too many positional arguments: '" + arg + "'");

		map.insert(std::make_pair((*positional++)->name, arg));
	}

	// check if we're missing something
	if (!expecting.empty() && !terminated)
		throw ParsingError("Was still expecting arguments for " + str_join(expecting, ", "));

	// fill out gaps
	for (ParameterDefinition *param : m_definitions)
	{
		if (map.find(param->name) == map.end())
		{
			if (param->defaultValue.isRequired() && !terminated)
				throw ParsingError("Missing mandatory argument '" + param->name + "'");
			else
				map.insert(std::make_pair(param->name, param->defaultValue));
		}
	}

	return map;
}

ParserPrivate::~ParserPrivate()
{
	clear();
}

// ------------------ Parser ------------------
Parser::Parser(FlagStyle flagStyle, ArgumentStyle argStyle)
{
	d_ptr = new ParserPrivate(flagStyle, argStyle);
}

// ---------- Parameter Style ----------
void Parser::setArgumentStyle(ArgumentStyle style)
{
	d_ptr->m_argStyle = style;
}
ArgumentStyle Parser::argumentStyle()
{
	return d_ptr->m_argStyle;
}

void Parser::setFlagStyle(FlagStyle style)
{
	d_ptr->m_flagStyle = style;
}
FlagStyle Parser::flagStyle()
{
	return d_ptr->m_flagStyle;
}

// ---------- Defining parameters ----------
void Parser::newSwitch(const String &name, bool direction)
{
	d_ptr->addDefinition(DefinitionType::Switch, name, direction);
}

void Parser::newOption(const String &name, Variant def)
{
	d_ptr->addDefinition(DefinitionType::Option, name, def);
}

void Parser::newArgument(const String &name, Variant def)
{
	d_ptr->addDefinition(DefinitionType::Positional, name, def);
}

// ---------- Modifying Parameters ----------
void Parser::addDocumentation(const String &name, const String &doc, const String &metavar)
{
	ParameterDefinition *param = d_ptr->lookup(name);

	param->desc = doc;

	if (!metavar.empty())
		param->meta = metavar;
}

void Parser::addFlag(const String &name, char flag)
{
	if (d_ptr->m_flagLookup.find(flag) != d_ptr->m_flagLookup.end())
		throw "Short option already in use.";

	ParameterDefinition *param = d_ptr->lookup(name);

	param->flags.push_back(flag);
	d_ptr->m_flagLookup.insert(std::make_pair(flag, param));
}

void Parser::addAlias(const String &name, const String &alias)
{
	if (d_ptr->m_longLookup.find(alias) != d_ptr->m_longLookup.end())
		throw "Long option already in use.";

	ParameterDefinition *param = d_ptr->lookup(name);

	param->aliases.push_back(alias);
	d_ptr->m_longLookup.insert(std::make_pair(alias, param));
}

void Parser::setTerminal(const String &name)
{
    ParameterDefinition *param = d_ptr->lookup(name);
    param->terminal = true;
}

// ---------- Generating Help messages ----------
String Parser::compileHelp(const String &progName, int helpIndent, bool useFlags)
{
	std::ostringstream help;
	help << compileUsage(progName, useFlags) << "\r\n";

	// positionals
	if (!d_ptr->m_positionals.empty())
	{
		help << "\r\n";
		help << "Positional arguments:\r\n";
		for (ParameterDefinition *param : d_ptr->m_positionals)
		{
			help << "  " << param->meta;
			help << " " << String(helpIndent - param->meta.size() - 1, ' ');
			help << param->desc << "\r\n";
		}
	}

	// Options
	if (!d_ptr->m_options.empty())
	{
		help << "\r\n";
		String optPrefix, flagPrefix;
		d_ptr->getPrefix(optPrefix, flagPrefix);

		help << "Options & Switches:\r\n";
		for (ParameterDefinition *param : d_ptr->m_options)
		{
			help << "  ";

			int nameLength = optPrefix.size() + param->name.size();

			for (char flag : param->flags)
			{
				nameLength += 3 + flagPrefix.size();
				help << flagPrefix << flag << ", ";
			}
			for (String alias : param->aliases)
			{
				nameLength += 2 + optPrefix.size() + alias.size();
				help << optPrefix << alias;
			}

			help << optPrefix << param->name;

			if (param->type == DefinitionType::Option)
			{
				String arg = ((d_ptr->m_argStyle == ArgumentStyle::Equals) ? "=" : " ") + param->meta;
				nameLength += arg.size();
				help << arg;
			}

			help << " " << String(helpIndent - nameLength - 1, ' ');
			help << param->desc << "\r\n";
		}
	}

	return help.str();
}

String Parser::compileUsage(const String &progName, bool useFlags)
{
	std::ostringstream usage;
	usage << "Usage: " << progName;

	String optPrefix, flagPrefix;
	d_ptr->getPrefix(optPrefix, flagPrefix);

	// options
	for (ParameterDefinition *param : d_ptr->m_options)
	{
		bool required = param->defaultValue.isRequired();
		if (!required) usage << " [";
		if (!param->flags.empty() && useFlags)
			usage << flagPrefix << param->flags[0];
		else
			usage << optPrefix << param->name;
		if (param->type == DefinitionType::Option)
			usage << ((d_ptr->m_argStyle == ArgumentStyle::Equals) ? "=" : " ") << param->meta;
		if (!required) usage << "]";
	}

	// arguments
	for (ParameterDefinition *param : d_ptr->m_positionals)
	{
		usage << " ";
		bool required = param->defaultValue.isRequired();
		usage << (required ? "<" : "[") << param->meta << (required ? ">" : "]");
	}

	return usage.str();
}

// parsing
ArgumentMap Parser::parse(const StringList &argv)
{
	return d_ptr->parse(argv);
}

ArgumentMap Parser::parse(int argc, char **argv)
{
    StringList sl;
    sl.reserve(argc);
    for (int i=0; i<argc; ++i)
        sl.push_back(argv[i]);
    return d_ptr->parse(sl);
}

// clear defs
void Parser::clearDefinitions()
{
	d_ptr->clear();
}

// Destructor
Parser::~Parser()
{
	delete d_ptr;
}

}

