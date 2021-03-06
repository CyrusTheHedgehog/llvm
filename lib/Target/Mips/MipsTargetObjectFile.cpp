//===-- MipsTargetObjectFile.cpp - Mips Object Files ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "MipsTargetObjectFile.h"
#include "MipsSubtarget.h"
#include "MipsTargetMachine.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ELF.h"
#include "llvm/Target/TargetMachine.h"
using namespace llvm;

static cl::opt<unsigned>
SSThreshold("mips-ssection-threshold", cl::Hidden,
            cl::desc("MIPS: Small data and bss section threshold size (default=8)"),
            cl::init(8));

static cl::opt<bool>
LocalSData("mlocal-sdata", cl::Hidden,
           cl::desc("MIPS: Use gp_rel for object-local data."),
           cl::init(true));

static cl::opt<bool>
ExternSData("mextern-sdata", cl::Hidden,
            cl::desc("MIPS: Use gp_rel for data that is not defined by the "
                     "current object."),
            cl::init(true));

void MipsTargetObjectFile::Initialize(MCContext &Ctx, const TargetMachine &TM){
  TargetLoweringObjectFileELF::Initialize(Ctx, TM);
  InitializeELF(TM.Options.UseInitArray);

  SmallDataSection = getContext().getELFSection(
      ".sdata", ELF::SHT_PROGBITS,
      ELF::SHF_WRITE | ELF::SHF_ALLOC | ELF::SHF_MIPS_GPREL);

  SmallBSSSection = getContext().getELFSection(".sbss", ELF::SHT_NOBITS,
                                               ELF::SHF_WRITE | ELF::SHF_ALLOC |
                                                   ELF::SHF_MIPS_GPREL);
  this->TM = &static_cast<const MipsTargetMachine &>(TM);
}

// A address must be loaded from a small section if its size is less than the
// small section size threshold. Data in this section must be addressed using
// gp_rel operator.
static bool IsInSmallSection(uint64_t Size) {
  // gcc has traditionally not treated zero-sized objects as small data, so this
  // is effectively part of the ABI.
  return Size > 0 && Size <= SSThreshold;
}

/// Return true if this global address should be placed into small data/bss
/// section. This method does all the work, directly influencing section
/// kind.
bool MipsTargetObjectFile::
isGlobalInSmallSectionKind(const GlobalObject *GO,
                           const TargetMachine &TM) const {
  const MipsSubtarget &Subtarget =
      *static_cast<const MipsTargetMachine &>(TM).getSubtargetImpl();

  // Return if small section is not available.
  if (!Subtarget.useSmallSection())
    return false;

  // Only global variables, not functions.
  const GlobalVariable *GVA = dyn_cast<GlobalVariable>(GO);
  if (!GVA)
    return false;

  // Enforce -mlocal-sdata.
  if (!LocalSData && GVA->hasLocalLinkage())
    return false;

  // Enforce -mextern-sdata.
  if (!ExternSData && ((GVA->hasExternalLinkage() && GVA->isDeclaration()) ||
                       GVA->hasCommonLinkage()))
    return false;

  Type *Ty = GVA->getValueType();
  return IsInSmallSection(
      GVA->getParent()->getDataLayout().getTypeAllocSize(Ty));
}

MCSection *MipsTargetObjectFile::SelectSectionForGlobal(
    const GlobalObject *GO, SectionKind Kind, const TargetMachine &TM) const {
  // TODO: Could also support "weak" symbols as well with ".gnu.linkonce.s.*"
  // sections?

  // Handle Small Section classification here.
  if (Kind.isSmallBSS())
    return SmallBSSSection;
  if (Kind.isSmallData())
    return SmallDataSection;

  // Otherwise, we work the same as ELF.
  return TargetLoweringObjectFileELF::SelectSectionForGlobal(GO, Kind, TM);
}

/// Return true if this constant should be placed into small data section.
bool MipsTargetObjectFile::isConstantInSmallSectionKind(
    const DataLayout &DL, const Constant *CN) const {
  return (static_cast<const MipsTargetMachine &>(*TM)
              .getSubtargetImpl()
              ->useSmallSection() &&
          LocalSData && IsInSmallSection(DL.getTypeAllocSize(CN->getType())));
}

/// Return true if this constant should be placed into small data section.
MCSection *MipsTargetObjectFile::getSectionForConstant(const DataLayout &DL,
                                                       SectionKind Kind,
                                                       const Constant *C,
                                                       unsigned &Align) const {
  if (Kind.isSmallReadOnly())
    return SmallDataSection;

  // Otherwise, we work the same as ELF.
  return TargetLoweringObjectFileELF::getSectionForConstant(DL, Kind, C, Align);
}
