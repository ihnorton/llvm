//===-- TLSSymbolResolverELF.cpp ----------------------------=---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "RuntimeDyldELF.h"

namespace llvm {

// The purpose of these classes is to support the implementation
// specific details of Thread Local Storage, which can differ
// between operating systems platforms and even C libraries.

#ifdef __GLIBC__

// The DTV as specified in the ELF ABI
typedef union dtv {
    size_t counter;
    struct {
        void *val;
        bool _;
    } pointer;
} dtv_t;

RuntimeDyld::TLSSymbolInfo
TLSSymbolResolverGLibCELF::findTLSSymbol(std::string Name) override {
    // It would be lovely to have an API for this. If we
    // wanted to, we might be able to actually look at the
    // internal libc datastructures, but that seems risky if
    // they were to change between versions. This implementation
    // is sketchy because it just searches through all the modules
    // and sees which one is closest, but at least it only relies on
    // the libc ABI, which should be stable.
    const void  *UnallocatedDTVSlot = (void *)-1l;

    // First ask the MemoryManager to dlsym this value for us
    RuntimeDyld::SymbolInfo SI = SR->findSymbol(Name);
    uint64_t Value = SI.getAddress();

    // This is glibc specifc but followed by a number of other C libraries
    dtv_t *dtv = (dtv_t *)(((void **)pthread_self())[1]);

    // The number of allocated entries in the DTV is specified as the value
    // of dtv[-1]
    size_t cnt = dtv[-1].counter;

    // Find the module whose start block for the current thread is closest
    // to the dlsym'd address. Ugly, but works.
    uint64_t distance;
    for (size_t i = 1; i < cnt; ++i) {
      uint64_t distance = (Value - (uint64_t)dtv[i].pointer.val);
      if (dtv[i].pointer.val == UnallocatedDTVSlot)
        continue;
      else if ((uint64_t)dtv[i].pointer.val > Value)
        continue;
      else if (distance < min_distance) {
        min_distance = distance;
        found_i = i;
        found_offset = distance;
      }
    }
    assert(found_i != 0 && "Value could not be found in thread local storage");

    return RuntimeDyldELF::TLSSymbolInfoELF(found_i, found_offset, SI.getFlags()).getOpaque();
}

#endif

#ifdef __APPLE__

typedef struct
{
    void*           tlv_get_addr;
    unsigned long   key;
    unsigned long   offset;
} TLVDescriptor;

// Support ELF-on-Darwin by matching ELF's TLS model onto that used by OS X
RuntimeDyld::TLSSymbolInfo
TLSSymbolResolverDarwinELF::findTLSSymbol(const std::string &Name) {
    // Ask the MemoryManager to dlsym this value for us.
    // The result will be a pointer to the TLVDescriptor.
    RuntimeDyld::SymbolInfo SI = SR->findSymbol(Name);
    TLVDescriptor *Descriptor = (TLVDescriptor *)SI.getAddress();
    if (tlv_get_addr_addr == 0) {
        tlv_get_addr_addr = (uint64_t)Descriptor->tlv_get_addr;
    } else {
        assert(tlv_get_addr_addr == (uint64_t)Descriptor->tlv_get_addr &&
            "Multiple values for __tlv_get_addr not supported");
    }
    return RuntimeDyldELF::TLSSymbolInfoELF(Descriptor->key, Descriptor->offset, SI.getFlags()).getOpaque();
};

}

#endif
