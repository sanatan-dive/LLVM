#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include <set>

using namespace llvm;

namespace {

struct InstrumentationPass : public FunctionPass {
  static char ID;
  InstrumentationPass() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;

private:
  bool instrumentBasicBlocks(Function &F);
  bool instrumentMemoryAccesses(Function &F);
  bool instrumentBranches(Function &F);
  void insertCounterIncrement(IRBuilder<> &Builder, Function *CounterFunc, Value *CounterID);
  Function *createOrGetProfileFunction(Module *M, StringRef Name, Type *ArgType);
};

bool InstrumentationPass::runOnFunction(Function &F) {
  if (F.isDeclaration())
    return false;

  bool Modified = false;
  
  // Add basic block execution counters
  Modified |= instrumentBasicBlocks(F);
  
  // Add memory access tracking
  Modified |= instrumentMemoryAccesses(F);
  
  // Add branch prediction tracking
  Modified |= instrumentBranches(F);
  
  return Modified;
}

bool InstrumentationPass::instrumentBasicBlocks(Function &F) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  
  // Create/get the profiling function
  Function *CounterFunc = createOrGetProfileFunction(M, "incrementBlockCounter", 
                                                    Type::getInt32Ty(Ctx));
  
  // Add counter increment at the start of each basic block
  unsigned BlockID = 0;
  for (auto &BB : F) {
    IRBuilder<> Builder(&*BB.begin());
    Value *CounterVal = ConstantInt::get(Type::getInt32Ty(Ctx), BlockID);
    insertCounterIncrement(Builder, CounterFunc, CounterVal);
    BlockID++;
  }
  
  return BlockID > 0;
}

bool InstrumentationPass::instrumentMemoryAccesses(Function &F) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  
  // Create/get memory access profiling functions
  Function *LoadFunc = createOrGetProfileFunction(M, "profileLoad", Type::getInt8PtrTy(Ctx));
  Function *StoreFunc = createOrGetProfileFunction(M, "profileStore", Type::getInt8PtrTy(Ctx));
  
  bool Modified = false;
  
  // Add instrumentation for each load and store
  for (auto &BB : F) {
    for (auto I = BB.begin(); I != BB.end(); ++I) {
      if (LoadInst *LI = dyn_cast<LoadInst>(&*I)) {
        IRBuilder<> Builder(LI->getNextNode());
        Value *PtrOperand = LI->getPointerOperand();
        Value *CastPtr = Builder.CreateBitCast(PtrOperand, Type::getInt8PtrTy(Ctx));
        Builder.CreateCall(LoadFunc, {CastPtr});
        Modified = true;
      } else if (StoreInst *SI = dyn_cast<StoreInst>(&*I)) {
        IRBuilder<> Builder(SI->getNextNode());
        Value *PtrOperand = SI->getPointerOperand();
        Value *CastPtr = Builder.CreateBitCast(PtrOperand, Type::getInt8PtrTy(Ctx));
        Builder.CreateCall(StoreFunc, {CastPtr});
        Modified = true;
      }
    }
  }
  
  return Modified;
}

bool InstrumentationPass::instrumentBranches(Function &F) {
  Module *M = F.getParent();
  LLVMContext &Ctx = M->getContext();
  
  // Create/get branch profiling function
  Function *BranchFunc = createOrGetProfileFunction(M, "profileBranch", 
                                                   Type::getInt32Ty(Ctx));
  
  bool Modified = false;
  
  // Add instrumentation for conditional branches
  for (auto &BB : F) {
    if (BranchInst *BI = dyn_cast<BranchInst>(BB.getTerminator())) {
      if (BI->isConditional()) {
        IRBuilder<> Builder(BI);
        Value *Condition = BI->getCondition();
        Value *ConditionInt = Builder.CreateZExt(Condition, Type::getInt32Ty(Ctx));
        Builder.CreateCall(BranchFunc, {ConditionInt});
        Modified = true;
      }
    }
  }
  
  return Modified;
}

void InstrumentationPass::insertCounterIncrement(IRBuilder<> &Builder, 
                                               Function *CounterFunc, 
                                               Value *CounterID) {
  Builder.CreateCall(CounterFunc, {CounterID});
}

Function *InstrumentationPass::createOrGetProfileFunction(Module *M, 
                                                        StringRef Name, 
                                                        Type *ArgType) {
  LLVMContext &Ctx = M->getContext();
  
  // Check if the function already exists
  if (Function *F = M->getFunction(Name))
    return F;
    
  // Create function type and declaration
  FunctionType *FTy = FunctionType::get(Type::getVoidTy(Ctx), {ArgType}, false);
  Function *F = Function::Create(FTy, Function::ExternalLinkage, Name, M);
  
  return F;
}

char InstrumentationPass::ID = 0;
static RegisterPass<InstrumentationPass>
    W("instrumentation", "Instrumentation Pass for Runtime Profiling");

}