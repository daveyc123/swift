//===--- GenFunc.h - Swift IR generation for functions ----------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
//  This file provides the private interface to the function and
//  function-type emission code.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_IRGEN_GENFUNC_H
#define SWIFT_IRGEN_GENFUNC_H

#include "swift/AST/Types.h"

namespace llvm {
  class Function;
  class Value;
}

namespace swift {
namespace irgen {
  class Address;
  class Explosion;
  class ForeignFunctionInfo;
  class IRGenFunction;

  /// Project the capture address from on-stack block storage.
  Address projectBlockStorageCapture(IRGenFunction &IGF,
                                     Address storageAddr,
                                     CanSILBlockStorageType storageTy);

  /// Load the stored isolation of an @isolated(any) function type, which
  /// is assumed to be at a known offset within a closure object.
  void emitExtractFunctionIsolation(IRGenFunction &IGF,
                                    llvm::Value *fnContext,
                                    Explosion &result);
  
  /// Emit the block header into a block storage slot.
  void emitBlockHeader(IRGenFunction &IGF,
                       Address storage,
                       CanSILBlockStorageType blockTy,
                       llvm::Constant *invokeFunction,
                       CanSILFunctionType invokeTy,
                       ForeignFunctionInfo foreignInfo);

  /// Emit a partial application thunk for a function pointer applied to a
  /// partial set of argument values.
  std::optional<StackAddress> emitFunctionPartialApplication(
      IRGenFunction &IGF, SILFunction &SILFn, const FunctionPointer &fnPtr,
      llvm::Value *fnContext, Explosion &args,
      ArrayRef<SILParameterInfo> argTypes, SubstitutionMap subs,
      CanSILFunctionType origType, CanSILFunctionType substType,
      CanSILFunctionType outType, Explosion &out, bool isOutlined);
  CanType getArgumentLoweringType(CanType type, SILParameterInfo paramInfo,
                                  bool isNoEscape);

  /// Stub function that weakly links againt the swift_coroFrameAlloc
  /// function. This is required for back-deployment.
  static llvm::Constant *getCoroFrameAllocStubFn(IRGenModule &IGM) {
  return IGM.getOrCreateHelperFunction(
    "__swift_coroFrameAllocStub", IGM.Int8PtrTy,
    {IGM.SizeTy, IGM.Int64Ty},
    [&](IRGenFunction &IGF) {
      auto parameters = IGF.collectParameters();
      auto *size = parameters.claimNext();
      auto coroAllocPtr = IGF.IGM.getCoroFrameAllocFn();
      auto coroAllocFn = dyn_cast<llvm::Function>(coroAllocPtr);
      coroAllocFn->setLinkage(llvm::GlobalValue::ExternalWeakLinkage);
      auto *coroFrameAllocFn = IGF.IGM.getOpaquePtr(coroAllocPtr);
      auto *nullSwiftCoroFrameAlloc = IGF.Builder.CreateCmp(
        llvm::CmpInst::Predicate::ICMP_NE, coroFrameAllocFn,
        llvm::ConstantPointerNull::get(
            cast<llvm::PointerType>(coroFrameAllocFn->getType())));
      auto *coroFrameAllocReturn = IGF.createBasicBlock("return-coroFrameAlloc");
      auto *mallocReturn = IGF.createBasicBlock("return-malloc");
      IGF.Builder.CreateCondBr(nullSwiftCoroFrameAlloc, coroFrameAllocReturn, mallocReturn);

      IGF.Builder.emitBlock(coroFrameAllocReturn);
      auto *mallocTypeId = parameters.claimNext();
      auto *coroFrameAllocCall = IGF.Builder.CreateCall(IGF.IGM.getCoroFrameAllocFunctionPointer(), {size, mallocTypeId});
      IGF.Builder.CreateRet(coroFrameAllocCall);

      IGF.Builder.emitBlock(mallocReturn);
      auto *mallocCall = IGF.Builder.CreateCall(IGF.IGM.getMallocFunctionPointer(), {size});
      IGF.Builder.CreateRet(mallocCall);
    },
    /*setIsNoInline=*/false,
    /*forPrologue=*/false,
    /*isPerformanceConstraint=*/false,
    /*optionalLinkageOverride=*/nullptr, llvm::CallingConv::C);
  }
} // end namespace irgen
} // end namespace swift

#endif
