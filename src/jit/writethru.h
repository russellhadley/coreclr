// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

//===============================================================================
#include "phase.h"
#include "smallhash.h"

typedef SmallHashTable<unsigned, unsigned, 32> ProxyVarsMap;

class WriteThru;

struct WriteThruData
{
    ProxyVarsMap* proxyVarsMap;
    bool* changed;
};

template<>
struct HashTableInfo<unsigned int>
{
    static bool Equals(unsigned int x, unsigned int y)
    {
        return x == y;
    }

    static unsigned GetHashCode(unsigned int key)
    {
        return key;
    }    
};

// Class implementing a simple EH write through.
//
// High level algorithm:
//   1) identify local vars that are live in/out of handlers/finallys
//   2) foreach local var live through EH mark the original local var as
//      do not allocate and create a proxy local var that will be enregistered
//      in the disjoint regions.
//   3) walk the method and replace all appearances of the original local var
//      with the proxy assigning the proxy to the original at defs and loading
//      the proxy from the original at handler labels and entries from finallys.
//
// The result of this is consistent stack state of the value of the local var and
// disjoint enregistered lifetimes.

class WriteThru : public Phase
{
private:

    VARSET_TP EHVars = nullptr;
    ProxyVarsMap* proxyVarsMap = nullptr;    

private:

    void CreateEnregisterableProxies();
    
    GenTreePtr FindInsertionPoint(BasicBlock* reloadBlock);

    void InsertParameterProxyDef();

    GenTreePtr InsertProxyReload(BasicBlock* reloadBlock, GenTreePtr insertionPoint, unsigned proxyId, unsigned localId);

    void InsertProxyReloads(BasicBlock* block);

    bool IsEHExit(BasicBlock* block);
    
    bool IsHandlerEntry(BasicBlock* block);

    static GenTreePtr RewriteAssignment(Compiler* comp, GenTreePtr node, GenTreePtr destination, unsigned proxyId);

    static Compiler::fgWalkResult RewriteEHLocalVarApperance(GenTreePtr* pTree, Compiler::fgWalkData* data);

    void WriteThruLocalVars();

public:

    virtual void DoPhase() override;

    WriteThru(Compiler* _comp);
};

inline WriteThru::WriteThru(Compiler* _comp) : Phase(_comp, "EH Write Thru", PHASE_EH_WRITE_THRU)
{
}
