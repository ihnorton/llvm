//===-- TLSImplementation.h - TLS Support for RuntimeDyld -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//


namespace llvm {

class TLSMemoryManager {};

class TLSMemoryManagerELF : public TLSMemoryManager {
public:
    typedef std::pair<uint64_t,uint64_t> TLSOffset;
    virtual TLSOffset TLSdlsym(StringRef Name) = 0;
};

class TLSImplementationMachO : public TLSImplementation {

};

}
