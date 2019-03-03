/*
 * HLSLGenerator.cpp
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2017 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "HLSLGenerator.h"
#include "HLSLKeywords.h"
#include "ReferenceAnalyzer.h"
#include "StructParameterAnalyzer.h"
#include "TypeDenoter.h"
#include "Exception.h"
#include "TypeConverter.h"
#include "ExprConverter.h"
#include "FuncNameConverter.h"
#include "Helper.h"
#include "ReportIdents.h"
#include <initializer_list>
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>


namespace Xsc
{


/*
 * Internal structures
 */

struct IfStmntArgs
{
    bool inHasElseParentNode;
};

struct StructDeclArgs
{
    bool inEndWithSemicolon;
};


/*
 * HLSLGenerator class
 */

HLSLGenerator::HLSLGenerator(Log* log) :
    Generator { log }
{
}

void HLSLGenerator::GenerateCodePrimary(
    Program& program, const ShaderInput& inputDesc, const ShaderOutput& outputDesc)
{
    /* Store parameters */
    versionOut_         = outputDesc.shaderVersion;
    nameMangling_       = outputDesc.nameMangling;
    allowExtensions_    = outputDesc.options.allowExtensions;
    preserveComments_   = outputDesc.options.preserveComments;
    separateShaders_    = outputDesc.options.separateShaders;
    allowLineMarks_     = outputDesc.formatting.lineMarks;
    compactWrappers_    = outputDesc.formatting.compactWrappers;
    alwaysBracedScopes_ = outputDesc.formatting.alwaysBracedScopes;

    #ifdef XSC_ENABLE_LANGUAGE_EXT
    extensions_         = inputDesc.extensions;
    #endif

    if (program.entryPointRef)
    {
        try
        {
            /* Pre-process AST before generation begins */
            PreProcessAST(inputDesc, outputDesc);

            /* Write header */
            if (inputDesc.entryPoint.empty())
                WriteComment("HLSL " + ToString(GetShaderTarget()));
            else
                WriteComment("HLSL " + ToString(GetShaderTarget()) + " \"" + inputDesc.entryPoint + "\"");
        
            WriteComment("Generated by XShaderCompiler");

            WriteComment(TimePoint());
            Blank();

            /* Visit program AST */
            Visit(&program);
        }
        catch (const Report&)
        {
            throw;
        }
        catch (const ASTRuntimeError& e)
        {
            Error(e.what(), e.GetAST());
        }
        catch (const std::exception& e)
        {
            Error(e.what());
        }
    }
    else
        Error(R_EntryPointNotFound(inputDesc.entryPoint));
}


/*
 * ======= Private: =======
 */

/* ------- Visit functions ------- */

#define IMPLEMENT_VISIT_PROC(AST_NAME) \
    void HLSLGenerator::Visit##AST_NAME(AST_NAME* ast, void* args)

IMPLEMENT_VISIT_PROC(Program)
{
    /* Write global uniform declarations */
    WriteGlobalUniforms();

    /* Write global program statements */
    WriteStmntList(ast->globalStmnts, true);
}

IMPLEMENT_VISIT_PROC(CodeBlock)
{
    WriteScopeOpen();
    {
        WriteStmntList(ast->stmnts);
    }
    WriteScopeClose();
}

IMPLEMENT_VISIT_PROC(SwitchCase)
{
    /* Write case header */
    if (ast->expr)
    {
        BeginLn();
        {
            Write("case ");
            Visit(ast->expr);
            Write(":");
        }
        EndLn();
    }
    else
        WriteLn("default:");

    /* Write statement list */
    IncIndent();
    {
        Visit(ast->stmnts);
    }
    DecIndent();
}

IMPLEMENT_VISIT_PROC(ArrayDimension)
{
    Write(ast->ToString());
}

IMPLEMENT_VISIT_PROC(TypeSpecifier)
{
    if (ast->structDecl)
        Visit(ast->structDecl);
    else
        WriteTypeDenoter(*ast->typeDenoter, ast);
}

/* --- Declarations --- */

IMPLEMENT_VISIT_PROC(VarDecl)
{
    if (auto staticMemberVar = ast->FetchStaticVarDeclRef())
        Write(staticMemberVar->ident);
    else
        Write(InsideStructDecl() ? ast->ident.Original() : ast->ident.Final());

    Visit(ast->arrayDims);

    if (!InsideUniformBufferDecl())
    {
        if (ast->initializer)
        {
            const auto& typeDen = ast->initializer->GetTypeDenoter()->GetAliased();
            if (!typeDen.IsNull())
            {
                Write(" = ");
                Visit(ast->initializer);
            }
        }
    }

    if(ast->semantic != Semantic::Undefined)
        Write(" : " + ast->semantic.ToString());
}

IMPLEMENT_VISIT_PROC(StructDecl)
{
    PushStructDecl(ast);
    {
        if (auto structDeclArgs = reinterpret_cast<StructDeclArgs*>(args))
            WriteStructDecl(ast, structDeclArgs->inEndWithSemicolon);
        else
            WriteStructDecl(ast, false);
    }
    PopStructDecl();
}

IMPLEMENT_VISIT_PROC(SamplerDecl)
{
    WriteSamplerDecl(*ast);
}

IMPLEMENT_VISIT_PROC(StateDecl)
{
    // Do nothing
}

/* --- Declaration statements --- */

IMPLEMENT_VISIT_PROC(FunctionDecl)
{
    /* Is this function reachable from the entry point? */
    if (!ast->flags(AST::isReachable))
    {
        /* Check for valid control paths */
        if (WarnEnabled(Warnings::Basic) && ast->flags(FunctionDecl::hasNonReturnControlPath))
            Warning(R_InvalidControlPathInUnrefFunc(ast->ToString()), ast);
        return;
    }

    /* Check for valid control paths */
    if (ast->flags(FunctionDecl::hasNonReturnControlPath))
        Error(R_InvalidControlPathInFunc(ast->ToString()), ast);

    /* Write line */
    WriteLineMark(ast);

    /* Write function declaration */
    PushFunctionDecl(ast);
    {
        if (ast->flags(FunctionDecl::isEntryPoint))
            WriteGlobalLayouts();

        WriteFunction(ast);
    }
    PopFunctionDecl();

    Blank();
}

IMPLEMENT_VISIT_PROC(UniformBufferDecl)
{
    if (!ast->flags(AST::isReachable))
        return;

    /* Write uniform buffer header */
    WriteLineMark(ast);

    /* Write uniform buffer declaration */
    ast->DeriveCommonStorageLayout();

    BeginLn();

    Write("cbuffer " + ast->ident);

    /* Write uniform buffer members */
    WriteScopeOpen(false, true);
    BeginSep();
    {
        PushUniformBufferDecl(ast);
        {
            Visit(ast->varMembers);
        }
        PopUniformBufferDecl();
    }
    EndSep();
    WriteScopeClose();

    Blank();
}

IMPLEMENT_VISIT_PROC(BufferDeclStmnt)
{
    if (ast->flags(AST::isReachable))
    {
        BeginLn();
        WriteTypeDenoter(*ast->typeDenoter, ast);
        Write(" ");

        /* Write buffer declarations */
        for (std::size_t i = 0; i < ast->bufferDecls.size(); i++)
        {
            WriteBufferDecl(ast->bufferDecls[i].get());

            if (i + 1 < ast->bufferDecls.size())
                Write(", ");
        }

        Write(";");
        EndLn();
    }
}

IMPLEMENT_VISIT_PROC(SamplerDeclStmnt)
{
    if (ast->flags(AST::isReachable))
    {
        /* Write sampler declarations */
        Visit(ast->samplerDecls);
    }
}

IMPLEMENT_VISIT_PROC(VarDeclStmnt)
{
    if (!ast->flags(AST::isReachable) && !InsideFunctionDecl() && !InsideStructDecl())
        return;

    auto varDecls = ast->varDecls;

    /* Ignore declaration statement of static member variables */
    if (ast->typeSpecifier->HasAnyStorageClassOf({ StorageClass::Static }) && ast->FetchStructDeclRef() != nullptr)
        return;

    PushVarDeclStmnt(ast);
    {
        BeginLn();

        /* Write storage classes and interpolation modifiers (must be before in/out keywords) */
        if (!InsideStructDecl())
        {
            WriteInterpModifiers(ast->typeSpecifier->interpModifiers, ast);
            WriteStorageClasses(ast->typeSpecifier->storageClasses, ast);
        }

        Separator();

        /* Write type modifiers */
        WriteTypeModifiersFrom(ast->typeSpecifier);
        Separator();

        /* Write variable type */
        if (ast->typeSpecifier->structDecl)
        {
            /* Do not end line here with "EndLn" */
            Visit(ast->typeSpecifier);
            BeginLn();
        }
        else
        {
            Visit(ast->typeSpecifier);
            Write(" ");
        }

        Separator();

        /* Write variable declarations */
        for (std::size_t i = 0; i < varDecls.size(); ++i)
        {
            Visit(varDecls[i]);
            if (i + 1 < varDecls.size())
                Write(", ");
        }

        Write(";");
        EndLn();
    }
    PopVarDeclStmnt();

    if (InsideGlobalScope())
        Blank();
}

IMPLEMENT_VISIT_PROC(AliasDeclStmnt)
{
    if (ast->structDecl && !ast->structDecl->IsAnonymous())
    {
        WriteLineMark(ast);

        /* Write structure declaration and end it with a semicolon */
        StructDeclArgs structDeclArgs;
        structDeclArgs.inEndWithSemicolon = true;

        Visit(ast->structDecl, &structDeclArgs);
    }
}

IMPLEMENT_VISIT_PROC(BasicDeclStmnt)
{
    if (ast->flags(AST::isReachable))
    {
        if (auto structDecl = ast->declObject->As<StructDecl>())
        {
            WriteLineMark(ast);

            /* Visit structure declaration */
            StructDeclArgs structDeclArgs;
            structDeclArgs.inEndWithSemicolon = true;

            Visit(structDecl, &structDeclArgs);
        }
        else
        {
            /* Visit declaration object only */
            Visit(ast->declObject);
        }
    }
}

/* --- Statements --- */

IMPLEMENT_VISIT_PROC(NullStmnt)
{
    WriteLn(";");
}

IMPLEMENT_VISIT_PROC(CodeBlockStmnt)
{
    Visit(ast->codeBlock);
}

IMPLEMENT_VISIT_PROC(ForLoopStmnt)
{
    /* Write loop header */
    BeginLn();
    
    Write("for (");

    PushOptions({ false, false });
    {
        Visit(ast->initStmnt);
        Write(" "); // initStmnt already has the ';'!
        Visit(ast->condition);
        Write("; ");
        Visit(ast->iteration);
    }
    PopOptions();

    Write(")");

    WriteScopedStmnt(ast->bodyStmnt.get());
}

IMPLEMENT_VISIT_PROC(WhileLoopStmnt)
{
    /* Write loop condExpr */
    BeginLn();
    
    Write("while (");
    Visit(ast->condition);
    Write(")");

    WriteScopedStmnt(ast->bodyStmnt.get());
}

IMPLEMENT_VISIT_PROC(DoWhileLoopStmnt)
{
    BeginLn();

    Write("do");
    WriteScopedStmnt(ast->bodyStmnt.get());

    /* Write loop condExpr */
    WriteScopeContinue();
    
    Write("while (");
    Visit(ast->condition);
    Write(");");
    
    EndLn();
}

IMPLEMENT_VISIT_PROC(IfStmnt)
{
    bool hasElseParentNode = (args != nullptr ? reinterpret_cast<IfStmntArgs*>(args)->inHasElseParentNode : false);

    /* Write if condExpr */
    if (!hasElseParentNode)
        BeginLn();
    
    Write("if (");
    Visit(ast->condition);
    Write(")");
    
    /* Write if body */
    WriteScopedStmnt(ast->bodyStmnt.get());

    Visit(ast->elseStmnt);
}

IMPLEMENT_VISIT_PROC(ElseStmnt)
{
    if (ast->bodyStmnt->Type() == AST::Types::IfStmnt)
    {
        /* Write else if statement */
        WriteScopeContinue();
        Write("else ");

        if (ast->bodyStmnt->Type() == AST::Types::IfStmnt)
        {
            IfStmntArgs ifStmntArgs;
            ifStmntArgs.inHasElseParentNode = true;
            Visit(ast->bodyStmnt, &ifStmntArgs);
        }
        else
            Visit(ast->bodyStmnt);
    }
    else
    {
        /* Write else statement */
        WriteScopeContinue();
        Write("else");
        WriteScopedStmnt(ast->bodyStmnt.get());
    }
}

IMPLEMENT_VISIT_PROC(SwitchStmnt)
{
    /* Write selector */
    BeginLn();
    
    Write("switch (");
    Visit(ast->selector);
    Write(")");

    /* Write switch cases */
    WriteScopeOpen();
    {
        Visit(ast->cases);
    }
    WriteScopeClose();
}

IMPLEMENT_VISIT_PROC(ExprStmnt)
{
    BeginLn();
    {
        Visit(ast->expr);
        Write(";");
    }
    EndLn();
}

IMPLEMENT_VISIT_PROC(ReturnStmnt)
{
    if (ast->expr)
    {
        BeginLn();
        {
            Write("return ");
            Visit(ast->expr);
            Write(";");
        }
        EndLn();
    }
    else if (!ast->flags(ReturnStmnt::isEndOfFunction))
        WriteLn("return;");
}

IMPLEMENT_VISIT_PROC(CtrlTransferStmnt)
{
    WriteLn(CtrlTransformToString(ast->transfer) + ";");
}

/* --- Expressions --- */

IMPLEMENT_VISIT_PROC(SequenceExpr)
{
    for (std::size_t i = 0, n = ast->exprs.size(); i < n; ++i)
    {
        Visit(ast->exprs[i]);
        if (i + 1 < n)
            Write(", ");
    }
}

IMPLEMENT_VISIT_PROC(LiteralExpr)
{
    Write(ast->value);
}

IMPLEMENT_VISIT_PROC(TypeSpecifierExpr)
{
    WriteTypeDenoter(*ast->typeSpecifier->typeDenoter, ast);
}

IMPLEMENT_VISIT_PROC(TernaryExpr)
{
    Visit(ast->condExpr);
    Write(" ? ");
    Visit(ast->thenExpr);
    Write(" : ");
    Visit(ast->elseExpr);
}

IMPLEMENT_VISIT_PROC(BinaryExpr)
{
    Visit(ast->lhsExpr);
    Write(" " + BinaryOpToString(ast->op) + " ");
    Visit(ast->rhsExpr);
}

IMPLEMENT_VISIT_PROC(UnaryExpr)
{
    Write(UnaryOpToString(ast->op));
    Visit(ast->expr);
}

IMPLEMENT_VISIT_PROC(PostUnaryExpr)
{
    Visit(ast->expr);
    Write(UnaryOpToString(ast->op));
}

IMPLEMENT_VISIT_PROC(CallExpr)
{
    WriteCallExprStandard(ast);
}

IMPLEMENT_VISIT_PROC(BracketExpr)
{
    Write("(");
    Visit(ast->expr);
    Write(")");
}

IMPLEMENT_VISIT_PROC(ObjectExpr)
{
    WriteObjectExpr(*ast);
}

IMPLEMENT_VISIT_PROC(AssignExpr)
{
    Visit(ast->lvalueExpr);
    Write(" " + AssignOpToString(ast->op) + " ");
    Visit(ast->rvalueExpr);
}

IMPLEMENT_VISIT_PROC(ArrayExpr)
{
    WriteArrayExpr(*ast);
}

IMPLEMENT_VISIT_PROC(CastExpr)
{
    WriteTypeDenoter(*ast->typeSpecifier->typeDenoter, ast);
    Write("(");
    Visit(ast->expr);
    Write(")");
}

IMPLEMENT_VISIT_PROC(InitializerExpr)
{
    if (ast->GetTypeDenoter()->GetAliased().IsArray())
    {
        WriteScopeOpen();

        for (std::size_t i = 0; i < ast->exprs.size(); ++i)
        {
            BeginLn();
            Visit(ast->exprs[i]);
            if (i + 1 < ast->exprs.size())
                Write(",");
            EndLn();
        }

        WriteScopeClose();
        BeginLn();
    }
    else
    {
        Write("{ ");
        
        for (std::size_t i = 0; i < ast->exprs.size(); ++i)
        {
            Visit(ast->exprs[i]);
            if (i + 1 < ast->exprs.size())
                Write(", ");
        }

        Write(" }");
    }
}

#undef IMPLEMENT_VISIT_PROC

/* --- Helper functions for code generation --- */

/* ----- Pre processing AST ----- */

void HLSLGenerator::PreProcessAST(const ShaderInput& inputDesc, const ShaderOutput& outputDesc)
{
    PreProcessStructParameterAnalyzer(inputDesc);
    PreProcessFuncNameConverter();
    PreProcessReferenceAnalyzer(inputDesc);
}

void HLSLGenerator::PreProcessStructParameterAnalyzer(const ShaderInput& inputDesc)
{
    /* Mark all structures that are used for another reason than entry-point parameter */
    StructParameterAnalyzer structAnalyzer;
    structAnalyzer.MarkStructsFromEntryPoint(*GetProgram(), inputDesc.shaderTarget);
}

void HLSLGenerator::PreProcessFuncNameConverter()
{
    /* Convert function names after main conversion, since functon owner structs may have been renamed as well */
    FuncNameConverter funcNameConverter;
    funcNameConverter.Convert(
        *GetProgram(),
        nameMangling_,
        [](const FunctionDecl& lhs, const FunctionDecl& rhs)
        {
            /* Compare function signatures and ignore generic sub types (GLSL has no distinction for these types) */
            return lhs.EqualsSignature(rhs, TypeDenoter::IgnoreGenericSubType);
        },
        FuncNameConverter::All
    );
}

void HLSLGenerator::PreProcessReferenceAnalyzer(const ShaderInput& inputDesc)
{
    /* Mark all reachable AST nodes */
    ReferenceAnalyzer refAnalyzer;
    refAnalyzer.MarkReferencesFromEntryPoint(*GetProgram(), inputDesc.shaderTarget);
}

/* ----- Basics ----- */

void HLSLGenerator::WriteComment(const std::string& text)
{
    std::size_t start = 0, end = 0;

    while (end < text.size())
    {
        /* Get next comment line */
        end = text.find('\n', start);

        auto line = (end < text.size() ? text.substr(start, end - start) : text.substr(start));

        /* Write comment line */
        BeginLn();
        {
            Write("// ");
            Write(line);
        }
        EndLn();

        start = end + 1;
    }
}

void HLSLGenerator::WriteLineMark(int lineNumber)
{
    if (allowLineMarks_)
        WriteLn("#line " + std::to_string(lineNumber));
}

void HLSLGenerator::WriteLineMark(const TokenPtr& tkn)
{
    WriteLineMark(tkn->Pos().Row());
}

void HLSLGenerator::WriteLineMark(const AST* ast)
{
    WriteLineMark(ast->area.Pos().Row());
}

/* ----- Global layouts ----- */

void HLSLGenerator::WriteGlobalLayouts()
{
    auto program = GetProgram();

    switch (GetShaderTarget())
    {
        case ShaderTarget::TessellationControlShader:
            WriteGlobalLayoutsTessControl(program->layoutTessControl);
            break;
        case ShaderTarget::TessellationEvaluationShader:
            WriteGlobalLayoutsTessEvaluation(program->layoutTessEvaluation);
            break;
        case ShaderTarget::GeometryShader:
            WriteGlobalLayoutsGeometry(program->layoutGeometry);
            break;
        case ShaderTarget::FragmentShader:
            WriteGlobalLayoutsFragment(program->layoutFragment);
            break;
        case ShaderTarget::ComputeShader:
            WriteGlobalLayoutsCompute(program->layoutCompute);
            break;
        default:
            break;
    }
}

void HLSLGenerator::WriteGlobalLayoutsTessControl(const Program::LayoutTessControlShader& layout)
{
    WriteLn("[outputcontrolpoints(" + std::to_string(layout.outputControlPoints) + ")]");
    WriteLn("[maxtessfactor(" + std::to_string(layout.maxTessFactor) + ")]");

    if(layout.patchConstFunctionRef)
        WriteLn("[patchconstantfunc(\"" + layout.patchConstFunctionRef->ident.Final() + "\")]");
}

void HLSLGenerator::WriteGlobalLayoutsTessEvaluation(const Program::LayoutTessEvaluationShader& layout)
{
    WriteLn("[domain(" + AttributeValueToHLSLKeyword(layout.domainType) + ")]");
    WriteLn("[partitioning(" + AttributeValueToHLSLKeyword(layout.partitioning) + ")]");
    WriteLn("[outputtopology(\"" + AttributeValueToHLSLKeyword(layout.outputTopology) + "\")]");
}

void HLSLGenerator::WriteGlobalLayoutsGeometry(const Program::LayoutGeometryShader& layout)
{
    WriteLn("[maxvertexcount(" + std::to_string(layout.maxVertices) + ")]");
}

void HLSLGenerator::WriteGlobalLayoutsFragment(const Program::LayoutFragmentShader& layout)
{
    if (layout.earlyDepthStencil)
        WriteLn("[early_fragment_tests]");
}

void HLSLGenerator::WriteGlobalLayoutsCompute(const Program::LayoutComputeShader& layout)
{
    WriteLn("[numthreads(" + 
        std::to_string(layout.numThreads[0]) + ", " + 
        std::to_string(layout.numThreads[1]) + ", " +
        std::to_string(layout.numThreads[2]) + ")]");
}

/* ----- Uniforms ----- */

void HLSLGenerator::WriteGlobalUniforms()
{
    bool uniformsWritten = false;

    for (auto& param : GetProgram()->entryPointRef->parameters)
    {
        if (param->IsUniform())
        {
            WriteGlobalUniformsParameter(param.get());
            uniformsWritten = true;
        }
    }

    if (uniformsWritten)
        Blank();
}

void HLSLGenerator::WriteGlobalUniformsParameter(VarDeclStmnt* param)
{
    /* Write uniform type */
    BeginLn();
    {
        Visit(param->typeSpecifier);
        Write(" ");

        /* Write parameter identifier */
        if (param->varDecls.size() == 1)
            Visit(param->varDecls.front());
        else
            Error(R_InvalidParamVarCount, param);

        Write(";");
    }
    EndLn();
}

/* ----- Object expression ----- */

void HLSLGenerator::WriteObjectExpr(const ObjectExpr& objectExpr)
{
    WriteObjectExprIdent(objectExpr);
}

void HLSLGenerator::WriteObjectExprIdent(const ObjectExpr& objectExpr, bool writePrefix)
{
    /* Write prefix expression */
    if (objectExpr.prefixExpr && !objectExpr.isStatic && writePrefix)
    {
        Visit(objectExpr.prefixExpr);

        if (auto literalExpr = objectExpr.prefixExpr->As<LiteralExpr>())
        {
            /* Append space between integer literal and '.' swizzle operator */
            if (literalExpr->IsSpaceRequiredForSubscript())
                Write(" ");
        }

        Write(".");
    }

    /* Write object identifier either from object expression or from symbol reference */
    if (auto symbol = objectExpr.symbolRef)
    {
        /* Write original identifier, if the identifier was marked as immutable */
        if (objectExpr.flags(ObjectExpr::isImmutable))
            Write(symbol->ident.Original());
        else
            Write(symbol->ident);
    }
    else
        Write(objectExpr.ident);
}

/* ----- Array expression ----- */

void HLSLGenerator::WriteArrayExpr(const ArrayExpr& arrayExpr)
{
    Visit(arrayExpr.prefixExpr);
    WriteArrayIndices(arrayExpr.arrayIndices);
}

void HLSLGenerator::WriteArrayIndices(const std::vector<ExprPtr>& arrayIndices)
{
    for (auto& arrayIndex : arrayIndices)
    {
        Write("[");
        Visit(arrayIndex);
        Write("]");
    }
}

/* ----- Type denoter ----- */

void HLSLGenerator::WriteStorageClasses(const std::set<StorageClass>& storageClasses, const AST* ast)
{
    for (auto entry : storageClasses)
    {
        auto keyword = StorageClassToHLSLKeyword(entry);
        Write(keyword + " ");
    }
}

void HLSLGenerator::WriteInterpModifiers(const std::set<InterpModifier>& interpModifiers, const AST* ast)
{
    for (auto entry : interpModifiers)
    {
        auto keyword = InterpModifierToHLSLKeyword(entry);
        Write(keyword + " ");
    }
}

void HLSLGenerator::WriteTypeModifiers(const std::set<TypeModifier>& typeModifiers, const TypeDenoterPtr& typeDenoter)
{
    for (auto entry : typeModifiers)
    {
        auto keyword = TypeModifierToHLSLKeyword(entry);
        Write(keyword + " ");
    }
}

void HLSLGenerator::WriteTypeModifiersFrom(const TypeSpecifierPtr& typeSpecifier)
{
    WriteTypeModifiers(typeSpecifier->typeModifiers, typeSpecifier->GetTypeDenoter()->GetSub());
}

void HLSLGenerator::WriteDataType(DataType dataType, const AST* ast)
{
    Write(DataTypeToString(dataType));
}

void HLSLGenerator::WriteTypeDenoter(const TypeDenoter& typeDenoter, const AST* ast)
{
    try
    {
        if (typeDenoter.IsVoid())
        {
            /* Just write void type */
            Write("void");
        }
        else if (auto baseTypeDen = typeDenoter.As<BaseTypeDenoter>())
        {
            /* Map HLSL base type */
            WriteDataType(baseTypeDen->dataType, ast);
        }
        else if (auto bufferTypeDen = typeDenoter.As<BufferTypeDenoter>())
        {
            /* Get buffer type */
            auto bufferType = bufferTypeDen->bufferType;
            if (bufferType == BufferType::Undefined)
            {
                if (auto bufferDecl = bufferTypeDen->bufferDeclRef)
                    bufferType = bufferDecl->GetBufferType();
                else
                    Error(R_MissingRefInTypeDen(R_BufferTypeDen), ast);
            }

            Write(BufferTypeToString(bufferType));

            bool hasArgs = false;
            if(bufferTypeDen->genericTypeDenoter)
            {
                hasArgs = true;

                Write("<");
                WriteTypeDenoter(*bufferTypeDen->genericTypeDenoter, ast);
            }

            if (IsTextureMSBufferType(bufferType) || IsPatchBufferType(bufferType))
            {
                if (hasArgs)
                    Write(", ");
                else
                {
                    Write("<");
                    hasArgs = true;
                }

                Write(std::to_string(bufferTypeDen->genericSize));
            }

            if(hasArgs)
                Write(">");
        }
        else if (auto samplerTypeDen = typeDenoter.As<SamplerTypeDenoter>())
        {
            /* Get sampler type */
            auto samplerType = samplerTypeDen->samplerType;
            if (samplerType == SamplerType::Undefined)
            {
                if (auto samplerDecl = samplerTypeDen->samplerDeclRef)
                    samplerType = samplerDecl->GetSamplerType();
                else
                    Error(R_MissingRefInTypeDen(R_SamplerTypeDen), ast);
            }

            Write(SamplerTypeToString(samplerType));
        }
        else if (auto structTypeDen = typeDenoter.As<StructTypeDenoter>())
        {
            /* Write struct identifier (either from structure declaration or stored identifier) */
            if (auto structDecl = structTypeDen->structDeclRef)
                Write(structDecl->ident);
            else
                Write(typeDenoter.Ident());
        }
        else if (typeDenoter.IsAlias())
        {
            /* Write aliased type denoter */
            WriteTypeDenoter(typeDenoter.GetAliased(), ast);
        }
        else if (auto arrayTypeDen = typeDenoter.As<ArrayTypeDenoter>())
        {
            /* Write sub type of array type denoter and array dimensions */
            WriteTypeDenoter(*arrayTypeDen->subTypeDenoter, ast);
            Visit(arrayTypeDen->arrayDims);
        }
        else
            Error(R_FailedToDetermineGLSLDataType, ast);
    }
    catch (const Report&)
    {
        throw;
    }
    catch (const std::exception& e)
    {
        Error(e.what(), ast);
    }
}

/* ----- Function declaration ----- */

void HLSLGenerator::WriteFunction(FunctionDecl* ast)
{
    /* Write function header */
    BeginLn();
    Visit(ast->returnType);
    Write(" " + ast->ident + "(");

    /* Write parameters */
    for (std::size_t i = 0; i < ast->parameters.size(); ++i)
    {
        WriteParameter(ast->parameters[i].get());
        if (i + 1 < ast->parameters.size())
            Write(", ");
    }

    Write(")");

    if(ast->semantic != Semantic::Undefined)
        Write(" : " + ast->semantic.ToString());

    if (ast->codeBlock)
    {
        /* Write function body */
        Visit(ast->codeBlock);
    }
    else
    {
        /* This is only a function forward declaration, so finish with statement terminator */
        Write(";");
        EndLn();
    }
}

/* ----- Function call ----- */

void HLSLGenerator::AssertIntrinsicNumArgs(CallExpr* funcCall, std::size_t numArgsMin, std::size_t numArgsMax)
{
    auto numArgs = funcCall->arguments.size();
    if (numArgs < numArgsMin || numArgs > numArgsMax)
        Error(R_InvalidIntrinsicArgCount(funcCall->ident), funcCall);
}

void HLSLGenerator::WriteCallExprStandard(CallExpr* funcCall)
{
    if(funcCall->prefixExpr)
    {
        Visit(funcCall->prefixExpr);
        Write(".");
    }

    /* Write function name */
    if (funcCall->intrinsic != Intrinsic::Undefined)
    {
        if (!funcCall->ident.empty())
        {
            /* Write wrapper function name */
            Write(funcCall->ident);
        }
        else
            Error(R_MissingFuncName, funcCall);
    }
    else if (auto funcDecl = funcCall->GetFunctionImpl())
    {
        /* Write final identifier of function declaration */
        Write(funcDecl->ident);
    }
    else if (funcCall->flags(CallExpr::isWrapperCall))
    {
        /* Write expression identifier */
        Write(funcCall->ident);
    }
    else if (funcCall->typeDenoter)
    {
        /* Write type denoter */
        WriteTypeDenoter(*funcCall->typeDenoter, funcCall);
    }
    else
        Error(R_MissingFuncName, funcCall);

    /* Write arguments */
    Write("(");
    WriteCallExprArguments(funcCall);
    Write(")");
}

void HLSLGenerator::WriteCallExprArguments(CallExpr* callExpr, std::size_t firstArgIndex, std::size_t numWriteArgs)
{
    if (numWriteArgs <= numWriteArgs + firstArgIndex)
        numWriteArgs = numWriteArgs + firstArgIndex;
    else
        numWriteArgs = ~0u;

    const auto n = callExpr->arguments.size();
    const auto m = std::min(numWriteArgs, n + callExpr->defaultArgumentRefs.size());

    for (std::size_t i = firstArgIndex; i < m; ++i)
    {
        if (i < n)
            Visit(callExpr->arguments[i]);
        else
            Visit(callExpr->defaultArgumentRefs[i - n]);

        if (i + 1 < m)
            Write(", ");
    }
}

/* ----- Structure ----- */

bool HLSLGenerator::WriteStructDecl(StructDecl* structDecl, bool endWithSemicolon)
{
    /* Write structure signature */
    BeginLn();

    Write("struct");
    if (!structDecl->ident.Empty())
        Write(' ' + structDecl->ident);

    /* Write structure members */
    WriteScopeOpen(false, endWithSemicolon);
    BeginSep();
    {
        Visit(structDecl->varMembers);
    }
    EndSep();
    WriteScopeClose();
    
    /* Only append blank line if struct is not part of a variable declaration */
    if (!InsideVarDeclStmnt())
        Blank();

    /* Write member functions */
    std::vector<BasicDeclStmnt*> funcMemberStmnts;
    funcMemberStmnts.reserve(structDecl->funcMembers.size());

    for (auto& funcDecl : structDecl->funcMembers)
        funcMemberStmnts.push_back(funcDecl->declStmntRef);

    WriteStmntList(funcMemberStmnts);

    return true;
}

/* ----- BufferDecl ----- */

void HLSLGenerator::WriteBufferDecl(BufferDecl* bufferDecl)
{
    Write(bufferDecl->ident);
    Visit(bufferDecl->arrayDims);
}

/* ----- SamplerDecl ----- */

void HLSLGenerator::WriteSamplerDecl(SamplerDecl& samplerDecl)
{
    auto samplerTypeKeyword = SamplerTypeToString(samplerDecl.GetSamplerType());

    BeginLn();
    {
        /* Write uniform sampler declaration (sampler declarations must only appear in global scope) */
        Write(samplerTypeKeyword + " " + samplerDecl.ident);

        /* Write array dimensions and statement terminator */
        Visit(samplerDecl.arrayDims);
        Write(";");
    }
    EndLn();

    Blank();
}

/* ----- Misc ----- */

void HLSLGenerator::WriteStmntComment(Stmnt* ast, bool insertBlank)
{
    if (ast && !ast->comment.empty())
    {
        if (insertBlank)
            Blank();
        WriteComment(ast->comment);
    }
}

template <typename T>
T* GetRawPtr(T* ptr)
{
    return ptr;
}

template <typename T>
T* GetRawPtr(const std::shared_ptr<T>& ptr)
{
    return ptr.get();
}

template <typename T>
void HLSLGenerator::WriteStmntList(const std::vector<T>& stmnts, bool isGlobalScope)
{
    if (preserveComments_)
    {
        /* Write statements with optional commentaries */
        for (std::size_t i = 0; i < stmnts.size(); ++i)
        {
            auto ast = GetRawPtr(stmnts[i]);

            if (!isGlobalScope || ast->flags(AST::isReachable))
                WriteStmntComment(ast, (!isGlobalScope && (i > 0)));

            Visit(ast);
        }
    }
    else
    {
        /* Write statements only */
        Visit(stmnts);
    }
}

void HLSLGenerator::WriteParameter(VarDeclStmnt* ast)
{
    /* Write input modifier */
    if (ast->IsOutput())
    {
        if (ast->IsInput())
            Write("inout ");
        else
            Write("out ");
    }

    /* Write type modifiers */
    WriteTypeModifiersFrom(ast->typeSpecifier);

    /* Write parameter type */
    Visit(ast->typeSpecifier);
    Write(" ");

    /* Write parameter identifier (without default initializer) */
    if (ast->varDecls.size() == 1)
    {
        auto paramVar = ast->varDecls.front().get();
        Write(paramVar->ident);
        Visit(paramVar->arrayDims);

        /* Write semantic, if any */
        if (paramVar->semantic != Semantic::Undefined)
            Write(" : " + paramVar->semantic.ToString());
    }
    else
        Error(R_InvalidParamVarCount, ast);
}

void HLSLGenerator::WriteScopedStmnt(Stmnt* ast)
{
    if (ast)
    {
        if (ast->Type() != AST::Types::CodeBlockStmnt)
        {
            WriteScopeOpen(false, false, alwaysBracedScopes_);
            {
                Visit(ast);
            }
            WriteScopeClose();
        }
        else
            Visit(ast);
    }
}

void HLSLGenerator::WriteLiteral(const std::string& value, const DataType& dataType, const AST* ast)
{
    if (IsScalarType(dataType))
    {
        Write(value);

        switch (dataType)
        {
            case DataType::UInt:
                if (!value.empty() && value.back() != 'u' && value.back() != 'U')
                    Write("u");
                break;
            case DataType::Float:
                if (value.find_first_of(".eE") == std::string::npos)
                    Write(".0");
                Write("f");
                break;
            default:
                break;
        }
    }
    else if (IsVectorType(dataType))
    {
        WriteDataType(dataType, ast);
        Write("(");
        Write(value);
        Write(")");
    }
    else
        Error(R_FailedToWriteLiteralType(value), ast);
}


} // /namespace Xsc



// ================================================================================
