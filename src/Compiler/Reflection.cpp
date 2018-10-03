/*
 * Reflection.cpp
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include <Xsc/Reflection.h>
#include "ReflectionPrinter.h"
#include "ASTEnums.h"


namespace Xsc
{


XSC_EXPORT std::string ToString(const Reflection::Filter t)
{
    return FilterToString(t);
}

XSC_EXPORT std::string ToString(const Reflection::TextureAddressMode t)
{
    return TexAddressModeToString(t);
}

XSC_EXPORT std::string ToString(const Reflection::ComparisonFunc t)
{
    return CompareFuncToString(t);
}

XSC_EXPORT std::string ToString(const Reflection::BlendOpType t)
{
    return BlendOpTypeToString(t);
}

XSC_EXPORT std::string ToString(const Reflection::StencilOpType t)
{
    return StencilOpTypeToString(t);
}

XSC_EXPORT std::string ToString(const Reflection::FillMode t)
{
    return FillModeToString(t);
}

XSC_EXPORT std::string ToString(const Reflection::CullMode t)
{
    return CullModeToString(t);
}

XSC_EXPORT std::string ToString(const Reflection::BlendFactor t)
{
    return BlendFactorToString(t);
}

XSC_EXPORT std::string ToString(const Reflection::SortMode t)
{
    return SortModeToString(t);
}

XSC_EXPORT void PrintReflection(std::ostream& stream, const Reflection::ReflectionData& reflectionData)
{
    ReflectionPrinter printer(stream);
    printer.PrintReflection(reflectionData);
}


} // /namespace Xsc



// ================================================================================
