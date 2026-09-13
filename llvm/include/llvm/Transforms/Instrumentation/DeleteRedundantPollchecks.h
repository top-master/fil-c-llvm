#ifndef LLVM_TRANSFORMS_INSTRUMENTATION_DELETEREDUNDANTPOLLCHECKS_H
#define LLVM_TRANSFORMS_INSTRUMENTATION_DELETEREDUNDANTPOLLCHECKS_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class Module;

class DeleteRedundantPollchecksPass
    : public PassInfoMixin<DeleteRedundantPollchecksPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM);
  static bool isRequired() { return true; }
};

} // namespace llvm

#endif /* LLVM_TRANSFORMS_INSTRUMENTATION_DELETEREDUNDANTPOLLCHECKS_H */

