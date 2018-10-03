/*
 * ReflectionAnalyzer.cpp
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "ReflectionAnalyzer.h"
#include "ExprEvaluator.h"
#include "AST.h"
#include "Helper.h"
#include "ReportIdents.h"

// BEGIN BANSHEE CHANGES
#include <cstring>
// END BANSHEE CHANGES


namespace Xsc
{


ReflectionAnalyzer::ReflectionAnalyzer(Log* log) :
    reportHandler_ { log }
{
}

void ReflectionAnalyzer::Reflect(
    Program& program, const ShaderTarget shaderTarget, Reflection::ReflectionData& reflectionData, bool enableWarnings)
{
    /* Copy parameters */
    shaderTarget_   = shaderTarget;
    program_        = (&program);
    data_           = (&reflectionData);
    enableWarnings_ = enableWarnings;

    /* Visit program AST */
    Visit(program_);
}


/*
 * ======= Private: =======
 */

void ReflectionAnalyzer::Warning(const std::string& msg, const AST* ast)
{
    if (enableWarnings_)
        reportHandler_.Warning(false, msg, program_->sourceCode.get(), (ast ? ast->area : SourceArea::ignore));
}

void ReflectionAnalyzer::Error(const std::string& msg, const AST* ast)
{
    reportHandler_.SubmitReport(false, ReportTypes::Error, R_Error, msg, program_->sourceCode.get(), 
        (ast ? ast->area : SourceArea::ignore));
}

int ReflectionAnalyzer::GetBindingPoint(const std::vector<RegisterPtr>& slotRegisters) const
{
    if (auto slotRegister = Register::GetForTarget(slotRegisters, shaderTarget_))
        return slotRegister->slot;
    else
        return -1;
}

int ReflectionAnalyzer::EvaluateConstExprInt(Expr& expr)
{
    /* Evaluate expression and return as integer */
    ExprEvaluator exprEvaluator;
    return static_cast<int>(exprEvaluator.EvaluateOrDefault(expr, Variant::IntType(0)).ToInt());
}

float ReflectionAnalyzer::EvaluateConstExprFloat(Expr& expr)
{
    /* Evaluate expression and return as integer */
    ExprEvaluator exprEvaluator;
    return static_cast<float>(exprEvaluator.EvaluateOrDefault(expr, Variant::RealType(0.0)).ToReal());
}

/* ------- Visit functions ------- */

#define IMPLEMENT_VISIT_PROC(AST_NAME) \
    void ReflectionAnalyzer::Visit##AST_NAME(AST_NAME* ast, void* args)

IMPLEMENT_VISIT_PROC(Program)
{
    /* Visit both active and disabled code */
    Visit(ast->globalStmnts);
    Visit(ast->disabledAST);

    if (auto entryPoint = ast->entryPointRef)
    {
        /* Reflect input attributes */
        for (auto varDecl : entryPoint->inputSemantics.varDeclRefs)
            data_->inputAttributes.push_back({ varDecl->ident, varDecl->semantic.Index() });
        for (auto varDecl : entryPoint->inputSemantics.varDeclRefsSV)
            data_->inputAttributes.push_back({ varDecl->semantic.ToString(), varDecl->semantic.Index() });

        /* Reflect output attributes */
        for (auto varDecl : entryPoint->outputSemantics.varDeclRefs)
            data_->outputAttributes.push_back({ varDecl->ident, varDecl->semantic.Index() });
        for (auto varDecl : entryPoint->outputSemantics.varDeclRefsSV)
            data_->outputAttributes.push_back({ varDecl->semantic.ToString(), varDecl->semantic.Index() });
        
        if (entryPoint->semantic.IsSystemValue())
            data_->outputAttributes.push_back({ entryPoint->semantic.ToString(), entryPoint->semantic.Index() });
    }
}

/* --- Declarations --- */

IMPLEMENT_VISIT_PROC(SamplerDecl)
{
    /* Reflect sampler state */
    Reflection::SamplerState samplerState;
    {
        for (auto& value : ast->samplerValues)
        {
            ReflectSamplerValue(value.get(), samplerState);

            // BEGIN BANSHEE CHANGES
            samplerState.isNonDefault = true;
            // END BANSHEE CHANGES
        }
    }

    samplerState.alias = ast->alias;
    data_->samplerStates[ast->ident] = samplerState;

    // BEGIN BANSHEE CHANGES

    Reflection::Uniform uniform;
    uniform.ident = ast->ident;
    uniform.type = Reflection::VariableType::Sampler;
    uniform.baseType = 0;

    data_->uniforms.push_back(uniform);

    // END BANSHEE CHANGES
}

IMPLEMENT_VISIT_PROC(StateDecl)
{
    if(!ast->initializer)
        return;

    switch(ast->GetStateType())
    {
    case StateType::Rasterizer: 
        for(auto& value : ast->initializer->exprs)
            ReflectRasterizerStateValue(value.get(), data_->rasterizerState);
        break;
    case StateType::Depth:
        for(auto& value : ast->initializer->exprs)
            ReflectDepthStateValue(value.get(), data_->depthState);
        break;
    case StateType::Stencil:
        for(auto& value : ast->initializer->exprs)
            ReflectStencilStateValue(value.get(), data_->stencilState);
        break;
    case StateType::Blend:
    {
        uint32_t blendTargetIdx = 0;
        for(auto& value : ast->initializer->exprs)
            ReflectBlendStateValue(value.get(), data_->blendState, blendTargetIdx);
        break;
    }
    case StateType::Options:
        for(auto& value : ast->initializer->exprs)
            ReflectOptionsStateValue(value.get(), data_->globalOptions);
        break;
    default:
    case StateType::Undefined: 
        break;
    }
}

/* --- Declaration statements --- */

// BEGIN BANSHEE CHANGES

Reflection::DataType DataTypeToReflType(DataType dataType)
{
#define CONVERSION_ENTRY(type) case DataType::type: return Reflection::DataType::type;

    switch (dataType)
    {
    CONVERSION_ENTRY(Bool)
    CONVERSION_ENTRY(Int)
    CONVERSION_ENTRY(UInt)
    CONVERSION_ENTRY(Half)
    CONVERSION_ENTRY(Float)
    CONVERSION_ENTRY(Double)
    CONVERSION_ENTRY(Bool2)
    CONVERSION_ENTRY(Bool3)
    CONVERSION_ENTRY(Bool4)
    CONVERSION_ENTRY(Int2)
    CONVERSION_ENTRY(Int3)
    CONVERSION_ENTRY(Int4)
    CONVERSION_ENTRY(UInt2)
    CONVERSION_ENTRY(UInt3)
    CONVERSION_ENTRY(UInt4)
    CONVERSION_ENTRY(Half2)
    CONVERSION_ENTRY(Half3)
    CONVERSION_ENTRY(Half4)
    CONVERSION_ENTRY(Float2)
    CONVERSION_ENTRY(Float3)
    CONVERSION_ENTRY(Float4)
    CONVERSION_ENTRY(Double2)
    CONVERSION_ENTRY(Double3)
    CONVERSION_ENTRY(Double4)
    CONVERSION_ENTRY(Bool2x2)
    CONVERSION_ENTRY(Bool2x3)
    CONVERSION_ENTRY(Bool2x4)
    CONVERSION_ENTRY(Bool3x2)
    CONVERSION_ENTRY(Bool3x3)
    CONVERSION_ENTRY(Bool3x4)
    CONVERSION_ENTRY(Bool4x2)
    CONVERSION_ENTRY(Bool4x3)
    CONVERSION_ENTRY(Bool4x4)
    CONVERSION_ENTRY(Int2x2)
    CONVERSION_ENTRY(Int2x3)
    CONVERSION_ENTRY(Int2x4)
    CONVERSION_ENTRY(Int3x2)
    CONVERSION_ENTRY(Int3x3)
    CONVERSION_ENTRY(Int3x4)
    CONVERSION_ENTRY(Int4x2)
    CONVERSION_ENTRY(Int4x3)
    CONVERSION_ENTRY(Int4x4)
    CONVERSION_ENTRY(UInt2x2)
    CONVERSION_ENTRY(UInt2x3)
    CONVERSION_ENTRY(UInt2x4)
    CONVERSION_ENTRY(UInt3x2)
    CONVERSION_ENTRY(UInt3x3)
    CONVERSION_ENTRY(UInt3x4)
    CONVERSION_ENTRY(UInt4x2)
    CONVERSION_ENTRY(UInt4x3)
    CONVERSION_ENTRY(UInt4x4)
    CONVERSION_ENTRY(Half2x2)
    CONVERSION_ENTRY(Half2x3)
    CONVERSION_ENTRY(Half2x4)
    CONVERSION_ENTRY(Half3x2)
    CONVERSION_ENTRY(Half3x3)
    CONVERSION_ENTRY(Half3x4)
    CONVERSION_ENTRY(Half4x2)
    CONVERSION_ENTRY(Half4x3)
    CONVERSION_ENTRY(Half4x4)
    CONVERSION_ENTRY(Float2x2)
    CONVERSION_ENTRY(Float2x3)
    CONVERSION_ENTRY(Float2x4)
    CONVERSION_ENTRY(Float3x2)
    CONVERSION_ENTRY(Float3x3)
    CONVERSION_ENTRY(Float3x4)
    CONVERSION_ENTRY(Float4x2)
    CONVERSION_ENTRY(Float4x3)
    CONVERSION_ENTRY(Float4x4)
    CONVERSION_ENTRY(Double2x2)
    CONVERSION_ENTRY(Double2x3)
    CONVERSION_ENTRY(Double2x4)
    CONVERSION_ENTRY(Double3x2)
    CONVERSION_ENTRY(Double3x3)
    CONVERSION_ENTRY(Double3x4)
    CONVERSION_ENTRY(Double4x2)
    CONVERSION_ENTRY(Double4x3)
    CONVERSION_ENTRY(Double4x4)
    default:
        return Reflection::DataType::Undefined;
    }

#undef CONVERSION_ENTRY
}

Reflection::BufferType BufferTypeToReflType(BufferType bufferType)
{
#define CONVERSION_ENTRY(type) case BufferType::type: return Reflection::BufferType::type;

    switch (bufferType)
    {
    CONVERSION_ENTRY(Buffer)
    CONVERSION_ENTRY(StructuredBuffer)
    CONVERSION_ENTRY(ByteAddressBuffer)
    CONVERSION_ENTRY(RWBuffer)
    CONVERSION_ENTRY(RWStructuredBuffer)
    CONVERSION_ENTRY(RWByteAddressBuffer)
    CONVERSION_ENTRY(AppendStructuredBuffer)
    CONVERSION_ENTRY(ConsumeStructuredBuffer)
    CONVERSION_ENTRY(RWTexture1D)
    CONVERSION_ENTRY(RWTexture1DArray)
    CONVERSION_ENTRY(RWTexture2D)
    CONVERSION_ENTRY(RWTexture2DArray)
    CONVERSION_ENTRY(RWTexture3D)
    CONVERSION_ENTRY(Texture1D)
    CONVERSION_ENTRY(Texture1DArray)
    CONVERSION_ENTRY(Texture2D)
    CONVERSION_ENTRY(Texture2DArray)
    CONVERSION_ENTRY(Texture3D)
    CONVERSION_ENTRY(TextureCube)
    CONVERSION_ENTRY(TextureCubeArray)
    CONVERSION_ENTRY(Texture2DMS)
    CONVERSION_ENTRY(Texture2DMSArray)
    default:
        return Reflection::BufferType::Undefined;
    }

#undef CONVERSION_ENTRY
}

Reflection::VarType DataTypeToVarType(DataType dataType)
{
#define CONVERSION_ENTRY(type) case DataType::type: return Reflection::VarType::type;

    switch(dataType)
    {
    CONVERSION_ENTRY(Bool)
    CONVERSION_ENTRY(Int)
    CONVERSION_ENTRY(UInt)
    CONVERSION_ENTRY(Half)
    CONVERSION_ENTRY(Float)
    CONVERSION_ENTRY(Double)
    CONVERSION_ENTRY(Bool2)
    CONVERSION_ENTRY(Bool3)
    CONVERSION_ENTRY(Bool4)
    CONVERSION_ENTRY(Int2)
    CONVERSION_ENTRY(Int3)
    CONVERSION_ENTRY(Int4)
    CONVERSION_ENTRY(UInt2)
    CONVERSION_ENTRY(UInt3)
    CONVERSION_ENTRY(UInt4)
    CONVERSION_ENTRY(Half2)
    CONVERSION_ENTRY(Half3)
    CONVERSION_ENTRY(Half4)
    CONVERSION_ENTRY(Float2)
    CONVERSION_ENTRY(Float3)
    CONVERSION_ENTRY(Float4)
    CONVERSION_ENTRY(Double2)
    CONVERSION_ENTRY(Double3)
    CONVERSION_ENTRY(Double4)
    CONVERSION_ENTRY(Bool2x2)
    CONVERSION_ENTRY(Bool2x3)
    CONVERSION_ENTRY(Bool2x4)
    CONVERSION_ENTRY(Bool3x2)
    CONVERSION_ENTRY(Bool3x3)
    CONVERSION_ENTRY(Bool3x4)
    CONVERSION_ENTRY(Bool4x2)
    CONVERSION_ENTRY(Bool4x3)
    CONVERSION_ENTRY(Bool4x4)
    CONVERSION_ENTRY(Int2x2)
    CONVERSION_ENTRY(Int2x3)
    CONVERSION_ENTRY(Int2x4)
    CONVERSION_ENTRY(Int3x2)
    CONVERSION_ENTRY(Int3x3)
    CONVERSION_ENTRY(Int3x4)
    CONVERSION_ENTRY(Int4x2)
    CONVERSION_ENTRY(Int4x3)
    CONVERSION_ENTRY(Int4x4)
    CONVERSION_ENTRY(UInt2x2)
    CONVERSION_ENTRY(UInt2x3)
    CONVERSION_ENTRY(UInt2x4)
    CONVERSION_ENTRY(UInt3x2)
    CONVERSION_ENTRY(UInt3x3)
    CONVERSION_ENTRY(UInt3x4)
    CONVERSION_ENTRY(UInt4x2)
    CONVERSION_ENTRY(UInt4x3)
    CONVERSION_ENTRY(UInt4x4)
    CONVERSION_ENTRY(Half2x2)
    CONVERSION_ENTRY(Half2x3)
    CONVERSION_ENTRY(Half2x4)
    CONVERSION_ENTRY(Half3x2)
    CONVERSION_ENTRY(Half3x3)
    CONVERSION_ENTRY(Half3x4)
    CONVERSION_ENTRY(Half4x2)
    CONVERSION_ENTRY(Half4x3)
    CONVERSION_ENTRY(Half4x4)
    CONVERSION_ENTRY(Float2x2)
    CONVERSION_ENTRY(Float2x3)
    CONVERSION_ENTRY(Float2x4)
    CONVERSION_ENTRY(Float3x2)
    CONVERSION_ENTRY(Float3x3)
    CONVERSION_ENTRY(Float3x4)
    CONVERSION_ENTRY(Float4x2)
    CONVERSION_ENTRY(Float4x3)
    CONVERSION_ENTRY(Float4x4)
    CONVERSION_ENTRY(Double2x2)
    CONVERSION_ENTRY(Double2x3)
    CONVERSION_ENTRY(Double2x4)
    CONVERSION_ENTRY(Double3x2)
    CONVERSION_ENTRY(Double3x3)
    CONVERSION_ENTRY(Double3x4)
    CONVERSION_ENTRY(Double4x2)
    CONVERSION_ENTRY(Double4x3)
    CONVERSION_ENTRY(Double4x4)
    default:
        return Reflection::VarType::Undefined;
    }

#undef CONVERSION_ENTRY
}
// END BANSHEE CHANGES

IMPLEMENT_VISIT_PROC(FunctionDecl)
{
    if (ast->flags(FunctionDecl::isEntryPoint))
        ReflectAttributes(ast->declStmntRef->attribs);

    // BEGIN BANSHEE CHANGES

    Reflection::Function function;
    function.ident = ast->ident;

    if (ast->returnType)
    {
        if (auto baseTypeDenoter = ast->returnType->typeDenoter->As<BaseTypeDenoter>())
            function.returnValue = DataTypeToVarType(baseTypeDenoter->dataType);
        else
            function.returnValue = Reflection::VarType::Undefined;
    }
    else
        function.returnValue = Reflection::VarType::Void;

    for(auto& entry : ast->parameters)
    {
        if (entry->varDecls.size() == 0)
            continue;

        VarDeclPtr varDecl = entry->varDecls[0];

        Reflection::Parameter param;
        param.ident = varDecl->ident;

        if (entry->typeSpecifier && entry->typeSpecifier->typeDenoter)
        {
            if (auto baseTypeDenoter = entry->typeSpecifier->typeDenoter->As<BaseTypeDenoter>())
                param.type = DataTypeToVarType(baseTypeDenoter->dataType);
            else
                param.type = Reflection::VarType::Undefined;

            param.flags = entry->typeSpecifier->IsInput() ? Reflection::Parameter::Flags::In : 0;
            param.flags |= entry->typeSpecifier->IsOutput() ? Reflection::Parameter::Flags::Out : 0;
        }
        else
            param.type = Reflection::VarType::Undefined;

        function.parameters.push_back(param);
    }

    data_->functions.push_back(function);

    // END BANSHEE CHANGES

    Visitor::VisitFunctionDecl(ast, args);
}

void ReflectionAnalyzer::ReflectStruct(StructDecl* ast, Reflection::Struct& output)
{
    if (ast->baseStructRef)
        ReflectStruct(ast, output);

    for (auto& stmt : ast->varMembers)
    {
        Reflection::VariableType type;
        int baseType = -1;
        if (auto structTypeDenoter = stmt->typeSpecifier->typeDenoter->As<StructTypeDenoter>())
        {
            type = Reflection::VariableType::Struct;

            int i = 0;
            for (auto& entry : data_->structs)
            {
                if (entry.name == structTypeDenoter->Ident())
                {
                    baseType = i;
                    break;
                }

                i++;
            }

            if(baseType == -1)
                Warning("Cannot find definition for embedded struct type.", ast);
        }
        else
        {
            type = Reflection::VariableType::Variable;

            if (auto baseTypeDenoter = stmt->typeSpecifier->typeDenoter->As<BaseTypeDenoter>())
                baseType = (int)DataTypeToReflType(baseTypeDenoter->dataType);
        }

        for (auto& memberDecl : stmt->varDecls)
        {
            Reflection::Variable member;
            member.ident = memberDecl->ident;
            member.type = type;
            member.baseType = baseType;

            for (auto& arrayDims : memberDecl->arrayDims)
            {
                int arraySize = arrayDims->size <= 0 ? 1 : arrayDims->size;
                member.arraySize *= arraySize;
            }

            output.members.push_back(member);
        }
    };
}

IMPLEMENT_VISIT_PROC(StructDecl)
{
    data_->structs.emplace_back();

    Reflection::Struct& structDesc = data_->structs.back();
    structDesc.name = ast->ident;

    ReflectStruct(ast, structDesc);
}

IMPLEMENT_VISIT_PROC(UniformBufferDecl)
{
    // BEGIN BANSHEE CHANGES
    //if (ast->flags(AST::isReachable))
    // END BANSHEE CHANGES
    {
        /* Reflect constant buffer binding */
        data_->constantBuffers.push_back({ ast->ident, GetBindingPoint(ast->slotRegisters) });

        // BEGIN BANSHEE CHANGES

        Reflection::Uniform uniform;
        uniform.ident = ast->ident;
        uniform.type = Reflection::VariableType::UniformBuffer;
        uniform.baseType = 0;

        if ((ast->extModifiers & ExtModifiers::Internal) != 0)
            uniform.flags = Reflection::Uniform::Flags::Internal;

        data_->uniforms.push_back(uniform);

        for(auto& stmt : ast->varMembers)
        {
            Reflection::VariableType type;
            int baseType = 0;

            BaseTypeDenoter* baseTypeDenoter = nullptr;
            if (StructTypeDenoter* structTypeDenoter = stmt->typeSpecifier->typeDenoter->As<StructTypeDenoter>())
            {
                type = Reflection::VariableType::Struct;

                int i = 0;
                for (auto& entry : data_->structs)
                {
                    if (entry.name == structTypeDenoter->Ident())
                    {
                        baseType = i;
                        break;
                    }

                    i++;
                }
            }
            else
            {
                type = Reflection::VariableType::Variable;

                if (baseTypeDenoter = stmt->typeSpecifier->typeDenoter->As<BaseTypeDenoter>())
                    baseType = (int)DataTypeToReflType(baseTypeDenoter->dataType);
            }
            
            int blockIdx = (int)data_->constantBuffers.size() - 1;

            for(auto& decl : stmt->varDecls)
            {
                Reflection::Uniform uniform;
                uniform.ident = decl->ident;
                uniform.type = type;
                uniform.baseType = baseType;
                uniform.uniformBlock = blockIdx;
                uniform.flags = 0;

                for (auto& arrayDims : decl->arrayDims)
                {
                    int arraySize = arrayDims->size <= 0 ? 1 : arrayDims->size;
                    uniform.arraySize *= arraySize;
                }

                if(baseTypeDenoter != nullptr)
                {
                    if ((baseTypeDenoter->extModifiers & ExtModifiers::Internal) != 0)
                        uniform.flags |= Reflection::Uniform::Flags::Internal;

                    if ((baseTypeDenoter->extModifiers & ExtModifiers::Color) != 0)
                        uniform.flags |= Reflection::Uniform::Flags::Color;

                    uniform.spriteUVRef = baseTypeDenoter->spriteUVRef;

                    if(decl->defaultValue.available)
                    {
                        Reflection::DefaultValue defaultValue;
                        memcpy(&defaultValue, &decl->defaultValue.matrix, sizeof(defaultValue));

                        uniform.defaultValue = (int)data_->defaultValues.size();
                        data_->defaultValues.push_back(defaultValue);
                    }
                }

                data_->uniforms.push_back(uniform);
            }
        }

        // END BANSHEE CHANGES
    }
}

IMPLEMENT_VISIT_PROC(BufferDeclStmnt)
{
// BEGIN BANSHEE CHANGES
    //if (ast->flags(AST::isReachable))
// END BANSHEE CHANGES
    {
        for (auto& bufferDecl : ast->bufferDecls)
        {
            // BEGIN BANSHEE CHANGES
            //if (bufferDecl->flags(AST::isReachable))
            // END BANSHEE CHANGES
            {
                /* Reflect texture or storage-buffer binding */
                Reflection::BindingSlot bindingSlot;
                {
                    bindingSlot.ident       = bufferDecl->ident;
                    bindingSlot.location    = GetBindingPoint(bufferDecl->slotRegisters);
                };

                if (!IsStorageBufferType(ast->typeDenoter->bufferType))
                    data_->textures.push_back(bindingSlot);
                else
                    data_->storageBuffers.push_back(bindingSlot);

                // BEGIN BANSHEE CHANGES

                Reflection::Uniform uniform;
                uniform.ident = bufferDecl->ident;
                uniform.type = Reflection::VariableType::Buffer;
                uniform.baseType = (int)BufferTypeToReflType(ast->typeDenoter->bufferType);

                if ((ast->typeDenoter->extModifiers & ExtModifiers::Internal) != 0)
                    uniform.flags |= Reflection::Uniform::Flags::Internal;

                if ((ast->typeDenoter->extModifiers & ExtModifiers::Color) != 0)
                    uniform.flags |= Reflection::Uniform::Flags::Color;

                if (bufferDecl->defaultValue.available)
                {
                    Reflection::DefaultValue defaultValue;
                    defaultValue.handle = bufferDecl->defaultValue.handle;

                    uniform.defaultValue = (int)data_->defaultValues.size();
                    data_->defaultValues.push_back(defaultValue);
                }

                data_->uniforms.push_back(uniform);

                // END BANSHEE CHANGES
            }
        }
    }
}

#undef IMPLEMENT_VISIT_PROC

/* --- Helper functions for code reflection --- */

void ReflectionAnalyzer::ReflectSamplerValue(SamplerValue* ast, Reflection::SamplerState& samplerState)
{
    const auto& name = ast->name;

    /* Assign value to sampler state */
    if (auto literalExpr = ast->value->As<LiteralExpr>())
    {
        const auto& value = literalExpr->value;

        if (name == "MipLODBias")
            samplerState.mipLODBias = FromStringOrDefault<float>(value);
        else if (name == "MaxAnisotropy")
            samplerState.maxAnisotropy = static_cast<unsigned int>(FromStringOrDefault<unsigned long>(value));
        else if (name == "MinLOD")
            samplerState.minLOD = FromStringOrDefault<float>(value);
        else if (name == "MaxLOD")
            samplerState.maxLOD = FromStringOrDefault<float>(value);
    }
    else if (auto objectExpr = ast->value->As<ObjectExpr>())
    {
        const auto& value = objectExpr->ident;

        if (name == "Filter")
            ReflectSamplerValueFilter(value, samplerState.filter, ast);
        else if (name == "AddressU")
            ReflectSamplerValueTextureAddressMode(value, samplerState.addressU, ast);
        else if (name == "AddressV")
            ReflectSamplerValueTextureAddressMode(value, samplerState.addressV, ast);
        else if (name == "AddressW")
            ReflectSamplerValueTextureAddressMode(value, samplerState.addressW, ast);
        else if (name == "ComparisonFunc")
            ReflectComparisonFunc(value, samplerState.comparisonFunc, ast);
    }
    else if (name == "BorderColor")
    {
        try
        {
            if (auto callExpr = ast->value->As<CallExpr>())
            {
                if (callExpr->typeDenoter && callExpr->typeDenoter->IsVector() && callExpr->arguments.size() == 4)
                {
                    /* Evaluate sub expressions to constant floats */
                    for (std::size_t i = 0; i < 4; ++i)
                        samplerState.borderColor[i] = EvaluateConstExprFloat(*callExpr->arguments[i]);
                }
                else
                    throw std::string(R_InvalidTypeOrArgCount);
            }
            else if (auto castExpr = ast->value->As<CastExpr>())
            {
                /* Evaluate sub expression to constant float and copy into four sub values */
                auto subValueSrc = EvaluateConstExprFloat(*castExpr->expr);
                for (std::size_t i = 0; i < 4; ++i)
                    samplerState.borderColor[i] = subValueSrc;
            }
            else if (auto initExpr = ast->value->As<InitializerExpr>())
            {
                if (initExpr->exprs.size() == 4)
                {
                    /* Evaluate sub expressions to constant floats */
                    for (std::size_t i = 0; i < 4; ++i)
                        samplerState.borderColor[i] = EvaluateConstExprFloat(*initExpr->exprs[i]);
                }
                else
                    throw std::string(R_InvalidArgCount);
            }
        }
        catch (const std::string& s)
        {
            Warning(R_FailedToInitializeSamplerValue(s, "BorderColor"), ast->value.get());
        }
    }
}

void ReflectionAnalyzer::ReflectStencilOperationValue(StateValue* ast, Reflection::StencilOperation& stencilOperation)
{
    const auto& name = ast->name;

    if (auto objectExpr = ast->value->As<ObjectExpr>())
    {
        const auto& value = objectExpr->ident;

        if (name == "fail")
            ReflectStencilOpType(value, stencilOperation.fail, ast);
        else if (name == "zfail")
            ReflectStencilOpType(value, stencilOperation.zfail, ast);
        else if (name == "pass")
            ReflectStencilOpType(value, stencilOperation.pass, ast);
        else if (name == "compare")
            ReflectComparisonFunc(value, stencilOperation.compareFunc, ast);
        else
            Error(R_UnknownStateKeyword("stencil operation"), ast);
    }
    else
        Error(R_ExpectedStateKeyword, ast);
}

void ReflectionAnalyzer::ReflectBlendOperationValue(StateValue* ast, Reflection::BlendOperation& blendOperation)
{
    const auto& name = ast->name;

    if (auto objectExpr = ast->value->As<ObjectExpr>())
    {
        const auto& value = objectExpr->ident;

        if (name == "source")
            ReflectBlendFactor(value, blendOperation.source, ast);
        else if (name == "dest")
            ReflectBlendFactor(value, blendOperation.destination, ast);
        else if (name == "op")
            ReflectBlendOpType(value, blendOperation.operation, ast);
        else
            Error(R_UnknownStateKeyword("blend operation"), ast);
    }
    else
        Error(R_ExpectedStateKeyword, ast);
}

void ReflectionAnalyzer::ReflectBlendStateTargetValue(StateValue* ast, Reflection::BlendStateTarget& blendStateTarget)
{
    const auto& name = ast->name;

    if (name == "enabled")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            blendStateTarget.enabled = variant.Bool();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "writemask")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            blendStateTarget.writeMask = (int8_t)variant.Int();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "color")
    {
        if (auto stateInitializerExpr = ast->value->As<StateInitializerExpr>())
        {
            for (auto& expr : stateInitializerExpr->exprs)
                ReflectBlendOperationValue(expr.get(), blendStateTarget.colorOp);
        }
        else
            Error(R_ExpectedStateInitializerExpr, ast);
    }
    else if (name == "alpha")
    {
        if (auto stateInitializerExpr = ast->value->As<StateInitializerExpr>())
        {
            for (auto& expr : stateInitializerExpr->exprs)
                ReflectBlendOperationValue(expr.get(), blendStateTarget.alphaOp);
        }
        else
            Error(R_ExpectedStateInitializerExpr, ast);
    }
    else if (name == "index")
    {
        // Ignore, parsed elsewhere
    }
    else
        Error(R_UnknownStateKeyword("blend target"), ast);
}

void ReflectionAnalyzer::ReflectRasterizerStateValue(StateValue* ast, Reflection::RasterizerState& rasterizerState)
{
    const auto& name = ast->name;

    if (name == "scissor")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            rasterizerState.scissorEnable = variant.Bool();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "multisample")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            rasterizerState.multisampleEnable = variant.Bool();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "lineaa")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            rasterizerState.antialisedLineEnable = variant.Bool();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "fill")
    {
        if (auto objectExpr = ast->value->As<ObjectExpr>())
            ReflectFillMode(objectExpr->ident, rasterizerState.fillMode, ast);
        else
            Error(R_ExpectedStateKeyword, ast);
    }
    else if (name == "cull")
    {
        if (auto objectExpr = ast->value->As<ObjectExpr>())
            ReflectCullMode(objectExpr->ident, rasterizerState.cullMode, ast);
        else
            Error(R_ExpectedStateKeyword, ast);
    }
    else
        Error(R_UnknownStateKeyword("rasterizer"), ast);
}

void ReflectionAnalyzer::ReflectDepthStateValue(StateValue* ast, Reflection::DepthState& depthState)
{
    const auto& name = ast->name;

    if (name == "read")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            depthState.readEnable = variant.Bool();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "write")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            depthState.writeEnable = variant.Bool();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "compare")
    {
        if (auto objectExpr = ast->value->As<ObjectExpr>())
            ReflectComparisonFunc(objectExpr->ident, depthState.compareFunc, ast);
        else
            Error(R_ExpectedStateKeyword, ast);
    }
    else if (name == "bias")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            depthState.depthBias = (float)variant.Real();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "scaledBias")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            depthState.scaledDepthBias = (float)variant.Real();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "clip")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            depthState.depthClip = variant.Bool();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else
        Error(R_UnknownStateKeyword("depth"), ast);
}

void ReflectionAnalyzer::ReflectStencilStateValue(StateValue* ast, Reflection::StencilState& stencilState)
{
    const auto& name = ast->name;

    if (name == "enabled")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            stencilState.enabled = variant.Bool();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "reference")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            stencilState.reference = (int32_t)variant.Int();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "readmask")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            stencilState.readMask = (int8_t)variant.Int();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "writemask")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            stencilState.writeMask = (int8_t)variant.Int();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "back")
    {
        if (auto stateInitializerExpr = ast->value->As<StateInitializerExpr>())
        {
            for (auto& expr : stateInitializerExpr->exprs)
                ReflectStencilOperationValue(expr.get(), stencilState.back);
        }
        else
            Error(R_ExpectedStateInitializerExpr, ast);
    }
    else if (name == "front")
    {
        if (auto stateInitializerExpr = ast->value->As<StateInitializerExpr>())
        {
            for (auto& expr : stateInitializerExpr->exprs)
                ReflectStencilOperationValue(expr.get(), stencilState.front);
        }
        else
            Error(R_ExpectedStateInitializerExpr, ast);
    }
    else
        Error(R_UnknownStateKeyword("stencil"), ast);
}

void ReflectionAnalyzer::ReflectBlendStateValue(StateValue* ast, Reflection::BlendState& blendState, uint32_t& blendTargetIdx)
{
    const auto& name = ast->name;

    if (name == "dither")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            blendState.alphaToCoverage = variant.Bool();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "independant")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            blendState.independantBlend = variant.Bool();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "target")
    {
        if (auto stateInitializerExpr = ast->value->As<StateInitializerExpr>())
        {
            // First look for an explicit target index
            for (auto& expr : stateInitializerExpr->exprs)
            {
                const auto& name = expr->name;
                if(name == "index")
                {
                    if (auto literalExpr = expr->value->As<LiteralExpr>())
                    {
                        auto variant = Variant::ParseFrom(literalExpr->value);
                        blendTargetIdx = (uint32_t)variant.Int();
                    }
                    else
                        Error(R_ExpectedLiteralExpr, expr->value.get());
                }
            }

            if(blendTargetIdx < Reflection::BlendState::MAX_NUM_RENDER_TARGETS)
            {
                for (auto& expr : stateInitializerExpr->exprs)
                    ReflectBlendStateTargetValue(expr.get(), blendState.targets[blendTargetIdx]);

                blendTargetIdx++;
            }
        }
        else
            Error(R_ExpectedStateInitializerExpr, ast);
    }
    else
        Error(R_UnknownStateKeyword("blend"), ast);
}

void ReflectionAnalyzer::ReflectOptionsStateValue(StateValue* ast, Reflection::GlobalOptions& options)
{
    const auto& name = ast->name;

    if (name == "separable")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            options.separable = variant.Bool();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "priority")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            options.priority = (int32_t)variant.Int();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "transparent")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            options.transparent = variant.Bool();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "forward")
    {
        if (auto literalExpr = ast->value->As<LiteralExpr>())
        {
            auto variant = Variant::ParseFrom(literalExpr->value);
            options.transparent = variant.Bool();
        }
        else
            Error(R_ExpectedLiteralExpr, ast);
    }
    else if (name == "sort")
    {
        if (auto objectExpr = ast->value->As<ObjectExpr>())
            ReflectSortMode(objectExpr->ident, options.sortMode, ast);
        else
            Error(R_ExpectedStateKeyword, ast);
    }
    else
        Error(R_UnknownStateKeyword("options"), ast);
}


void ReflectionAnalyzer::ReflectSamplerValueFilter(const std::string& value, Reflection::Filter& filter, const AST* ast)
{
    try
    {
        filter = StringToFilter(value);
    }
    catch (const std::invalid_argument& e)
    {
        Error(e.what(), ast);
    }
}

void ReflectionAnalyzer::ReflectBlendFactor(const std::string& value, Reflection::BlendFactor& blendFactor, const AST* ast)
{
    try
    {
        blendFactor = StringToBlendFactor(value);
    }
    catch (const std::invalid_argument& e)
    {
        Error(e.what(), ast);
    }
}

void ReflectionAnalyzer::ReflectBlendOpType(const std::string& value, Reflection::BlendOpType& blendOp, const AST* ast)
{
    try
    {
        blendOp = StringToBlendOpType(value);
    }
    catch (const std::invalid_argument& e)
    {
        Error(e.what(), ast);
    }
}

void ReflectionAnalyzer::ReflectCullMode(const std::string& value, Reflection::CullMode& cullMode, const AST* ast)
{
    try
    {
        cullMode = StringToCullMode(value);
    }
    catch (const std::invalid_argument& e)
    {
        Error(e.what(), ast);
    }
}

void ReflectionAnalyzer::ReflectFillMode(const std::string& value, Reflection::FillMode& fillMode, const AST* ast)
{
    try
    {
        fillMode = StringToFillMode(value);
    }
    catch (const std::invalid_argument& e)
    {
        Error(e.what(), ast);
    }
}

void ReflectionAnalyzer::ReflectStencilOpType(const std::string& value, Reflection::StencilOpType& stencilOp, const AST* ast)
{
    try
    {
        stencilOp = StringToStencilOpType(value);
    }
    catch (const std::invalid_argument& e)
    {
        Error(e.what(), ast);
    }
}

void ReflectionAnalyzer::ReflectSamplerValueTextureAddressMode(const std::string& value, Reflection::TextureAddressMode& addressMode, const AST* ast)
{
    try
    {
        addressMode = StringToTexAddressMode(value);
    }
    catch (const std::invalid_argument& e)
    {
        Warning(e.what(), ast);
    }
}

void ReflectionAnalyzer::ReflectComparisonFunc(const std::string& value, Reflection::ComparisonFunc& comparisonFunc, const AST* ast)
{
    try
    {
        comparisonFunc = StringToCompareFunc(value);
    }
    catch (const std::invalid_argument& e)
    {
        Warning(e.what(), ast);
    }
}

void ReflectionAnalyzer::ReflectSortMode(const std::string& value, Reflection::SortMode& sortMode, const AST* ast)
{
    try
    {
        sortMode = StringToSortMode(value);
    }
    catch (const std::invalid_argument& e)
    {
        Error(e.what(), ast);
    }
}

void ReflectionAnalyzer::ReflectAttributes(const std::vector<AttributePtr>& attribs)
{
    for (const auto& attr : attribs)
    {
        switch (attr->attributeType)
        {
            case AttributeType::NumThreads:
                ReflectAttributesNumThreads(attr.get());
                break;
            default:
                break;
        }
    }
}

void ReflectionAnalyzer::ReflectAttributesNumThreads(Attribute* ast)
{
    /* Reflect "numthreads" attribute for compute shader */
    if (shaderTarget_ == ShaderTarget::ComputeShader && ast->arguments.size() == 3)
    {
        /* Evaluate attribute arguments */
        data_->numThreads.x = EvaluateConstExprInt(*ast->arguments[0]);
        data_->numThreads.y = EvaluateConstExprInt(*ast->arguments[1]);
        data_->numThreads.z = EvaluateConstExprInt(*ast->arguments[2]);
    }
}


} // /namespace Xsc



// ================================================================================
