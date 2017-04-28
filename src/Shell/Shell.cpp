/*
 * Shell.cpp
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "Shell.h"
#include "Helper.h"
#include "CommandFactory.h"
#include <Xsc/ConsoleManip.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <conio.h>
#endif


namespace Xsc
{

namespace Util
{


#ifdef XSC_ENABLE_EASTER_EGGS

static void PrintBackdoorEasterEgg(std::ostream& output)
{
    output << "here is your backdoor :-)" << std::endl;
    output << " _____ " << std::endl;
    output << "| ___ |" << std::endl;
    output << "||___||" << std::endl;
    output << "|   ~o|" << std::endl;
    output << "|     |" << std::endl;
    output << "|_____|" << std::endl;
    output << "-------" << std::endl;
}

#endif

Shell* Shell::instance_ = nullptr;

Shell::Shell(std::ostream& output) :
    output{ output }
{
    Shell::instance_ = this;
}

Shell::~Shell()
{
    Shell::instance_ = nullptr;
}

Shell* Shell::Instance()
{
    return instance_;
}

bool Shell::ExecuteCommandLine(CommandLine& cmdLine, bool enableBriefHelp)
{
    if (cmdLine.ReachedEnd())
    {
        /* Print brief help (if enabled) */
        if (enableBriefHelp)
        {
            CommandFactory::Instance().GetHelpPrinter().PrintHelpReference(output);
            return true;
        }
        return false;
    }

    try
    {
        /* Pares all arguments from command line */
        while (!cmdLine.ReachedEnd())
        {
            /* Get next command */
            auto cmdName = cmdLine.Accept();

            #ifdef XSC_ENABLE_EASTER_EGGS

            if (cmdName == "--backdoor")
            {
                PrintBackdoorEasterEgg(output);
                continue;
            }

            #endif

            Command::Identifier cmdIdent;
            if (auto cmd = CommandFactory::Instance().Get(cmdName, &cmdIdent))
            {
                /* Check if value is included within the command name */
                if (cmdIdent.includesValue)
                {
                    if (cmdName.size() > cmdIdent.name.size())
                        cmdLine.Insert(cmdName.substr(cmdIdent.name.size()));
                    else
                        throw std::invalid_argument("missing value in command '" + cmdIdent.name + "'");
                }

                /* Run command */
                cmd->Run(cmdLine, state_);
            }
            else
            {
                /* Compile specified shader file */
                Compile(cmdName);

                /* Reset output filename and entry point */
                state_.outputFilename.clear();
                state_.inputDesc.entryPoint.clear();
                state_.actionPerformed = true;
            }
        }

        if (!state_.actionPerformed)
        {
            /* No action performed -> return false */
            return false;
        }
    }
    catch (const std::exception& e)
    {
        /* Print highlighted exception info */
        {
            ConsoleManip::ScopedColor color { ConsoleManip::ColorFlags::Red | ConsoleManip::ColorFlags::Intens };
            output << "exception thrown: ";
        }

        /* Print error message */
        output << e.what() << std::endl;
    }

    return true;
}

void Shell::WaitForUser()
{
    /* Wait for user input (if enabled) */
    #ifdef _WIN32
    if (state_.pauseApp)
    {
        output << "press any key to continue ...";
        int i = _getch();
        output << std::endl;
    }
    #endif
}

void Shell::PushState()
{
    stateStack_.push(state_);
}

void Shell::PopState()
{
    state_ = stateStack_.top();
    stateStack_.pop();
}


/*
 * ======= Private: =======
 */

// Returns the filename without its file extension from the specified string.
static std::string GetFilePart(const std::string& s)
{
    const auto pos = s.find_last_of('.');
    return (pos == std::string::npos ? s : s.substr(0, pos));
}

// Returns the path without its filename from the specified string.
static std::string GetPathPart(const std::string& s)
{
    const auto pos = s.find_last_of("\\/");
    return (pos == std::string::npos ? "" : s.substr(0, pos));
}

static std::string TargetToExtension(const ShaderTarget shaderTarget)
{
    switch (shaderTarget)
    {
        case ShaderTarget::Undefined:                       break;
        case ShaderTarget::VertexShader:                    return "vert";
        case ShaderTarget::TessellationControlShader:       return "tesc";
        case ShaderTarget::TessellationEvaluationShader:    return "tese";
        case ShaderTarget::GeometryShader:                  return "geom";
        case ShaderTarget::FragmentShader:                  return "frag";
        case ShaderTarget::ComputeShader:                   return "comp";
    }
    return "glsl";
}

std::string Shell::GetDefaultOutputFilename(const std::string& filename) const
{
    return (GetFilePart(filename) + "." + state_.inputDesc.entryPoint + "." + TargetToExtension(state_.inputDesc.shaderTarget));
}

void Shell::Compile(const std::string& filename)
{
    lastOutputFilename_.clear();

    const auto  defaultOutputFilename   = GetDefaultOutputFilename(filename);
    auto        outputFilename          = state_.outputFilename;

    if (outputFilename.empty())
        outputFilename = defaultOutputFilename;
    else
        Replace(outputFilename, "*", defaultOutputFilename);

    try
    {
        /* Add pre-defined macros at the top of the input stream */
        auto inputStream = std::make_shared<std::stringstream>();
        
        for (const auto& macro : state_.predefinedMacros)
        {
            *inputStream << "#define " << macro.ident;
            if (!macro.value.empty())
                *inputStream << ' ' << macro.value;
            *inputStream << std::endl;
        }

        /* Open input stream */
        state_.inputDesc.filename = filename;

        std::ifstream inputFile(filename);
        if (!inputFile.good())
            throw std::runtime_error("failed to read file: \"" + filename + "\"");

        *inputStream << inputFile.rdbuf();

        std::stringstream outputStream;

        /* Initialize input and output descriptors */
        state_.inputDesc.sourceCode  = inputStream;
        state_.outputDesc.sourceCode = &outputStream;

        /* Final setup before compilation */
        StdLog                      log;
        IncludeHandler              includeHandler;
        Reflection::ReflectionData  reflectionData;
        
        includeHandler.searchPaths = state_.searchPaths;
        state_.inputDesc.includeHandler = &includeHandler;

        /* Add file path to include paths */
        const auto inputPath = GetPathPart(filename);
        if (!inputPath.empty())
            includeHandler.searchPaths.push_back(inputPath);

        /* Show compilation/validation status */
        if (state_.verbose)
        {
            if (state_.outputDesc.options.validateOnly)
                output << "validate \"" << filename << '\"' << std::endl;
            else
                output << "compile \"" << filename << "\" to \"" << outputFilename << '\"' << std::endl;
        }

        /* Compile shader file */
        auto result = CompileShader(
            state_.inputDesc,
            state_.outputDesc,
            &log,
            (state_.showReflection ? &reflectionData : nullptr)
        );

        /* Print all reports to the log output */
        log.PrintAll(state_.verbose);

        if (result)
        {
            if (!state_.outputDesc.options.validateOnly)
            {
                if (state_.verbose)
                    output << "compilation successful" << std::endl;

                /* Write result to output stream only on success */
                std::ofstream outputFile(outputFilename);
                if (outputFile.good())
                    outputFile << outputStream.rdbuf();
                else
                    throw std::runtime_error("failed to write file: \"" + outputFilename + "\"");

                /* Store output filename after successful compilation */
                lastOutputFilename_ = outputFilename;
            }
            else if (state_.verbose)
                output << "validation successful" << std::endl;
        }
        else
        {
            /* Always print message on failure */
            if (state_.outputDesc.options.validateOnly)
                output << "validation failed" << std::endl;
            else
                output << "compilation failed" << std::endl;
        }

        /* Show output statistics (if enabled) */
        if (state_.showReflection)
            PrintReflection(output, reflectionData);
    }
    catch (const std::exception& err)
    {
        /* Print error message */
        output << err.what() << std::endl;
    }
}


} // /namespace Util

} // /namespace Xsc



// ================================================================================
