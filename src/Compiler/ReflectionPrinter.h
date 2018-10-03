/*
 * ReflectionPrinter.h
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_REFLECTION_PRINTER_H
#define XSC_REFLECTION_PRINTER_H


#include <Xsc/IndentHandler.h>
#include <Xsc/Reflection.h>
#include <ostream>


namespace Xsc
{


class ReflectionPrinter
{
    
    public:
        
        ReflectionPrinter(std::ostream& output);

        void PrintReflection(const Reflection::ReflectionData& reflectionData);

    private:

        std::ostream& IndentOut();

        void PrintReflectionObjects(const std::vector<Reflection::BindingSlot>& objects, const std::string& title);
        void PrintReflectionObjects(const std::vector<std::string>& idents, const std::string& title);
        void PrintReflectionObjects(const std::map<std::string, Reflection::SamplerState>& samplerStates, const std::string& title);
        void PrintReflectionObject(const Reflection::RasterizerState& state, const std::string& title);
        void PrintReflectionObject(const Reflection::DepthState& state, const std::string& title);
        void PrintReflectionObject(const Reflection::StencilState& state, const std::string& title);
        void PrintReflectionObject(const Reflection::BlendState& state, const std::string& title);
        void PrintReflectionObject(const Reflection::BlendStateTarget& state);
        void PrintReflectionObject(const Reflection::StencilOperation& state);
        void PrintReflectionObject(const Reflection::BlendOperation& state);
        void PrintReflectionAttribute(const Reflection::NumThreads& numThreads, const std::string& title);

        std::ostream&   output_;
        IndentHandler   indentHandler_;

};


} // /namespace Xsc


#endif



// ================================================================================