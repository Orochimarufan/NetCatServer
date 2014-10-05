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

#include <exception>
#include <stdexcept>

#include <unordered_map>
#include <string>
#include <vector>

/**
 * @file util/cmdparser.h
 * @brief commandline parsing and processing utilities
 */

namespace CmdParser {

class Variant;

// Typedefs
typedef std::string String;
typedef std::vector<String> StringList;
typedef std::unordered_map<String, Variant> ArgumentMap;

// ------------------ Utility Functions ------------------
/**
 * @brief split a string into argv items like a shell would do
 * @param args the argument string
 * @return a std::vector containing all arguments
 */
StringList splitArgs(const String &args);

// ------------------ Parser Preference Enums ------------------
/**
 * @brief The FlagStyle enum
 * Defines how Flags and Options are detected
 */
enum class FlagStyle
{
	GNU,	 /**< --option and -o (GNU Style) */
	Unix,	/**< -option and -o  (Unix Style) */
	Windows, /**< /option and /o  (Windows Style) */
	// Defaults: Windows = Windows, Other = GNU
#ifdef WIN32
	Default = Windows
#else
	Default = GNU
#endif
};

/**
 * @brief The ArgumentStyle enum
 * Defines how arguments are mapped to Options
 */
enum class ArgumentStyle
{
	Space,		  /**< --option=value */
	Equals,		 /**< --option value */
	SpaceAndEquals, /**< --option[= ]value */
	// Defaults: Windows = Equals, Other = SpaceAndEquals
#ifdef WIN32
	Default = Equals
#else
	Default = SpaceAndEquals
#endif
};

// ------------------ Variant class   ------------------
class Variant
{
    enum Type
    {
        void_t,
        required_t,
        boolean_t,
        number_t,
        string_t
    };

    Type type;

    union {
        long number_u;
        bool boolean_u;
    };

    String string_u;

    explicit Variant(Type tp);

public:
    // Create
    static const Variant required;

    Variant(void);
    Variant(bool boolean);
    Variant(const String &str);
    Variant(long number);

    Variant(const Variant &v);
    Variant(Variant &&v);
    ~Variant();
    Variant &operator=(const Variant &v);
    Variant &operator=(Variant &&v);

    // Check
    bool isVoid();
    bool isRequired();
    bool isBool();
    bool isString();
    bool isNumber();

    // Get
    bool toBool();
    String toString();
    long toNumber();
};

// ------------------ Exception class ------------------
/**
 * @brief The ParsingError class
 */
class ParsingError : public std::runtime_error
{
    using runtime_error::runtime_error;
};

// ------------------ Parser class ------------------
/**
 * @brief Private part of the parser
 */
class ParserPrivate;

/**
 * @brief The Parser class
 */
class Parser
{
	ParserPrivate *d_ptr;
public:
	// ---------- Constructor ----------
	/**
	 * @brief Parser constructor
	 * @param flagStyle the FlagStyle to use in this Parser
	 * @param argStyle the ArgumentStyle to use in this Parser
	 */
	Parser(FlagStyle flagStyle = FlagStyle::Default,
		   ArgumentStyle argStyle = ArgumentStyle::Default);

	// ---------- Parameter Style ----------
	/**
	 * @brief set the flag style
	 * @param style
	 */
	void setFlagStyle(FlagStyle style);

	/**
	 * @brief get the flag style
	 * @return
	 */
	FlagStyle flagStyle();

	/**
	 * @brief set the argument style
	 * @param style
	 */
	void setArgumentStyle(ArgumentStyle style);

	/**
	 * @brief get the argument style
	 * @return
	 */
	ArgumentStyle argumentStyle();

	// ---------- Defining parameters ----------
	/**
	 * @brief define a boolean switch
	 * @param name the parameter name
	 * @param direction start off as true or false
	 */
	void newSwitch(const String &name, bool direction = false);

	/**
	 * @brief define an option that takes an additional argument
	 * @param name the parameter name
	 * @param def the default value. May be Variant::required.
	 */
	void newOption(const String &name, Variant def = Variant());

	/**
	 * @brief define a positional argument
	 * @param name the parameter name
	 * @param def the default value. May be Variant::required.
	 */
	void newArgument(const String &name, Variant def = Variant::required);

	// ---------- Modifying Parameters ----------
	/**
	 * @brief adds documentation to a Parameter
	 * @param name the parameter name
	 * @param metavar a string to be displayed as placeholder for the value
	 * @param doc a string containing the documentation
	 * Note: on positional arguments, metavar replaces the name as displayed.
	 *       on options , metavar replaces the value placeholder
	 */
	void addDocumentation(const String &name, const String &doc, const String &metavar = String());

	/**
	 * @brief adds a flag to an existing parameter
	 * @param name the (existing) parameter name
	 * @param flag the flag character
	 * @see addSwitch addOption
	 */
	void addFlag(const String &name, char flag);

	/**
	 * @brief adds an alias to an existing parameter
	 * @param name the (existing) parameter name
	 * @param alias the alias
	 * @see addSwitch addOption
	 */
	void addAlias(const String &name, const String &alias);

	/**
     * @brief mark a switch terminal
     * @param name the (existing) parameter name
     * A terminal switch terminates the processing.
     * Used for --help and --version etc
     */
	void setTerminal(const String &name);

	// ---------- Generating Help messages ----------
	/**
	 * @brief generate a help message
	 * @param progName the program name to use in the help message
	 * @param helpIndent how much the parameter documentation should be indented
	 * @param flagsInUsage whether we should use flags instead of options in the usage
	 * @return a help message
	 */
	String compileHelp(const String &progName, int helpIndent = 22, bool flagsInUsage = true);

	/**
	 * @brief generate a short usage message
	 * @param progName the program name to use in the usage message
	 * @param useFlags whether we should use flags instead of options
	 * @return a usage message
	 */
	String compileUsage(const String &progName, bool useFlags = true);

	// ---------- Parsing ----------
	/**
	 * @brief parse
	 * @param argv a std::vector containing the program ARGV
	 * @return an unordered_map mapping argument names to their values
	 */
	ArgumentMap parse(const StringList &argv);

    /**
	 * @brief parse
	 * @param argc number of program arguments
	 * @param argv pointer to the argument array
	 * @return an unordered_map mapping argument names to their values
	 */
	ArgumentMap parse(int argc, char **argv);

	// ---------- Destruction ----------
	/**
	 * @brief clear all definitions
	 */
	void clearDefinitions();

	~Parser();
};

}
