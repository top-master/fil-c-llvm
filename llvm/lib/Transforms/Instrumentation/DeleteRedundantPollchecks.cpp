//===- DeleteRedundantPollchecks.cpp - delete redundant pollchecks --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file is a part of Fil-C. https://fil-c.org/
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Instrumentation/DeleteRedundantPollchecks.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

static constexpr unsigned NumUnwindRegisters = 2;

static constexpr uint8_t ThreadStateCheckRequested = 2;
static constexpr uint8_t ThreadStateStopRequested = 4;
static constexpr uint8_t ThreadStateDeferredSignal = 8;

struct DeleteRedundantPollchecks {
  DeleteRedundantPollchecks(Module &M) : M(M) {}

  Module &M;
  LLVMContext &C = M.getContext();

  static constexpr unsigned TargetAS = 0;

  const DataLayout DL = M.getDataLayoutAfterFilC();

  unsigned PtrBits = DL.getPointerSizeInBits(TargetAS);
  Type *VoidTy = Type::getVoidTy(C);
  IntegerType *Int8Ty = Type::getInt8Ty(C);
  IntegerType *Int32Ty = Type::getInt32Ty(C);
  IntegerType *IntPtrTy = Type::getIntNTy(C, PtrBits);
  PointerType *RawPtrTy = PointerType::get(C, TargetAS);
  StructType *FlightPtrTy =
      StructType::create({RawPtrTy, RawPtrTy}, "filc_flight_ptr");
  FunctionCallee Pollcheck =
      M.getOrInsertFunction("filc_pollcheck", VoidTy, RawPtrTy, RawPtrTy);
  FunctionCallee PollcheckSlow =
      M.getOrInsertFunction("filc_pollcheck_slow", VoidTy, RawPtrTy, RawPtrTy);

  StructType *ThreadTy = StructType::create(
      {
          IntPtrTy, // stack_limit, index 0
          Int8Ty,   // state, index 1
          Int32Ty,  // tid, index 2
          RawPtrTy, // top_frame, index 3
          RawPtrTy, // alignment word, index 4
          ArrayType::get(FlightPtrTy,
                         NumUnwindRegisters), // unwind_registers, index 5
          FlightPtrTy,                        // cookie_ptr, index 6
          Int8Ty,                             // alignment point, index 7
      },
      "filc_thread_ish_base2");

  Value *threadFieldPtr(Value *Thread, unsigned FieldIndex, const char *Name,
                        Instruction *InsertBefore) {
    GetElementPtrInst *GEP = GetElementPtrInst::Create(
        ThreadTy, Thread,
        {ConstantInt::get(IntPtrTy, 0), ConstantInt::get(Int32Ty, FieldIndex)},
        Name, InsertBefore);
    GEP->setDebugLoc(InsertBefore->getDebugLoc());
    return GEP;
  }

  Value *threadStatePtr(Value *Thread, Instruction *InsertBefore) {
    return threadFieldPtr(Thread, 1, "filc_thread_state_ptr", InsertBefore);
  }

  void run() {
    MDBuilder MDB(M.getContext());

    for (Function &F : M) {
      if (F.isDeclaration())
        continue;

      DominatorTree DT(F);
      LoopInfo LI(DT);

      std::vector<CallInst *> PollchecksToDelete;
      std::vector<CallInst *> PollchecksToKeep;
      DenseMap<Loop *, std::vector<BasicBlock *>> BlocksWithPollcheck;

      ReversePostOrderTraversal<Function *> RPOT(&F);
      for (BasicBlock *BB : RPOT) {
        bool SeenPollcheck = false;
        Loop *L = LI.getLoopFor(BB);
        auto &BlockList = BlocksWithPollcheck[L];
        for (auto &I : *BB) {
          if (auto *CI = dyn_cast<CallInst>(&I)) {
            if (CI->getCalledOperand() == Pollcheck.getCallee()) {
              if (SeenPollcheck)
                PollchecksToDelete.push_back(CI);
              else {
                SeenPollcheck = true;
                if (llvm::any_of(BlockList, [&](BasicBlock *OtherBB) {
                      return DT.dominates(OtherBB, BB);
                    }))
                  PollchecksToDelete.push_back(CI);
                else
                  PollchecksToKeep.push_back(CI);
                BlockList.push_back(BB);
              }
            }
          }
        }
      }

      for (Instruction *I : PollchecksToDelete)
        I->eraseFromParent();
      for (CallInst *I : PollchecksToKeep) {
        Value *MyThread = I->getArgOperand(0);
        DebugLoc DL = I->getDebugLoc();
        Value *StatePtr = threadStatePtr(MyThread, I);
        LoadInst *StateLoad =
            new LoadInst(Int8Ty, StatePtr, "filc_thread_state_load", I);
        StateLoad->setDebugLoc(DL);
        BinaryOperator *Masked = BinaryOperator::Create(
            Instruction::And, StateLoad,
            ConstantInt::get(Int8Ty, ThreadStateCheckRequested |
                                         ThreadStateStopRequested |
                                         ThreadStateDeferredSignal),
            "filc_thread_state_masked", I);
        Masked->setDebugLoc(DL);
        ICmpInst *PollcheckNotNeeded = new ICmpInst(
            I, ICmpInst::ICMP_EQ, Masked, ConstantInt::get(Int8Ty, 0),
            "filc_pollcheck_not_needed");
        PollcheckNotNeeded->setDebugLoc(DL);
        Instruction *NewTerm = SplitBlockAndInsertIfElse(
            PollcheckNotNeeded, I, false, MDB.createBranchWeights(2000, 1),
            nullptr, &LI);
        I->removeFromParent();
        I->insertBefore(NewTerm);
        I->setCalledOperand(PollcheckSlow.getCallee());
      }
    }
  }
};

PreservedAnalyses DeleteRedundantPollchecksPass::run(Module &M,
                                                     ModuleAnalysisManager &) {
  DeleteRedundantPollchecks P(M);
  P.run();
  return PreservedAnalyses::none();
}
