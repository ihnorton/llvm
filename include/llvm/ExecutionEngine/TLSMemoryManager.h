//===-- TLSImplementation.h - TLS Support for RuntimeDyld -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_TLSMEMORYMANAGER_H
#define LLVM_EXECUTIONENGINE_TLSMEMORYMANAGER_H

#include <utility>
#include <stdint.h>
#include "llvm/ADT/StringRef.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"

namespace llvm {

class TLSMemoryManager {};

class TLSMemoryManagerELF : public TLSMemoryManager {
public:
    typedef std::pair<uint64_t,uint64_t> TLSOffset;
    virtual int64_t ExtraGOTAddend() { return 0; };
    virtual uint64_t GetAddrOverride() { return 0; };
    virtual TLSOffset TLSdlsym(StringRef Name) = 0;
};


// Specific implementations provided by LLVM

// TLS as implemented by glibc
class TLSMemoryManagerGLibC : public TLSMemoryManagerELF {
private:
    RTDyldMemoryManager *MM;
public:
    TLSMemoryManagerGLibC(RTDyldMemoryManager *MM) : TLSMemoryManagerELF(), MM(MM) {};
    TLSOffset TLSdlsym(StringRef Name) override;
};

// Support ELF-on-Darwin by matching ELF's TLS model onto that used by OS X
class TLSMemoryManagerDarwin : public TLSMemoryManagerELF {
private:
    RTDyldMemoryManager *MM;
    uint64_t tlv_get_addr_addr;
public:
    TLSMemoryManagerDarwin(RTDyldMemoryManager *MM) : TLSMemoryManagerELF(), MM(MM),
        tlv_get_addr_addr(0) {};

    int64_t ExtraGOTAddend() override { return -8; };
    uint64_t GetAddrOverride() override { return tlv_get_addr_addr; };
    TLSOffset TLSdlsym(StringRef Name) override;
};


/*
class TLSImplementationMachO : public TLSImplementation {

};
*/

}

#endif
