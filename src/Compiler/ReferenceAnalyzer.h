/*
 * ReferenceAnalyzer.h
 * 
 * This file is part of the XShaderCompiler project (Copyright (c) 2014-2016 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#ifndef XSC_REFERENCE_ANALYZER_H
#define XSC_REFERENCE_ANALYZER_H


#include "Visitor.h"
#include "Token.h"
#include "SymbolTable.h"


namespace Xsc
{


/*
Object reference analyzer.
This helper class for the context analyzer marks all functions
which are used from the beginning of the shader entry point.
All other functions will be removed from the code generation.
*/
class ReferenceAnalyzer : private Visitor
{
    
    public:
        
        ReferenceAnalyzer(const ASTSymbolTable& symTable);

        // Marks all declarational AST nodes (i.e. function decl, structure decl etc.) that are reachable from the specififed entry point.
        void MarkReferencesFromEntryPoint(FunctionDecl* ast, Program* program);

    private:
        
        using OnOverrideProc = ASTSymbolTable::OnOverrideProc;

        /* === Visitor implementation === */

        DECL_VISIT_PROC( Program           );
        DECL_VISIT_PROC( CodeBlock         );
        DECL_VISIT_PROC( FunctionCall      );
        DECL_VISIT_PROC( Structure         );
        DECL_VISIT_PROC( SwitchCase        );

        DECL_VISIT_PROC( FunctionDecl      );
        DECL_VISIT_PROC( UniformBufferDecl );
        DECL_VISIT_PROC( StructDecl        );

        DECL_VISIT_PROC( CodeBlockStmnt    );
        DECL_VISIT_PROC( ForLoopStmnt      );
        DECL_VISIT_PROC( WhileLoopStmnt    );
        DECL_VISIT_PROC( DoWhileLoopStmnt  );
        DECL_VISIT_PROC( IfStmnt           );
        DECL_VISIT_PROC( ElseStmnt         );
        DECL_VISIT_PROC( SwitchStmnt       );
        DECL_VISIT_PROC( VarDeclStmnt      );
        DECL_VISIT_PROC( AssignStmnt       );
        DECL_VISIT_PROC( FunctionCallStmnt );
        DECL_VISIT_PROC( ReturnStmnt       );
        DECL_VISIT_PROC( StructDeclStmnt   );

        DECL_VISIT_PROC( ListExpr          );
        DECL_VISIT_PROC( BinaryExpr        );
        DECL_VISIT_PROC( UnaryExpr         );
        DECL_VISIT_PROC( PostUnaryExpr     );
        DECL_VISIT_PROC( FunctionCallExpr  );
        DECL_VISIT_PROC( BracketExpr       );
        DECL_VISIT_PROC( CastExpr          );
        DECL_VISIT_PROC( VarAccessExpr     );
        DECL_VISIT_PROC( InitializerExpr   );

        DECL_VISIT_PROC( VarType           );
        DECL_VISIT_PROC( VarIdent          );
        DECL_VISIT_PROC( VarDecl           );

        /* --- Helper functions for analysis --- */

        void MarkTextureReference(AST* ast, const std::string& texIdent);

        /* === Members === */

        Program*                program_    = nullptr;
        const ASTSymbolTable*   symTable_   = nullptr;

};


} // /namespace Xsc


#endif



// ================================================================================