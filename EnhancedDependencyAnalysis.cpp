#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <map>
#include <set>
#include <vector>

using namespace llvm;

namespace {

// DependencyInfo tracks dependency information for instructions
struct DependencyInfo {
  std::set<Instruction*> DataDependencies;
  std::set<Instruction*> ControlDependencies;
  std::set<Instruction*> MemoryDependencies;
  unsigned CriticalPathLength = 0;
};

class EnhancedDependencyAnalysis : public FunctionPass {
public:
  static char ID;
  EnhancedDependencyAnalysis() : FunctionPass(ID) {}

  // Dependency map for the entire function
  std::map<Instruction*, DependencyInfo> DependencyMap;
  
  // Track critical paths through the function
  std::vector<std::vector<Instruction*>> CriticalPaths;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<AAResultsWrapperPass>();
    AU.addRequired<DependenceAnalysisWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.setPreservesAll();
  }

  bool runOnFunction(Function &F) override;

private:
  void analyzeDataDependencies(Function &F);
  void analyzeControlDependencies(Function &F);
  void analyzeMemoryDependencies(Function &F);
  void identifyCriticalPaths(Function &F);
  bool isMemoryDependent(Instruction *I1, Instruction *I2, AliasAnalysis &AA);
  void printDependencyInfo(raw_ostream &OS, Function &F);
};

bool EnhancedDependencyAnalysis::runOnFunction(Function &F) {
  // Clear previous analysis results
  DependencyMap.clear();
  CriticalPaths.clear();

  // Initialize dependency map entries for all instructions
  for (auto &BB : F) {
    for (auto &I : BB) {
      DependencyMap[&I] = DependencyInfo();
    }
  }

  // Perform dependency analysis
  analyzeDataDependencies(F);
  analyzeControlDependencies(F);
  analyzeMemoryDependencies(F);
  identifyCriticalPaths(F);

  // Debug output
  DEBUG_WITH_TYPE("dependency", {
    dbgs() << "Enhanced Dependency Analysis for function: " << F.getName() << "\n";
    printDependencyInfo(dbgs(), F);
  });

  // This is an analysis pass, so it doesn't modify the function
  return false;
}

void EnhancedDependencyAnalysis::analyzeDataDependencies(Function &F) {
  // For each instruction, find all instructions that use its result
  for (auto &BB : F) {
    for (auto &I : BB) {
      for (User *U : I.users()) {
        if (Instruction *UserInst = dyn_cast<Instruction>(U)) {
          // UserInst depends on I
          DependencyMap[UserInst].DataDependencies.insert(&I);
        }
      }
    }
  }
}

void EnhancedDependencyAnalysis::analyzeControlDependencies(Function &F) {
  // Simple implementation: instructions in a basic block are control dependent on
  // the terminator of all dominating blocks that have multiple successors
  
  // This is a simplified version - a full implementation would use post-dominance
  // frontier analysis
  
  for (auto &BB : F) {
    Instruction *Terminator = BB.getTerminator();
    if (Terminator->getNumSuccessors() > 1) {
      // Terminator has multiple successors, add control dependencies
      for (unsigned i = 0; i < Terminator->getNumSuccessors(); ++i) {
        BasicBlock *Succ = Terminator->getSuccessor(i);
        for (auto &I : *Succ) {
          DependencyMap[&I].ControlDependencies.insert(Terminator);
        }
      }
    }
  }
}

void EnhancedDependencyAnalysis::analyzeMemoryDependencies(Function &F) {
  AliasAnalysis &AA = getAnalysis<AAResultsWrapperPass>().getAAResults();
  
  // For all pairs of memory operations, check if they might alias
  for (auto &BB : F) {
    for (auto &I1 : BB) {
      if (isa<LoadInst>(I1) || isa<StoreInst>(I1)) {
        for (auto &I2 : BB) {
          if (&I1 != &I2 && (isa<LoadInst>(I2) || isa<StoreInst>(I2))) {
            if (isMemoryDependent(&I1, &I2, AA)) {
              DependencyMap[&I2].MemoryDependencies.insert(&I1);
            }
          }
        }
      }
    }
  }
}

bool EnhancedDependencyAnalysis::isMemoryDependent(Instruction *I1, Instruction *I2, AliasAnalysis &AA) {
  // Check if I2 is dependent on I1 due to memory operations
  
  // We need at least one store for a memory dependency to exist
  if (!isa<StoreInst>(I1) && !isa<StoreInst>(I2))
    return false;
    
  // Get memory locations accessed
  MemoryLocation Loc1 = MemoryLocation::get(I1);
  MemoryLocation Loc2 = MemoryLocation::get(I2);
  
  // Check aliasing
  AliasResult AR = AA.alias(Loc1, Loc2);
  
  // If there's no alias, there's no dependency
  if (AR == AliasResult::NoAlias)
    return false;
    
  // If I1 is before I2 in program order and they may alias, there's a dependency
  // This is a simplification - a real implementation would consider different types
  // of dependencies (RAW, WAR, WAW)
  return true;
}

void EnhancedDependencyAnalysis::identifyCriticalPaths(Function &F) {
  // Compute critical path length for each instruction
  for (auto &BB : F) {
    for (auto &I : BB) {
      unsigned MaxDependencyLength = 0;
      
      // Check all dependencies
      for (Instruction *Dep : DependencyMap[&I].DataDependencies) {
        MaxDependencyLength = std::max(MaxDependencyLength, 
                                       DependencyMap[Dep].CriticalPathLength);
      }
      
      for (Instruction *Dep : DependencyMap[&I].ControlDependencies) {
        MaxDependencyLength = std::max(MaxDependencyLength, 
                                       DependencyMap[Dep].CriticalPathLength);
      }
      
      for (Instruction *Dep : DependencyMap[&I].MemoryDependencies) {
        MaxDependencyLength = std::max(MaxDependencyLength, 
                                       DependencyMap[Dep].CriticalPathLength);
      }
      
      // Update critical path length
      DependencyMap[&I].CriticalPathLength = MaxDependencyLength + 1;
    }
  }
  
  // TODO: Extract actual critical paths
}

void EnhancedDependencyAnalysis::printDependencyInfo(raw_ostream &OS, Function &F) {
  for (auto &BB : F) {
    OS << "Basic Block: " << BB.getName() << "\n";
    for (auto &I : BB) {
      OS << "  " << I << "\n";
      
      DependencyInfo &DI = DependencyMap[&I];
      OS << "    Critical Path Length: " << DI.CriticalPathLength << "\n";
      
      OS << "    Data Dependencies: ";
      for (Instruction *Dep : DI.DataDependencies) {
        OS << *Dep << ", ";
      }
      OS << "\n";
      
      OS << "    Control Dependencies: ";
      for (Instruction *Dep : DI.ControlDependencies) {
        OS << *Dep << ", ";
      }
      OS << "\n";
      
      OS << "    Memory Dependencies: ";
      for (Instruction *Dep : DI.MemoryDependencies) {
        OS << *Dep << ", ";
      }
      OS << "\n";
    }
  }
}

char EnhancedDependencyAnalysis::ID = 0;
static RegisterPass<EnhancedDependencyAnalysis> 
    X("enhanced-dep-analysis", "Enhanced Dependency Analysis Pass");

}