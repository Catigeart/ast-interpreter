//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//

/// shit mountain re-construction 3rd ///

#include <cstdio>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

/// record type info for further extension
/// but I don't think I will continue this project...

/// give up.
/*
class Var {
    QualType type;
    int64_t val;
public:
    Var operator + (Var& b) { return {this->type, this->val + b.val}; }
    Var operator - (Var& b) { return {this->type, this->val - b.val}; }
    Var operator * (Var& b) { return {this->type, this->val * b.val}; }
    Var operator / (Var& b) { return {this->type, this->val / b.val}; }
    bool operator == (Var& b) { return this->val == b.val; }

    Var() : type(), val(0) {}
    Var(QualType qType, int64_t value) : type(qType), val(value) {}
};
*/

class StackFrame {
   /// StackFrame maps Variable Declaration to Value
   /// Which are either integer or addresses (also represented using an Integer value)
   std::map<Decl*, int64_t> mVars{}; // all declarations
   std::map<Stmt*, int64_t> mExprs{}; // all exprs and their value
   /// The current stmt
   // Stmt * mPC; // useless.
public:
   // StackFrame() : mVars(), mExprs() {}

   void bindDecl(Decl* decl, int64_t val) {
      mVars[decl] = val;
   }    
   int64_t getDeclVal(Decl * decl) {
      assert (mVars.find(decl) != mVars.end());
      return mVars.find(decl)->second;
   }
   bool hasDecl(Decl * decl) {
       return mVars.find(decl) != mVars.end();
   }
   void bindStmt(Stmt * stmt, int64_t val) {
	   mExprs[stmt] = val;
   }
   int64_t getStmtVal(Stmt * stmt) {
	   assert (mExprs.find(stmt) != mExprs.end());
	   return mExprs[stmt];
   }
   bool hasStmt(Stmt * stmt) {
       return mExprs.find(stmt) != mExprs.end();
   }
   /*
   void setPC(Stmt * stmt) {
	   mPC = stmt;
   }
   Stmt * getPC() {
	   return mPC;
   }
   */
};

/// Heap maps address to a value
/// call malloc() and free() directly. -- fish touching
/*
class Heap {
public:
   int Malloc(int size) ;
   void Free (int addr) ;
   void Update(int addr, int val) ;
   int get(int addr);
};
*/

class Environment {
   std::vector<StackFrame> mStack;

   FunctionDecl * mFree{};				/// Declarations to the built-in functions
   FunctionDecl * mMalloc{};
   FunctionDecl * mInput{};
   FunctionDecl * mOutput{};

   FunctionDecl * mEntry{}; /// main

   std::map<Decl*, int64_t> gVars{}; /// global variable
   int64_t retVal{}; /// function's return value
public:
   /// Get the declarations to the built-in functions
   // Environment() : mStack(), mFree(nullptr), mMalloc(nullptr), mInput(nullptr), mOutput(nullptr), mEntry(nullptr), retVar() {}

   /** init and getEntry **/
   /// Initialize the Environment
   void init(TranslationUnitDecl * unit) {
       mStack.emplace_back(); /// tmp frame to calculate gVars' stmt
	   for (TranslationUnitDecl::decl_iterator i =unit->decls_begin(), e = unit->decls_end(); i != e; ++ i) {
		   if (auto * fdecl = dyn_cast<FunctionDecl>(*i) ) {
			   if (fdecl->getName().equals("FREE")) mFree = fdecl;
			   else if (fdecl->getName().equals("MALLOC")) mMalloc = fdecl;
			   else if (fdecl->getName().equals("GET")) mInput = fdecl;
			   else if (fdecl->getName().equals("PRINT")) mOutput = fdecl;
			   else if (fdecl->getName().equals("main")) mEntry = fdecl;
		   }
           else if (auto *varDecl = dyn_cast<VarDecl>(*i)) {
               // global value
               // literals are not be recorded
               if (varDecl->hasInit()) {
                   Expr *initStmt = varDecl->getInit();
                   gVars[varDecl] = calculateExpr(initStmt);
               } else {
                   gVars[varDecl] = 0;
               }
           }
	   }
	   mStack.pop_back();
       mStack.emplace_back(); /// main function's scope
   }

   FunctionDecl * getEntry() {
	   return mEntry;
   }

    /** decl and declRef **/
    void decl(DeclStmt * declStmt) {
        for (DeclStmt::decl_iterator it = declStmt->decl_begin(), ie = declStmt->decl_end(); it != ie; ++ it) {
            Decl * decl = *it;
            if (auto * varDecl = dyn_cast<VarDecl>(decl)) {
                QualType type = varDecl->getType();
                int64_t val = 0;
                if (type->isIntegerType() || type->isPointerType()) {
                    if (varDecl->hasInit()) {
                        val = calculateExpr(varDecl->getInit());
                    }
                }
                else if (type->isArrayType()) { /// no init
                    const auto *pArrayType = dyn_cast<ConstantArrayType>(type.getTypePtr());
                    int64_t size = pArrayType->getSize().getSExtValue();
                    auto *arr = new int64_t[size];
                    for (int64_t i = 0; i < size; i++) {
                        arr[i] = 0;
                    }
                    val = (int64_t) arr;
                }
                else {
                    llvm::errs() << "Unhandle decl value type.\n";
                    assert(false);
                }
                mStack.back().bindDecl(varDecl, val);
            }
        }
    }

    void declRef(DeclRefExpr * declRef) {
        // mStack.back().setPC(declref);
        QualType type = declRef->getType();
        if (type->isIntegerType() || type->isArrayType() || type->isPointerType()) {
            Decl *decl = declRef->getFoundDecl();
            int64_t val = 0;
            if (mStack.back().hasDecl(decl)) {
                val = mStack.back().getDeclVal(decl);
            } else {
                assert(gVars.find(decl) != gVars.end());
                val = gVars[decl];
            }
            mStack.back().bindStmt(declRef, val);
        }
    }

    /** cast **/
    void cast(CastExpr * castExpr) {
        // mStack.back().setPC(castexpr);
        QualType type = castExpr->getType();
        if (type->isIntegerType() || (type->isPointerType() && !type->isFunctionPointerType())) {
            Expr *subExpr = castExpr->getSubExpr();
            int64_t val = calculateExpr(subExpr);
            if (isa<ArraySubscriptExpr>(subExpr)) {
                val = *(int64_t*)val;
            }
            mStack.back().bindStmt(castExpr, val);
        }
    }

    /** array **/
    void array(ArraySubscriptExpr *arrayExpr) {
        Expr *base = arrayExpr->getBase();
        Expr *idx = arrayExpr->getIdx();
        int64_t *basePtr;
        // super ugly code
        // completly code by dump
        /*
        if (auto * castExpr = dyn_cast<ImplicitCastExpr>(base)) {
            Expr * expr = castExpr->getSubExpr();
            if (auto * declExpr = dyn_cast<DeclRefExpr>(expr)) {
                Decl * decl = declExpr->getFoundDecl();
                basePtr = (int64_t *) mStack.back().getDeclVal(decl);
            }
            else {
                llvm::errs() << "1\n";
                base->dump();
            }
        }
        else {
            llvm::errs() << "2\n";
            base->dump();
        }
        */
        /*
        if (auto * declRef = dyn_cast<DeclRefExpr>(base)) {
            basePtr = (int64_t *)mStack.back().getDeclVal(declRef->getFoundDecl());
        }

        else {
            llvm::errs() << "unhandle array base type.\n";
            assert(false);
        }
         */
        basePtr = (int64_t *) mStack.back().getStmtVal(base);
        int64_t idxVal = calculateExpr(idx);
        /// arrayExpr should be its value, not pointer
        mStack.back().bindStmt(arrayExpr, *(basePtr + idxVal));
    }

    /** (size of only) **/
    void uett(UnaryExprOrTypeTraitExpr *uettExpr) {
        UnaryExprOrTypeTrait kind = uettExpr->getKind();
        int64_t result;
        switch (kind) {
            case UETT_SizeOf:
                result = (sizeof(int64_t));
                break;
            default:
                llvm::errs() << "Unhandled uett.";
                assert(false);
        }
        mStack.back().bindStmt(uettExpr, result);
    }

    /** paren "(i)" **/
    void paren(ParenExpr *parenExpr) {
        int64_t val = calculateExpr(parenExpr->getSubExpr());
        mStack.back().bindStmt(parenExpr,val);
    }

    /** op calculation **/
   void binOp(BinaryOperator *bop) {
	   Expr * left = bop->getLHS();
	   Expr * right = bop->getRHS();
       int64_t rightVal = calculateExpr(right);
       int64_t result = 0;
       if (bop->isAssignmentOp()) {
           /* left expr:
            * 1. a
            * 2. a[1]
            * 3. *a
            * */
           if (auto * declExpr = dyn_cast<DeclRefExpr>(left)) {
               Decl * decl = declExpr->getFoundDecl();
               mStack.back().bindDecl(decl, rightVal);
           }
           else if (auto * arrayExpr = dyn_cast<ArraySubscriptExpr>(left)) {
               auto *ptr = (int64_t *) calculateExpr(arrayExpr);
               *ptr = rightVal;
           }
           else if (auto *uop = dyn_cast<UnaryOperator>(left)) {
               if (uop->getOpcode() == UO_Deref) {
                   auto *ptr = (int64_t *) calculateExpr(uop->getSubExpr());
                   *ptr = rightVal;
               }
               else {
                   llvm::errs() << "unhandle uop for bop left-expr.\n";
                   assert(false);
               }
           }

           if (isa<ArraySubscriptExpr>(right)) {
               rightVal = *(int64_t*)rightVal;
           }

           mStack.back().bindStmt(left, rightVal);
           result = rightVal;
       }
       else {
           BinaryOperatorKind opCode = bop->getOpcode();
           int64_t leftVal = calculateExpr(left);

           // integer + pointer
           if (left->getType()->isPointerType() && right->getType()->isIntegerType()) {
               assert(opCode == BO_Add || opCode == BO_Sub);
               rightVal *= sizeof(int64_t);
           } else if (left->getType()->isIntegerType() && right->getType()->isPointerType()) {
               assert(opCode == BO_Add || opCode == BO_Sub);
               leftVal *= sizeof(int64_t);
           }

           switch (opCode) {
               case BO_Add:
                   result = leftVal + rightVal;
                   break;
               case BO_Sub:
                   result = leftVal - rightVal;
                   break;
               case BO_Mul:
                   result = leftVal * rightVal;
                   break;
               case BO_Div:
                   result = leftVal / rightVal;
                   break;
               case BO_EQ:
                   result = leftVal == rightVal;
                   break;
               case BO_NE:
                   result = leftVal != rightVal;
                   break;
               case BO_LT:
                   result = leftVal < rightVal;
                   break;
               case BO_GT:
                   result = leftVal > rightVal;
                   break;
               case BO_LE:
                   result = leftVal <= rightVal;
                   break;
               case BO_GE:
                   result = leftVal >= rightVal;
                   break;
               default:
                   llvm::errs() << "unhandle binop.\n";
                   assert(false);
           }
       }

       mStack.back().bindStmt(bop, result);
   }

   void unaryOp(UnaryOperator *uop) {
       UnaryOperatorKind opCode = uop->getOpcode();
       int64_t rightVal = calculateExpr(uop->getSubExpr());
       int64_t result = 0;

       switch (opCode) {
           case UO_Plus:
               result = rightVal;
               break;
           case UO_Minus:
               result = -rightVal;
               break;
           case UO_Deref:
               result = *(int64_t *) rightVal;
               break;
           default:
               llvm::errs() << "unhandle unary op.\n";
               assert(false);
       }
       mStack.back().bindStmt(uop, result);
   }

   /** function **/
   void returnStmt(Expr *retExpr) {
       retVal = calculateExpr(retExpr);
   }

   void inFunc(CallExpr * callExpr) {
       FunctionDecl *callee = callExpr->getDirectCallee();
       size_t paramCount = callee->getNumParams();
       StackFrame frame{};
       for (int i = 0; i < paramCount; i++) {
           ParmVarDecl *parmVarDecl = callee->getParamDecl(i);
           int64_t val = calculateExpr(callExpr->getArg(i));
           frame.bindDecl(parmVarDecl, val);
       }
       mStack.emplace_back(frame);
   }

   void outFunc(CallExpr * callExpr) {
       mStack.pop_back();
       mStack.back().bindStmt(callExpr, retVal);
   }

   bool isBuiltInCall(CallExpr * callExpr) {
	   // mStack.back().setPC(callexpr);
	   int64_t val = 0;
	   FunctionDecl * callee = callExpr->getDirectCallee();
	   if (callee == mInput) {
		  llvm::errs() << "Please Input an Integer Value : ";
		  scanf("%ld", &val);
		  mStack.back().bindStmt(callExpr, val);
          return true;
	   } else if (callee == mOutput) {
		   Expr * decl = callExpr->getArg(0);
		   val = calculateExpr(decl);
		   llvm::errs() << val;
           mStack.back().bindStmt(callExpr, val);
           return true;
       } else if (callee == mMalloc) {
           int64_t size = calculateExpr(callExpr->getArg(0));
           mStack.back().bindStmt(callExpr, (int64_t)malloc(size));
           return true;
       } else if (callee == mFree) {
           auto *ptr = (int64_t *) calculateExpr(callExpr->getArg(0));
           free(ptr);
           return true;
	   } else {
		   /// You could add your code here for Function call Return
		   return false;
	   }
   }

   /** util **/
   /// get literal directly to avoid recording too much stmt(expr)
   int64_t calculateExpr(Expr * expr) {
       int64_t val;
       if (auto * iLiteral = dyn_cast<IntegerLiteral>(expr)) {
           val = iLiteral->getValue().getSExtValue();
       }
       else if (auto * cLiteral = dyn_cast<CharacterLiteral>(expr)) {
           val = cLiteral->getValue();
       }
       else if (auto * arrayExpr = dyn_cast<ArraySubscriptExpr>(expr)) {
           Expr * base = arrayExpr->getBase();
           Expr * idx = arrayExpr->getIdx();
           auto *basePtr = (int64_t *) mStack.back().getStmtVal(base);
           int64_t idxVal = calculateExpr(idx);
           val = (int64_t)(basePtr + idxVal);
       }
       else {
           val = mStack.back().getStmtVal(expr);
       }
       return val;
   }
};


