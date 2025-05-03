
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

// Command line options
static cl::opt<bool> EnableProfiling("paralldiscover-profile",
                                    cl::desc("Enable profiling instrumentation"),
                                    cl::init(false));

static cl::opt<bool> EnableSpeculation("paralldiscover-speculate",
                                      cl::desc("Enable speculative execution transformations"),
                                      cl::init(true));

static cl::opt<unsigned> UnrollFactor("paralldiscover-unroll-factor",
                                     cl::desc("Loop unrolling factor for ILP exposure"),
                                     cl::init(4));

namespace {

// Main pass that runs the entire PARALLDISCOVER framework
struct ParallDiscover : public ModulePass {
  static char ID;
  ParallDiscover() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    bool Modified = false;
    
    legacy::FunctionPassManager FPM(&M);
    
    // Add analysis passes
    FPM.add(new EnhancedDependencyAnalysis());
    
    // Add transformation passes
    FPM.add(new ILPExposureTransform());
    
    if (EnableSpeculation) {
      FPM.add(new SpeculativeExecutionGuard());
    }
    
    if (EnableProfiling) {
      FPM.add(new InstrumentationPass());
    }
    
    // Initialize and run
    FPM.doInitialization();
    
    for (Function &F : M) {
      if (!F.isDeclaration()) {
        bool LocalModified = FPM.run(F);
        Modified |= LocalModified;
        
        if (LocalModified) {
          errs() << "PARALLDISCOVER modified function: " << F.getName() << "\n";
        }
      }
    }
    
    FPM.doFinalization();
    
    return Modified;
  }
};

char ParallDiscover::ID = 0;
static RegisterPass<ParallDiscover>
    PARALLD("paralldiscover", "PARALLDISCOVER: ILP Discovery Framework");

// Register for opt -O3
static void registerParallDiscover(const PassManagerBuilder &,
                                  legacy::PassManagerBase &PM) {
  PM.add(new ParallDiscover());
}
static RegisterStandardPasses
    RegisterParallDiscover(PassManagerBuilder::EP_VectorizerStart,
                          registerParallDiscover);

}