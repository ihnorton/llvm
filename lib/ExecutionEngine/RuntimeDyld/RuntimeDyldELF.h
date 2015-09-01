//===-- RuntimeDyldELF.h - Run-time dynamic linker for MC-JIT ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// ELF support for MC-JIT runtime dynamic linker.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_EXECUTIONENGINE_RUNTIMEDYLD_RUNTIMEDYLDELF_H
#define LLVM_LIB_EXECUTIONENGINE_RUNTIMEDYLD_RUNTIMEDYLDELF_H

#include "RuntimeDyldImpl.h"
#include "llvm/ADT/DenseMap.h"

using namespace llvm;

namespace llvm {

class RuntimeDyldELF : public RuntimeDyldImpl {
public:
    // Wraps a RuntimeDyld::TLSSymbolInfo with accessors to easily access the ELF specific interpretation
  class TLSSymbolInfoELF {
  public:
    struct ModuleInfo {
        uint64_t ModuleID;
        int64_t tpoff;
        ModuleInfo(uint64_t ModuleID, int64_t tpoff) :
          ModuleID(ModuleID), tpoff(tpoff) {}
    };

    TLSSymbolInfoELF(ModuleInfo Mod, uint64_t Offset, JITSymbolFlags Flags = JITSymbolFlags::None) :
      Data(Mod.ModuleID,Offset,Mod.tpoff,Flags) {};
    TLSSymbolInfoELF(RuntimeDyld::TLSSymbolInfo Data) : Data(Data) {};

    ModuleInfo getModInfo() { return ModuleInfo(Data.getFirst(), Data.getThird()); }
    uint64_t getOffset() { return Data.getSecond(); }
    RuntimeDyld::TLSSymbolInfo getOpaque() { return Data; }

  private:
    RuntimeDyld::TLSSymbolInfo Data;
  };

  class TLSSymbolResolverELF : public RuntimeDyld::TLSSymbolResolver {
  public:
    virtual int64_t ExtraGOTAddend() { return 0; };
    virtual uint64_t GetAddrOverride() { return 0; };
    TLSSymbolInfoELF findTLSSymbolELF(const std::string &Name) { return TLSSymbolInfoELF(findTLSSymbol(Name)); }
  };

private:

  void resolveRelocation(const SectionEntry &Section, uint64_t Offset,
                         uint64_t Value, uint32_t Type, int64_t Addend,
                         uint64_t SymOffset = 0, SID SectionID = 0);

  void resolveRelocationTLS(const RelocationEntry &RE,
                            TLSSymbolInfoELF Value);

  void resolveX86_64Relocation(const SectionEntry &Section, uint64_t Offset,
                               uint64_t Value, uint32_t Type, int64_t Addend,
                               uint64_t SymOffset);

  void resolveX86_64RelocationTLS(const SectionEntry &Section, uint64_t Offset,
                                  TLSSymbolInfoELF Value, uint32_t Type);

  void resolveX86Relocation(const SectionEntry &Section, uint64_t Offset,
                            uint32_t Value, uint32_t Type, int32_t Addend);

  void resolveAArch64Relocation(const SectionEntry &Section, uint64_t Offset,
                                uint64_t Value, uint32_t Type, int64_t Addend);

  void resolveARMRelocation(const SectionEntry &Section, uint64_t Offset,
                            uint32_t Value, uint32_t Type, int32_t Addend);

  void resolveMIPSRelocation(const SectionEntry &Section, uint64_t Offset,
                             uint32_t Value, uint32_t Type, int32_t Addend);

  void resolvePPC64Relocation(const SectionEntry &Section, uint64_t Offset,
                              uint64_t Value, uint32_t Type, int64_t Addend);

  void resolveSystemZRelocation(const SectionEntry &Section, uint64_t Offset,
                                uint64_t Value, uint32_t Type, int64_t Addend);

  void resolveMIPS64Relocation(const SectionEntry &Section, uint64_t Offset,
                               uint64_t Value, uint32_t Type, int64_t Addend,
                               uint64_t SymOffset, SID SectionID);

  int64_t evaluateMIPS64Relocation(const SectionEntry &Section,
                                   uint64_t Offset, uint64_t Value,
                                   uint32_t Type,  int64_t Addend,
                                   uint64_t SymOffset, SID SectionID);

  void applyMIPS64Relocation(uint8_t *TargetPtr, int64_t CalculatedValue,
                             uint32_t Type);

  // Get the maximum necessary stub space needed for one relocation
  unsigned getMaxStubSize() override {
    if (Arch == Triple::aarch64 || Arch == Triple::aarch64_be)
      return 20; // movz; movk; movk; movk; br
    if (Arch == Triple::arm || Arch == Triple::thumb)
      return 8; // 32-bit instruction and 32-bit address
    else if (IsMipsO32ABI)
      return 16;
    else if (Arch == Triple::ppc64 || Arch == Triple::ppc64le)
      return 44;
    else if (Arch == Triple::x86_64)
      return 16; // but we may need to allocate up to two GOT entries (=16).
    else if (Arch == Triple::systemz)
      return 16;
    else
      return 0;
  }

  // Get the size of just the function itself.
  // This is in general smaller than getMaxStubSize if the stub
  // function also needs a GOT entry.
  unsigned getMaxStubFunctionSize() {
    if (Arch == Triple::x86_64)
      return 6; // 2-byte jmp instruction + 32-bit relative address
    else
      return getMaxStubSize();
  }

  unsigned getStubAlignment() override {
    if (Arch == Triple::systemz)
      return 8;
    else
      return 1;
  }

  void setMipsABI(const ObjectFile &Obj) override;

  void findPPC64TOCSection(const ELFObjectFileBase &Obj,
                           ObjSectionToIDMap &LocalSections,
                           RelocationValueRef &Rel);
  void findOPDEntrySection(const ELFObjectFileBase &Obj,
                           ObjSectionToIDMap &LocalSections,
                           RelocationValueRef &Rel);

  size_t getGOTEntrySize();

  SectionEntry &getSection(unsigned SectionID) { return Sections[SectionID]; }

  // Allocate no GOT entries for use in the given section.
  uint64_t allocateGOTEntries(unsigned SectionID, unsigned no);

  // Resolve the relvative address of GOTOffset in Section ID and place
  // it at the given Offset
  void resolveGOTOffsetRelocation(unsigned SectionID, uint64_t Offset,
                                  uint64_t GOTOffset);

  // For a GOT entry referenced from SectionID, compute a relocation entry
  // that will place the final resolved value in the GOT slot
  RelocationEntry computeGOTOffsetRE(unsigned SectionID,
                                     uint64_t GOTOffset,
                                     uint64_t SymbolOffset,
                                     unsigned Type);

  // Compute the address in memory where we can find the placeholder
  void *computePlaceholderAddress(unsigned SectionID, uint64_t Offset) const;

  // Split out common case for createing the RelocationEntry for when the relocation requires
  // no particular advanced processing.
  void processSimpleRelocation(unsigned SectionID, uint64_t Offset, unsigned RelType, RelocationValueRef Value);

  // The tentative ID for the GOT section
  unsigned GOTSectionID;

  // Records the current number of allocated slots in the GOT
  // (This would be equivalent to GOTEntries.size() were it not for relocations
  // that consume more than one slot)
  unsigned CurrentGOTIndex;

  // A map from section to a GOT section that has entries for section's GOT
  // relocations. (Mips64 specific)
  DenseMap<SID, SID> SectionToGOTMap;

  // A map to avoid duplicate got entries (Mips64 specific)
  StringMap<uint64_t> GOTSymbolOffsets;

  // When a module is loaded we save the SectionID of the EH frame section
  // in a table until we receive a request to register all unregistered
  // EH frame sections with the memory manager.
  SmallVector<SID, 2> UnregisteredEHFrameSections;
  SmallVector<SID, 2> RegisteredEHFrameSections;

public:
  RuntimeDyldELF(RuntimeDyld::MemoryManager &MemMgr,
                 RuntimeDyld::SymbolResolver &Resolver,
                 RuntimeDyld::TLSSymbolResolver *TLSResolver);
  ~RuntimeDyldELF() override;

  std::unique_ptr<RuntimeDyld::LoadedObjectInfo>
  loadObject(const object::ObjectFile &O) override;

  void resolveExternalTLSSymbols() override;

  void resolveRelocation(const RelocationEntry &RE, uint64_t Value) override;
  relocation_iterator
  processRelocationRef(unsigned SectionID, relocation_iterator RelI,
                       const ObjectFile &Obj,
                       ObjSectionToIDMap &ObjSectionToID,
                       StubMap &Stubs) override;
  bool isCompatibleFile(const object::ObjectFile &Obj) const override;
  void registerEHFrames() override;
  void deregisterEHFrames() override;
  void finalizeLoad(const ObjectFile &Obj,
                    ObjSectionToIDMap &SectionMap) override;
};

// Useful out-of-the box TLS symbol resolver implementations for
// libc (those using the glibc ABI) and ELF-on-Darwin

// TLS as implemented by glibc
class TLSSymbolResolverGLibCELF : public RuntimeDyldELF::TLSSymbolResolverELF {
private:
    RuntimeDyld::SymbolResolver *SR;
public:
    TLSSymbolResolverGLibCELF(RuntimeDyld::SymbolResolver *SR) : TLSSymbolResolverELF(), SR(SR) {};
    RuntimeDyld::TLSSymbolInfo findTLSSymbol(const std::string &Name) override;
};

// Support ELF-on-Darwin by matching ELF's TLS model onto that used by OS X
class TLSSymbolResolverDarwinELF : public RuntimeDyldELF::TLSSymbolResolverELF {
private:
    RuntimeDyld::SymbolResolver *SR;
    uint64_t tlv_get_addr_addr;
public:
    TLSSymbolResolverDarwinELF(RuntimeDyld::SymbolResolver *SR) : TLSSymbolResolverELF(), SR(SR),
        tlv_get_addr_addr(0) {};

    int64_t ExtraGOTAddend() override { return -8; };
    uint64_t GetAddrOverride() override { return tlv_get_addr_addr; };
    RuntimeDyld::TLSSymbolInfo findTLSSymbol(const std::string &Name) override;
};

} // end namespace llvm

#endif
