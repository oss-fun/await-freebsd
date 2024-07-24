//===- X86LegalizerInfo.cpp --------------------------------------*- C++ -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file implements the targeting of the Machinelegalizer class for X86.
/// \todo This should be generated by TableGen.
//===----------------------------------------------------------------------===//

#include "X86LegalizerInfo.h"
#include "X86Subtarget.h"
#include "X86TargetMachine.h"
#include "llvm/CodeGen/GlobalISel/LegalizerHelper.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"

using namespace llvm;
using namespace TargetOpcode;
using namespace LegalizeActions;
using namespace LegalityPredicates;

X86LegalizerInfo::X86LegalizerInfo(const X86Subtarget &STI,
                                   const X86TargetMachine &TM)
    : Subtarget(STI) {

  bool Is64Bit = Subtarget.is64Bit();
  bool HasCMOV = Subtarget.canUseCMOV();
  bool HasSSE1 = Subtarget.hasSSE1();
  bool HasSSE2 = Subtarget.hasSSE2();
  bool HasSSE41 = Subtarget.hasSSE41();
  bool HasAVX = Subtarget.hasAVX();
  bool HasAVX2 = Subtarget.hasAVX2();
  bool HasAVX512 = Subtarget.hasAVX512();
  bool HasVLX = Subtarget.hasVLX();
  bool HasDQI = Subtarget.hasAVX512() && Subtarget.hasDQI();
  bool HasBWI = Subtarget.hasAVX512() && Subtarget.hasBWI();

  const LLT p0 = LLT::pointer(0, TM.getPointerSizeInBits(0));
  const LLT s1 = LLT::scalar(1);
  const LLT s8 = LLT::scalar(8);
  const LLT s16 = LLT::scalar(16);
  const LLT s32 = LLT::scalar(32);
  const LLT s64 = LLT::scalar(64);
  const LLT s80 = LLT::scalar(80);
  const LLT s128 = LLT::scalar(128);
  const LLT sMaxScalar = Subtarget.is64Bit() ? s64 : s32;
  const LLT v2s32 = LLT::fixed_vector(2, 32);
  const LLT v4s8 = LLT::fixed_vector(4, 8);


  const LLT v16s8 = LLT::fixed_vector(16, 8);
  const LLT v8s16 = LLT::fixed_vector(8, 16);
  const LLT v4s32 = LLT::fixed_vector(4, 32);
  const LLT v2s64 = LLT::fixed_vector(2, 64);
  const LLT v2p0 = LLT::fixed_vector(2, p0);

  const LLT v32s8 = LLT::fixed_vector(32, 8);
  const LLT v16s16 = LLT::fixed_vector(16, 16);
  const LLT v8s32 = LLT::fixed_vector(8, 32);
  const LLT v4s64 = LLT::fixed_vector(4, 64);
  const LLT v4p0 = LLT::fixed_vector(4, p0);

  const LLT v64s8 = LLT::fixed_vector(64, 8);
  const LLT v32s16 = LLT::fixed_vector(32, 16);
  const LLT v16s32 = LLT::fixed_vector(16, 32);
  const LLT v8s64 = LLT::fixed_vector(8, 64);

  // todo: AVX512 bool vector predicate types

  // implicit/constants
  getActionDefinitionsBuilder(G_IMPLICIT_DEF)
      .legalIf([=](const LegalityQuery &Query) -> bool {
        // 32/64-bits needs support for s64/s128 to handle cases:
        // s64 = EXTEND (G_IMPLICIT_DEF s32) -> s64 = G_IMPLICIT_DEF
        // s128 = EXTEND (G_IMPLICIT_DEF s32/s64) -> s128 = G_IMPLICIT_DEF
        return typeInSet(0, {p0, s1, s8, s16, s32, s64})(Query) ||
               (Is64Bit && typeInSet(0, {s128})(Query));
      });

  getActionDefinitionsBuilder(G_CONSTANT)
      .legalIf([=](const LegalityQuery &Query) -> bool {
        return typeInSet(0, {p0, s8, s16, s32})(Query) ||
               (Is64Bit && typeInSet(0, {s64})(Query));
      })
      .widenScalarToNextPow2(0, /*Min=*/8)
      .clampScalar(0, s8, sMaxScalar);

  // merge/unmerge
  for (unsigned Op : {G_MERGE_VALUES, G_UNMERGE_VALUES}) {
    unsigned BigTyIdx = Op == G_MERGE_VALUES ? 0 : 1;
    unsigned LitTyIdx = Op == G_MERGE_VALUES ? 1 : 0;
    getActionDefinitionsBuilder(Op)
        .widenScalarToNextPow2(LitTyIdx, /*Min=*/8)
        .widenScalarToNextPow2(BigTyIdx, /*Min=*/16)
        .minScalar(LitTyIdx, s8)
        .minScalar(BigTyIdx, s32)
        .legalIf([=](const LegalityQuery &Q) {
          switch (Q.Types[BigTyIdx].getSizeInBits()) {
          case 16:
          case 32:
          case 64:
          case 128:
          case 256:
          case 512:
            break;
          default:
            return false;
          }
          switch (Q.Types[LitTyIdx].getSizeInBits()) {
          case 8:
          case 16:
          case 32:
          case 64:
          case 128:
          case 256:
            return true;
          default:
            return false;
          }
        });
  }

  // integer addition/subtraction
  getActionDefinitionsBuilder({G_ADD, G_SUB})
      .legalIf([=](const LegalityQuery &Query) -> bool {
        if (typeInSet(0, {s8, s16, s32})(Query))
          return true;
        if (Is64Bit && typeInSet(0, {s64})(Query))
          return true;
        if (HasSSE2 && typeInSet(0, {v16s8, v8s16, v4s32, v2s64})(Query))
          return true;
        if (HasAVX2 && typeInSet(0, {v32s8, v16s16, v8s32, v4s64})(Query))
          return true;
        if (HasAVX512 && typeInSet(0, {v16s32, v8s64})(Query))
          return true;
        if (HasBWI && typeInSet(0, {v64s8, v32s16})(Query))
          return true;
        return false;
      })
      .clampMinNumElements(0, s8, 16)
      .clampMinNumElements(0, s16, 8)
      .clampMinNumElements(0, s32, 4)
      .clampMinNumElements(0, s64, 2)
      .clampMaxNumElements(0, s8, HasBWI ? 64 : (HasAVX2 ? 32 : 16))
      .clampMaxNumElements(0, s16, HasBWI ? 32 : (HasAVX2 ? 16 : 8))
      .clampMaxNumElements(0, s32, HasAVX512 ? 16 : (HasAVX2 ? 8 : 4))
      .clampMaxNumElements(0, s64, HasAVX512 ? 8 : (HasAVX2 ? 4 : 2))
      .widenScalarToNextPow2(0, /*Min=*/32)
      .clampScalar(0, s8, sMaxScalar)
      .scalarize(0);

  getActionDefinitionsBuilder({G_UADDE, G_UADDO, G_USUBE, G_USUBO})
      .legalIf([=](const LegalityQuery &Query) -> bool {
        return typePairInSet(0, 1, {{s8, s1}, {s16, s1}, {s32, s1}})(Query) ||
               (Is64Bit && typePairInSet(0, 1, {{s64, s1}})(Query));
      })
      .widenScalarToNextPow2(0, /*Min=*/32)
      .clampScalar(0, s8, sMaxScalar)
      .clampScalar(1, s1, s1)
      .scalarize(0);

  // integer multiply
  getActionDefinitionsBuilder(G_MUL)
      .legalIf([=](const LegalityQuery &Query) -> bool {
        if (typeInSet(0, {s8, s16, s32})(Query))
          return true;
        if (Is64Bit && typeInSet(0, {s64})(Query))
          return true;
        if (HasSSE2 && typeInSet(0, {v8s16})(Query))
          return true;
        if (HasSSE41 && typeInSet(0, {v4s32})(Query))
          return true;
        if (HasAVX2 && typeInSet(0, {v16s16, v8s32})(Query))
          return true;
        if (HasAVX512 && typeInSet(0, {v16s32})(Query))
          return true;
        if (HasDQI && typeInSet(0, {v8s64})(Query))
          return true;
        if (HasDQI && HasVLX && typeInSet(0, {v2s64, v4s64})(Query))
          return true;
        if (HasBWI && typeInSet(0, {v32s16})(Query))
          return true;
        return false;
      })
      .clampMinNumElements(0, s16, 8)
      .clampMinNumElements(0, s32, 4)
      .clampMinNumElements(0, s64, HasVLX ? 2 : 8)
      .clampMaxNumElements(0, s16, HasBWI ? 32 : (HasAVX2 ? 16 : 8))
      .clampMaxNumElements(0, s32, HasAVX512 ? 16 : (HasAVX2 ? 8 : 4))
      .clampMaxNumElements(0, s64, 8)
      .widenScalarToNextPow2(0, /*Min=*/32)
      .clampScalar(0, s8, sMaxScalar)
      .scalarize(0);

  getActionDefinitionsBuilder({G_SMULH, G_UMULH})
      .legalIf([=](const LegalityQuery &Query) -> bool {
        return typeInSet(0, {s8, s16, s32})(Query) ||
               (Is64Bit && typeInSet(0, {s64})(Query));
      })
      .widenScalarToNextPow2(0, /*Min=*/32)
      .clampScalar(0, s8, sMaxScalar)
      .scalarize(0);

  // integer divisions
  getActionDefinitionsBuilder({G_SDIV, G_SREM, G_UDIV, G_UREM})
      .legalIf([=](const LegalityQuery &Query) -> bool {
        return typeInSet(0, {s8, s16, s32})(Query) ||
               (Is64Bit && typeInSet(0, {s64})(Query));
      })
      .clampScalar(0, s8, sMaxScalar);

  // integer shifts
  getActionDefinitionsBuilder({G_SHL, G_LSHR, G_ASHR})
      .legalIf([=](const LegalityQuery &Query) -> bool {
        return typePairInSet(0, 1, {{s8, s8}, {s16, s8}, {s32, s8}})(Query) ||
               (Is64Bit && typePairInSet(0, 1, {{s64, s8}})(Query));
      })
      .clampScalar(0, s8, sMaxScalar)
      .clampScalar(1, s8, s8);

  // integer logic
  getActionDefinitionsBuilder({G_AND, G_OR, G_XOR})
      .legalIf([=](const LegalityQuery &Query) -> bool {
        if (typeInSet(0, {s8, s16, s32})(Query))
          return true;
        if (Is64Bit && typeInSet(0, {s64})(Query))
          return true;
        if (HasSSE2 && typeInSet(0, {v16s8, v8s16, v4s32, v2s64})(Query))
          return true;
        if (HasAVX && typeInSet(0, {v32s8, v16s16, v8s32, v4s64})(Query))
          return true;
        if (HasAVX512 && typeInSet(0, {v64s8, v32s16, v16s32, v8s64})(Query))
          return true;
        return false;
      })
      .clampMinNumElements(0, s8, 16)
      .clampMinNumElements(0, s16, 8)
      .clampMinNumElements(0, s32, 4)
      .clampMinNumElements(0, s64, 2)
      .clampMaxNumElements(0, s8, HasAVX512 ? 64 : (HasAVX ? 32 : 16))
      .clampMaxNumElements(0, s16, HasAVX512 ? 32 : (HasAVX ? 16 : 8))
      .clampMaxNumElements(0, s32, HasAVX512 ? 16 : (HasAVX ? 8 : 4))
      .clampMaxNumElements(0, s64, HasAVX512 ? 8 : (HasAVX ? 4 : 2))
      .widenScalarToNextPow2(0, /*Min=*/32)
      .clampScalar(0, s8, sMaxScalar)
      .scalarize(0);

  // integer comparison
  const std::initializer_list<LLT> IntTypes32 = {s8, s16, s32, p0};
  const std::initializer_list<LLT> IntTypes64 = {s8, s16, s32, s64, p0};

  getActionDefinitionsBuilder(G_ICMP)
      .legalForCartesianProduct({s8}, Is64Bit ? IntTypes64 : IntTypes32)
      .clampScalar(0, s8, s8)
      .clampScalar(1, s8, sMaxScalar)
      .scalarSameSizeAs(2, 1);

  // bswap
  getActionDefinitionsBuilder(G_BSWAP)
      .legalIf([=](const LegalityQuery &Query) {
        return Query.Types[0] == s32 ||
               (Subtarget.is64Bit() && Query.Types[0] == s64);
      })
      .widenScalarToNextPow2(0, /*Min=*/32)
      .clampScalar(0, s32, sMaxScalar);

  // popcount
  getActionDefinitionsBuilder(G_CTPOP)
      .legalIf([=](const LegalityQuery &Query) -> bool {
        return Subtarget.hasPOPCNT() &&
               (typePairInSet(0, 1, {{s16, s16}, {s32, s32}})(Query) ||
                (Is64Bit && typePairInSet(0, 1, {{s64, s64}})(Query)));
      })
      .widenScalarToNextPow2(1, /*Min=*/16)
      .clampScalar(1, s16, sMaxScalar)
      .scalarSameSizeAs(0, 1);

  // count leading zeros (LZCNT)
  getActionDefinitionsBuilder(G_CTLZ)
      .legalIf([=](const LegalityQuery &Query) -> bool {
        return Subtarget.hasLZCNT() &&
               (typePairInSet(0, 1, {{s16, s16}, {s32, s32}})(Query) ||
                (Is64Bit && typePairInSet(0, 1, {{s64, s64}})(Query)));
      })
      .widenScalarToNextPow2(1, /*Min=*/16)
      .clampScalar(1, s16, sMaxScalar)
      .scalarSameSizeAs(0, 1);

  // count trailing zeros
  getActionDefinitionsBuilder({G_CTTZ_ZERO_UNDEF, G_CTTZ})
      .legalIf([=](const LegalityQuery &Query) -> bool {
        return (Query.Opcode == G_CTTZ_ZERO_UNDEF || Subtarget.hasBMI()) &&
               (typePairInSet(0, 1, {{s16, s16}, {s32, s32}})(Query) ||
                (Is64Bit && typePairInSet(0, 1, {{s64, s64}})(Query)));
      })
      .widenScalarToNextPow2(1, /*Min=*/16)
      .clampScalar(1, s16, sMaxScalar)
      .scalarSameSizeAs(0, 1);

  // control flow
  getActionDefinitionsBuilder(G_PHI)
      .legalIf([=](const LegalityQuery &Query) -> bool {
        return typeInSet(0, {s8, s16, s32, p0})(Query) ||
               (Is64Bit && typeInSet(0, {s64})(Query)) ||
               (HasSSE1 && typeInSet(0, {v16s8, v8s16, v4s32, v2s64})(Query)) ||
               (HasAVX && typeInSet(0, {v32s8, v16s16, v8s32, v4s64})(Query)) ||
               (HasAVX512 &&
                typeInSet(0, {v64s8, v32s16, v16s32, v8s64})(Query));
      })
      .clampMinNumElements(0, s8, 16)
      .clampMinNumElements(0, s16, 8)
      .clampMinNumElements(0, s32, 4)
      .clampMinNumElements(0, s64, 2)
      .clampMaxNumElements(0, s8, HasAVX512 ? 64 : (HasAVX ? 32 : 16))
      .clampMaxNumElements(0, s16, HasAVX512 ? 32 : (HasAVX ? 16 : 8))
      .clampMaxNumElements(0, s32, HasAVX512 ? 16 : (HasAVX ? 8 : 4))
      .clampMaxNumElements(0, s64, HasAVX512 ? 8 : (HasAVX ? 4 : 2))
      .widenScalarToNextPow2(0, /*Min=*/32)
      .clampScalar(0, s8, sMaxScalar)
      .scalarize(0);

  getActionDefinitionsBuilder(G_BRCOND).legalFor({s1});

  // pointer handling
  const std::initializer_list<LLT> PtrTypes32 = {s1, s8, s16, s32};
  const std::initializer_list<LLT> PtrTypes64 = {s1, s8, s16, s32, s64};

  getActionDefinitionsBuilder(G_PTRTOINT)
      .legalForCartesianProduct(Is64Bit ? PtrTypes64 : PtrTypes32, {p0})
      .maxScalar(0, sMaxScalar)
      .widenScalarToNextPow2(0, /*Min*/ 8);

  getActionDefinitionsBuilder(G_INTTOPTR).legalFor({{p0, sMaxScalar}});

  getActionDefinitionsBuilder(G_PTR_ADD)
      .legalIf([=](const LegalityQuery &Query) -> bool {
        return typePairInSet(0, 1, {{p0, s32}})(Query) ||
               (Is64Bit && typePairInSet(0, 1, {{p0, s64}})(Query));
      })
      .widenScalarToNextPow2(1, /*Min*/ 32)
      .clampScalar(1, s32, sMaxScalar);

  getActionDefinitionsBuilder({G_FRAME_INDEX, G_GLOBAL_VALUE}).legalFor({p0});

  // load/store: add more corner cases
  for (unsigned Op : {G_LOAD, G_STORE}) {
    auto &Action = getActionDefinitionsBuilder(Op);
    Action.legalForTypesWithMemDesc({{s8, p0, s1, 1},
                                     {s8, p0, s8, 1},
                                     {s16, p0, s8, 1},
                                     {s16, p0, s16, 1},
                                     {s32, p0, s8, 1},
                                     {s32, p0, s16, 1},
                                     {s32, p0, s32, 1},
                                     {s80, p0, s80, 1},
                                     {p0, p0, p0, 1},
                                     {v4s8, p0, v4s8, 1}});
    if (Is64Bit)
      Action.legalForTypesWithMemDesc({{s64, p0, s8, 1},
                                       {s64, p0, s16, 1},
                                       {s64, p0, s32, 1},
                                       {s64, p0, s64, 1},
                                       {v2s32, p0, v2s32, 1}});
    if (HasSSE1)
      Action.legalForTypesWithMemDesc({{v16s8, p0, v16s8, 1},
                                       {v8s16, p0, v8s16, 1},
                                       {v4s32, p0, v4s32, 1},
                                       {v2s64, p0, v2s64, 1},
                                       {v2p0, p0, v2p0, 1}});
    if (HasAVX)
      Action.legalForTypesWithMemDesc({{v32s8, p0, v32s8, 1},
                                       {v16s16, p0, v16s16, 1},
                                       {v8s32, p0, v8s32, 1},
                                       {v4s64, p0, v4s64, 1},
                                       {v4p0, p0, v4p0, 1}});
    if (HasAVX512)
      Action.legalForTypesWithMemDesc({{v64s8, p0, v64s8, 1},
                                       {v32s16, p0, v32s16, 1},
                                       {v16s32, p0, v16s32, 1},
                                       {v8s64, p0, v8s64, 1}});
    Action.widenScalarToNextPow2(0, /*Min=*/8).clampScalar(0, s8, sMaxScalar);
  }

  for (unsigned Op : {G_SEXTLOAD, G_ZEXTLOAD}) {
    auto &Action = getActionDefinitionsBuilder(Op);
    Action.legalForTypesWithMemDesc({{s16, p0, s8, 1},
                                     {s32, p0, s8, 1},
                                     {s32, p0, s16, 1}});
    if (Is64Bit)
      Action.legalForTypesWithMemDesc({{s64, p0, s8, 1},
                                       {s64, p0, s16, 1},
                                       {s64, p0, s32, 1}});
    // TODO - SSE41/AVX2/AVX512F/AVX512BW vector extensions
  }

  // sext, zext, and anyext
  getActionDefinitionsBuilder({G_SEXT, G_ZEXT, G_ANYEXT})
      .legalIf([=](const LegalityQuery &Query) {
        return typeInSet(0, {s8, s16, s32})(Query) ||
          (Query.Opcode == G_ANYEXT && Query.Types[0] == s128) ||
          (Is64Bit && Query.Types[0] == s64);
      })
    .widenScalarToNextPow2(0, /*Min=*/8)
    .clampScalar(0, s8, sMaxScalar)
    .widenScalarToNextPow2(1, /*Min=*/8)
    .clampScalar(1, s8, sMaxScalar);

  getActionDefinitionsBuilder(G_SEXT_INREG).lower();

  // fp constants
  getActionDefinitionsBuilder(G_FCONSTANT)
      .legalIf([=](const LegalityQuery &Query) -> bool {
        return (HasSSE1 && typeInSet(0, {s32})(Query)) ||
               (HasSSE2 && typeInSet(0, {s64})(Query));
      });

  // fp arithmetic
  getActionDefinitionsBuilder({G_FADD, G_FSUB, G_FMUL, G_FDIV})
      .legalIf([=](const LegalityQuery &Query) {
        return (HasSSE1 && typeInSet(0, {s32, v4s32})(Query)) ||
               (HasSSE2 && typeInSet(0, {s64, v2s64})(Query)) ||
               (HasAVX && typeInSet(0, {v8s32, v4s64})(Query)) ||
               (HasAVX512 && typeInSet(0, {v16s32, v8s64})(Query));
      });

  // fp comparison
  getActionDefinitionsBuilder(G_FCMP)
      .legalIf([=](const LegalityQuery &Query) {
        return (HasSSE1 && typePairInSet(0, 1, {{s8, s32}})(Query)) ||
               (HasSSE2 && typePairInSet(0, 1, {{s8, s64}})(Query));
      })
      .clampScalar(0, s8, s8)
      .clampScalar(1, s32, HasSSE2 ? s64 : s32)
      .widenScalarToNextPow2(1);

  // fp conversions
  getActionDefinitionsBuilder(G_FPEXT).legalIf([=](const LegalityQuery &Query) {
    return (HasSSE2 && typePairInSet(0, 1, {{s64, s32}})(Query)) ||
           (HasAVX && typePairInSet(0, 1, {{v4s64, v4s32}})(Query)) ||
           (HasAVX512 && typePairInSet(0, 1, {{v8s64, v8s32}})(Query));
  });

  getActionDefinitionsBuilder(G_FPTRUNC).legalIf(
      [=](const LegalityQuery &Query) {
        return (HasSSE2 && typePairInSet(0, 1, {{s32, s64}})(Query)) ||
               (HasAVX && typePairInSet(0, 1, {{v4s32, v4s64}})(Query)) ||
               (HasAVX512 && typePairInSet(0, 1, {{v8s32, v8s64}})(Query));
      });

  getActionDefinitionsBuilder(G_SITOFP)
      .legalIf([=](const LegalityQuery &Query) {
        return (HasSSE1 &&
                (typePairInSet(0, 1, {{s32, s32}})(Query) ||
                 (Is64Bit && typePairInSet(0, 1, {{s32, s64}})(Query)))) ||
               (HasSSE2 &&
                (typePairInSet(0, 1, {{s64, s32}})(Query) ||
                 (Is64Bit && typePairInSet(0, 1, {{s64, s64}})(Query))));
      })
      .clampScalar(1, s32, sMaxScalar)
      .widenScalarToNextPow2(1)
      .clampScalar(0, s32, HasSSE2 ? s64 : s32)
      .widenScalarToNextPow2(0);

  getActionDefinitionsBuilder(G_FPTOSI)
      .legalIf([=](const LegalityQuery &Query) {
        return (HasSSE1 &&
                (typePairInSet(0, 1, {{s32, s32}})(Query) ||
                 (Is64Bit && typePairInSet(0, 1, {{s64, s32}})(Query)))) ||
               (HasSSE2 &&
                (typePairInSet(0, 1, {{s32, s64}})(Query) ||
                 (Is64Bit && typePairInSet(0, 1, {{s64, s64}})(Query))));
      })
      .clampScalar(1, s32, HasSSE2 ? s64 : s32)
      .widenScalarToNextPow2(0)
      .clampScalar(0, s32, sMaxScalar)
      .widenScalarToNextPow2(1);

  // vector ops
  getActionDefinitionsBuilder({G_EXTRACT, G_INSERT})
      .legalIf([=](const LegalityQuery &Query) {
        unsigned SubIdx = Query.Opcode == G_EXTRACT ? 0 : 1;
        unsigned FullIdx = Query.Opcode == G_EXTRACT ? 1 : 0;
        return (HasAVX && typePairInSet(SubIdx, FullIdx,
                                        {{v16s8, v32s8},
                                         {v8s16, v16s16},
                                         {v4s32, v8s32},
                                         {v2s64, v4s64}})(Query)) ||
               (HasAVX512 && typePairInSet(SubIdx, FullIdx,
                                           {{v16s8, v64s8},
                                            {v32s8, v64s8},
                                            {v8s16, v32s16},
                                            {v16s16, v32s16},
                                            {v4s32, v16s32},
                                            {v8s32, v16s32},
                                            {v2s64, v8s64},
                                            {v4s64, v8s64}})(Query));
      });

  // todo: only permit dst types up to max legal vector register size?
  getActionDefinitionsBuilder(G_CONCAT_VECTORS)
      .legalIf([=](const LegalityQuery &Query) {
        return (HasSSE1 && typePairInSet(1, 0,
                                         {{v16s8, v32s8},
                                          {v8s16, v16s16},
                                          {v4s32, v8s32},
                                          {v2s64, v4s64}})(Query)) ||
               (HasAVX && typePairInSet(1, 0,
                                        {{v16s8, v64s8},
                                         {v32s8, v64s8},
                                         {v8s16, v32s16},
                                         {v16s16, v32s16},
                                         {v4s32, v16s32},
                                         {v8s32, v16s32},
                                         {v2s64, v8s64},
                                         {v4s64, v8s64}})(Query));
      });

  // todo: vectors and address spaces
  getActionDefinitionsBuilder(G_SELECT)
      .legalFor({{s8, s32}, {s16, s32}, {s32, s32}, {s64, s32}, {p0, s32}})
      .widenScalarToNextPow2(0, /*Min=*/8)
      .clampScalar(0, HasCMOV ? s16 : s8, sMaxScalar)
      .clampScalar(1, s32, s32);

  // memory intrinsics
  getActionDefinitionsBuilder({G_MEMCPY, G_MEMMOVE, G_MEMSET}).libcall();

  getActionDefinitionsBuilder({G_DYN_STACKALLOC,
                               G_STACKSAVE,
                               G_STACKRESTORE}).lower();

  // fp intrinsics
  getActionDefinitionsBuilder(G_INTRINSIC_ROUNDEVEN)
      .scalarize(0)
      .minScalar(0, LLT::scalar(32))
      .libcall();

  getActionDefinitionsBuilder({G_FREEZE, G_CONSTANT_FOLD_BARRIER})
    .legalFor({s8, s16, s32, s64, p0})
    .widenScalarToNextPow2(0, /*Min=*/8)
    .clampScalar(0, s8, sMaxScalar);

  getLegacyLegalizerInfo().computeTables();
  verify(*STI.getInstrInfo());
}

bool X86LegalizerInfo::legalizeIntrinsic(LegalizerHelper &Helper,
                                         MachineInstr &MI) const {
  return true;
}
