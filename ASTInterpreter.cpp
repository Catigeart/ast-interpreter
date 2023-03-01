//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//

#include <fstream>
#include <sstream>
#include <string>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

#include "Environment.h"

class InterpreterVisitor : 
   public EvaluatedExprVisitor<InterpreterVisitor> {
public:
   explicit InterpreterVisitor(const ASTContext &context, Environment * env)
   : EvaluatedExprVisitor(context), mEnv(env) {}
   virtual ~InterpreterVisitor() = default;

   /** Decl and DeclRef **/
   virtual void VisitDeclStmt(DeclStmt * declstmt) {
       mEnv->decl(declstmt);
   }

   virtual void VisitDeclRefExpr(DeclRefExpr * expr) {
       VisitStmt(expr);
       mEnv->declRef(expr);
   }

   /** no op Expr **/
   virtual void VisitCastExpr(CastExpr * expr) {
       VisitStmt(expr);
       mEnv->cast(expr);
   }

   virtual void VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *expr){
       // sizeof int64_t only, return 8
       mEnv->uett(expr);
   }

   virtual void VisitArraySubscriptExpr(ArraySubscriptExpr *arrayexpr) {
       VisitStmt(arrayexpr);
       mEnv->array(arrayexpr);
   }

   virtual void VisitParenExpr(ParenExpr *parenexpr) {
       VisitStmt(parenexpr);
       mEnv->paren(parenexpr);
   }

   /** op Expr **/
   virtual void VisitBinaryOperator (BinaryOperator * bop) {
       VisitStmt(bop);
       mEnv->binOp(bop);
   }

   virtual void VisitUnaryOperator(UnaryOperator *uop) {
       VisitStmt(uop);
       mEnv->unaryOp(uop);
   }

   /** control flow **/
   virtual void VisitIfStmt(IfStmt *ifstmt) {
       Expr *cond = ifstmt->getCond();
       Visit(cond);

       if (mEnv->calculateExpr(cond)) {
           Visit(ifstmt->getThen());
       }
       else {
           if (Stmt * elseStmt = ifstmt->getElse()) {
               Visit(elseStmt);
           }
       }
   }

    virtual void VisitWhileStmt(WhileStmt *whilestmt) {
        Expr *cond = whilestmt->getCond();
        Stmt *body = whilestmt->getBody();

        Visit(cond);

        while (mEnv->calculateExpr(cond)) {
            Visit(body);
            Visit(cond);
        }
    }

    virtual void VisitForStmt(ForStmt *forstmt) {
        Stmt *init = forstmt->getInit();
        Expr *cond = forstmt->getCond();
        Expr *inc = forstmt->getInc();
        Stmt *body = forstmt->getBody();

        if (init) {
            Visit(init);
        }
        if (cond) {
            Visit(cond);
        }

        while (mEnv->calculateExpr(cond)) {
            Visit(body);
            if (inc) {
                Visit(inc);
            }
            if (cond) {
                Visit(cond);
            }
        }
    }

   /** func **/
   virtual void VisitReturnStmt(ReturnStmt *ret) {
       VisitStmt(ret);
       mEnv->returnStmt(ret->getRetValue());
   }

   virtual void VisitCallExpr(CallExpr * call) {
       VisitStmt(call);

       if(mEnv->isBuiltInCall(call)) {
           return;
       }

       mEnv->inFunc(call);
       VisitStmt(call->getDirectCallee()->getBody());
       mEnv->outFunc(call);
   }

private:
   Environment * mEnv;
};

class InterpreterConsumer : public ASTConsumer {
public:
   explicit InterpreterConsumer(const ASTContext& context) : mEnv(),
   	   mVisitor(context, &mEnv) {
   }
   ~InterpreterConsumer() override = default;

   void HandleTranslationUnit(clang::ASTContext &Context) override {
	   TranslationUnitDecl * decl = Context.getTranslationUnitDecl();
	   mEnv.init(decl);

	   FunctionDecl * entry = mEnv.getEntry();
	   mVisitor.VisitStmt(entry->getBody());
  }
private:
   Environment mEnv;
   InterpreterVisitor mVisitor;
};

class InterpreterClassAction : public ASTFrontendAction {
public: 
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) override {
    return std::unique_ptr<clang::ASTConsumer>(
        new InterpreterConsumer(Compiler.getASTContext()));
  }
};

std::string filename2Str(char * filename) {
    std::ifstream iFile(filename);
    std::ostringstream buf;
    char ch;
    while(buf && iFile.get(ch))
        buf.put(ch);
    return buf.str();
}

int main (int argc, char ** argv) {
   if (argc > 1) {
       clang::tooling::runToolOnCode(std::unique_ptr<clang::FrontendAction>(new InterpreterClassAction), argv[1]);
   }
   
/*
   if (argc > 1) {
       std::string fileStr = filename2Str(argv[1]);
       clang::tooling::runToolOnCode(std::unique_ptr<clang::FrontendAction>(new InterpreterClassAction), fileStr);
   }
*/

}
