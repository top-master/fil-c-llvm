//===- KillUB.cpp - remove all UB flags -----------------------------------===//
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

#include "llvm/Transforms/Instrumentation/KillUB.h"

#include <llvm/IR/AttributeMask.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/Instructions.h>
#include <vector>

using namespace llvm;

PreservedAnalyses KillUBPass::run(Module &M, ModuleAnalysisManager&) {
  // This is just the beginning! There are probably more UB killings that have to happen.
  
  AttributeMask AM;
  AM.addAttribute(Attribute::AllocSize);
  AM.addAttribute(Attribute::NoUndef);
  AM.addAttribute(Attribute::Convergent);
  AM.addAttribute(Attribute::Dereferenceable);
  AM.addAttribute(Attribute::DereferenceableOrNull);
  AM.addAttribute(Attribute::Memory);
  AM.addAttribute(Attribute::NoAlias);
  AM.addAttribute(Attribute::NoCallback);
  AM.addAttribute(Attribute::NoCapture);
  AM.addAttribute(Attribute::Captures);
  AM.addAttribute(Attribute::NoDivergenceSource);
  AM.addAttribute(Attribute::NoExt);
  AM.addAttribute(Attribute::NoFree);
  AM.addAttribute(Attribute::DeadOnUnwind);
  AM.addAttribute(Attribute::NonNull);
  AM.addAttribute(Attribute::NoRecurse);
  AM.addAttribute(Attribute::NoRedZone);
  AM.addAttribute(Attribute::NoReturn);
  AM.addAttribute(Attribute::NoSync);
  AM.addAttribute(Attribute::Range);
  AM.addAttribute(Attribute::ReadNone);
  AM.addAttribute(Attribute::ReadOnly);
  AM.addAttribute(Attribute::Returned);
  AM.addAttribute(Attribute::Returned);
  AM.addAttribute(Attribute::WillReturn);
  AM.addAttribute(Attribute::Writable);
  AM.addAttribute(Attribute::WriteOnly);
  AM.addAttribute(Attribute::MustProgress);

  for (Function& F : M) {
    if (F.isIntrinsic()) {
      assert(F.isDeclaration());
      continue;
    }
    F.removeFnAttrs(AM);
    F.removeRetAttrs(AM);
    for (size_t Idx = F.arg_size(); Idx--;)
      F.removeParamAttrs(Idx, AM);
    std::vector<Instruction*> ToErase;
    for (BasicBlock& BB : F) {
      for (Instruction& I : BB) {
        I.dropUnknownNonDebugMetadata();
        I.dropPoisonGeneratingAnnotations();
        I.dropUBImplyingAttrsAndUnknownMetadata();
        CallBase* CI = dyn_cast<CallBase>(&I);
        if (!CI)
          continue;
        Function* Callee = dyn_cast<Function>(CI->getCalledOperand());
        if (!Callee || !Callee->isIntrinsic())
          continue;
        bool ShouldErase = false;
        switch (Callee->getIntrinsicID()) {
        case Intrinsic::assume:
        case Intrinsic::experimental_noalias_scope_decl:
        case Intrinsic::invariant_start:
        case Intrinsic::invariant_end:
          ShouldErase = true;
          break;
        case Intrinsic::launder_invariant_group:
        case Intrinsic::strip_invariant_group:
          CI->replaceAllUsesWith(CI->getArgOperand(0));
          ShouldErase = true;
          break;
        default:
          break;
        }
        if (ShouldErase) {
          if (InvokeInst* II = dyn_cast<InvokeInst>(CI))
            BranchInst::Create(II->getNormalDest(), CI)->setDebugLoc(CI->getDebugLoc());
          ToErase.push_back(CI);
        }
      }
    }
    for (Instruction* I : ToErase)
      I->eraseFromParent();
  }
  
  return PreservedAnalyses::none();
}

