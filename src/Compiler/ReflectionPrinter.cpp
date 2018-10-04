/*
 * ReflectionPrinter.cpp
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "ReflectionPrinter.h"
#include "ReportIdents.h"
#include <algorithm>


namespace Xsc
{


ReflectionPrinter::ReflectionPrinter(std::ostream& output) :
    output_ { output }
{
}

void ReflectionPrinter::PrintReflection(const Reflection::ReflectionData& reflectionData)
{
    output_ << R_CodeReflection() << ':' << std::endl;
    indentHandler_.IncIndent();
    {
        PrintReflectionObjects  ( reflectionData.macros,           "Macros"            );
        PrintReflectionObjects  ( reflectionData.textures,         "Textures"          );
        PrintReflectionObjects  ( reflectionData.storageBuffers,   "Storage Buffers"   );
        PrintReflectionObjects  ( reflectionData.constantBuffers,  "Constant Buffers"  );
        PrintReflectionObjects  ( reflectionData.inputAttributes,  "Input Attributes"  );
        PrintReflectionObjects  ( reflectionData.outputAttributes, "Output Attributes" );
        PrintReflectionObjects  ( reflectionData.samplerStates,    "Sampler States"    );
        PrintReflectionObject   ( reflectionData.rasterizerState,  "Rasterizer state"  );
        PrintReflectionObject   ( reflectionData.depthState,       "Depth state"       );
        PrintReflectionObject   ( reflectionData.stencilState,     "Stencil state"     );
        PrintReflectionObject   ( reflectionData.blendState,       "Blend state"       );
        PrintReflectionObject   ( reflectionData.globalOptions,    "Global options"    );
        PrintReflectionAttribute( reflectionData.numThreads,       "Number of Threads" );
    }
    indentHandler_.DecIndent();
}


/*
 * ======= Private: =======
 */

std::ostream& ReflectionPrinter::IndentOut()
{
    output_ << indentHandler_.FullIndent();
    return output_;
}

void ReflectionPrinter::PrintReflectionObjects(const std::vector<Reflection::BindingSlot>& objects, const std::string& title)
{
    IndentOut() << title << ':' << std::endl;
    ScopedIndent indent(indentHandler_);

    if (!objects.empty())
    {
        /* Determine offset for right-aligned location index */
        int maxLocation = -1;
        for (const auto& obj : objects)
            maxLocation = std::max(maxLocation, obj.location);

        std::size_t maxLocationLen = std::to_string(maxLocation).size();

        /* Print binding points */
        for (const auto& obj : objects)
        {
            output_ << indentHandler_.FullIndent();
            if (maxLocation >= 0)
            {
                if (obj.location >= 0)
                    output_ << std::string(maxLocationLen - std::to_string(obj.location).size(), ' ') << obj.location << ": ";
                else
                    output_ << std::string(maxLocationLen, ' ') << "  ";
            }
            output_ << obj.ident << std::endl;
        }
    }
    else
        IndentOut() << "< none >" << std::endl;
}

void ReflectionPrinter::PrintReflectionObjects(const std::vector<std::string>& idents, const std::string& title)
{
    IndentOut() << title << ':' << std::endl;
    ScopedIndent indent(indentHandler_);

    if (!idents.empty())
    {
        for (const auto& i : idents)
            IndentOut() << i << std::endl;
    }
    else
        IndentOut() << "< none >" << std::endl;
}

void ReflectionPrinter::PrintReflectionObjects(const std::map<std::string, Reflection::SamplerState>& samplerStates, const std::string& title)
{
    IndentOut() << title << ':' << std::endl;
    ScopedIndent indent(indentHandler_);
    
    if (!samplerStates.empty())
    {
        for (const auto& it : samplerStates)
        {
            IndentOut() << it.first << std::endl;
            indentHandler_.IncIndent();
            {
                const auto& smpl = it.second;
                const auto& brdCol = smpl.borderColor;
                IndentOut() << "AddressU       = " << ToString(smpl.addressU) << std::endl;
                IndentOut() << "AddressV       = " << ToString(smpl.addressV) << std::endl;
                IndentOut() << "AddressW       = " << ToString(smpl.addressW) << std::endl;
                IndentOut() << "BorderColor    = { " << brdCol[0] << ", " << brdCol[1] << ", " << brdCol[2] << ", " << brdCol[3] << " }" << std::endl;
                IndentOut() << "ComparisonFunc = " << ToString(smpl.comparisonFunc) << std::endl;
                IndentOut() << "FilterMin      = " << ToString(smpl.filterMin) << std::endl;
                IndentOut() << "FilterMax      = " << ToString(smpl.filterMax) << std::endl;
                IndentOut() << "FilterMip      = " << ToString(smpl.filterMip) << std::endl;
                IndentOut() << "MaxAnisotropy  = " << smpl.maxAnisotropy << std::endl;
                IndentOut() << "MaxLOD         = " << smpl.maxLOD << std::endl;
                IndentOut() << "MinLOD         = " << smpl.minLOD << std::endl;
                IndentOut() << "MipLODBias     = " << smpl.mipLODBias << std::endl;
            }
            indentHandler_.DecIndent();
        }
    }
    else
        IndentOut() << "< none >" << std::endl;
}

void ReflectionPrinter::PrintReflectionObject(const Reflection::BlendOperation& state)
{
    IndentOut() << "Source          = " << ToString(state.source) << std::endl;
    IndentOut() << "Destination     = " << ToString(state.destination) << std::endl;
    IndentOut() << "Operation       = " << ToString(state.operation) << std::endl;
}

void ReflectionPrinter::PrintReflectionObject(const Reflection::StencilOperation& state)
{
    IndentOut() << "Fail            = " << ToString(state.fail) << std::endl;
    IndentOut() << "ZFail           = " << ToString(state.zfail) << std::endl;
    IndentOut() << "Pass            = " << ToString(state.pass) << std::endl;
    IndentOut() << "ComparisonFunc  = " << ToString(state.compareFunc) << std::endl;
}
 
void ReflectionPrinter::PrintReflectionObject(const Reflection::BlendStateTarget& state)
{
    IndentOut() << "Enabled         = " << state.enabled << std::endl;
    IndentOut() << "WriteMask       = " << state.writeMask << std::endl;
    IndentOut() << "Color" << std::endl;
    {
        ScopedIndent indent(indentHandler_);
        PrintReflectionObject(state.colorOp);
    }

    IndentOut() << "Alpha" << std::endl;
    {
        ScopedIndent indent(indentHandler_);
        PrintReflectionObject(state.alphaOp);
    }
}

void ReflectionPrinter::PrintReflectionObject(const Reflection::BlendState& state, const std::string& title)
{
    IndentOut() << title << ':' << std::endl;
    ScopedIndent indent(indentHandler_);
    
    IndentOut() << "AlphaToCoverage       = " << state.alphaToCoverage << std::endl;
    IndentOut() << "IndependantBlend      = " << state.independantBlend << std::endl;

    for(uint32_t i = 0; i < Reflection::BlendState::MAX_NUM_RENDER_TARGETS; i++)
    {
        IndentOut() << "Target " << i << std::endl;

        ScopedIndent indent(indentHandler_);
        PrintReflectionObject(state.targets[i]);
    }
}

void ReflectionPrinter::PrintReflectionObject(const Reflection::DepthState& state, const std::string& title)
{
    IndentOut() << title << ':' << std::endl;
    ScopedIndent indent(indentHandler_);
    
    IndentOut() << "ReadEnable       = " << state.readEnable << std::endl;
    IndentOut() << "WriteEnable      = " << state.writeEnable << std::endl;
    IndentOut() << "ComparisonFunc   = " << ToString(state.compareFunc) << std::endl;
    IndentOut() << "DepthBias        = " << state.depthBias << std::endl;
    IndentOut() << "ScaledDepthBias  = " << state.scaledDepthBias << std::endl;
    IndentOut() << "DepthClip        = " << state.depthClip << std::endl;
}

void ReflectionPrinter::PrintReflectionObject(const Reflection::RasterizerState& state, const std::string& title)
{
    IndentOut() << title << ':' << std::endl;
    ScopedIndent indent(indentHandler_);
    
    IndentOut() << "FillMode        = " << ToString(state.fillMode) << std::endl;
    IndentOut() << "CullMode        = " << ToString(state.cullMode) << std::endl;
    IndentOut() << "AALine          = " << state.antialisedLineEnable << std::endl;
    IndentOut() << "Multisample     = " << state.multisampleEnable << std::endl;
    IndentOut() << "Scissor         = " << state.scissorEnable << std::endl;
}

void ReflectionPrinter::PrintReflectionObject(const Reflection::StencilState& state, const std::string& title)
{
    IndentOut() << title << ':' << std::endl;
    ScopedIndent indent(indentHandler_);
    
    IndentOut() << "Enabled         = " << state.enabled << std::endl;
    IndentOut() << "Reference       = " << state.reference << std::endl;
    IndentOut() << "ReadMask        = " << state.readMask << std::endl;
    IndentOut() << "WriteMask       = " << state.writeMask << std::endl;

    {
        IndentOut() << "Back" << std::endl;

        ScopedIndent indent(indentHandler_);
        PrintReflectionObject(state.back);
    }

    {
        IndentOut() << "Front" << std::endl;

        ScopedIndent indent(indentHandler_);
        PrintReflectionObject(state.front);
    }
}

void ReflectionPrinter::PrintReflectionObject(const Reflection::GlobalOptions& state, const std::string& title)
{
    IndentOut() << title << ':' << std::endl;
    ScopedIndent indent(indentHandler_);
    
    IndentOut() << "SortMode            = " << ToString(state.sortMode) << std::endl;
    IndentOut() << "Separable           = " << state.separable << std::endl;
    IndentOut() << "Transparent         = " << state.transparent << std::endl;
    IndentOut() << "Forward             = " << state.forward << std::endl;
    IndentOut() << "Priority            = " << state.priority << std::endl;
}

void ReflectionPrinter::PrintReflectionAttribute(const Reflection::NumThreads& numThreads, const std::string& title)
{
    IndentOut() << title << ':' << std::endl;
    ScopedIndent indent(indentHandler_);

    IndentOut() << "X = " << numThreads.x << std::endl;
    IndentOut() << "Y = " << numThreads.y << std::endl;
    IndentOut() << "Z = " << numThreads.z << std::endl;
}


} // /namespace Xsc



// ================================================================================
