#include "jitpch.h"
#ifdef _MSC_VER
#pragma hdrstop
#endif

// DoPhase
//
// Notes:
//   Entry point for WriteThru phase.  Implements toplevel algorithm documented in
//   writethru.h. 
//
void WriteThru::DoPhase()
{
    int count = g_jitHost->getIntConfigValue(W("DoEHWriteThru"), -1);
    if (count == -1)
    {
        // default value do nothing;
        return;
    }

    // Build liveness to compute live on handler entry/finally exit sets.
    // This will be incrementally updated and then reused in the SSA phase.
    
    comp->fgLocalVarLiveness();
    comp->EndPhase(PHASE_BUILD_EH_LIVENESS);

    // Using the sets stashed on the compiler instance, write through the
    // local vars that are live across EH.

    if (VarSetOps::IsEmpty(comp, comp->exceptVars)
        && VarSetOps::IsEmpty(comp, comp->finallyVars)
        && VarSetOps::IsEmpty(comp, comp->filterVars))
    {
        // Don't do write through if there are no local vars live across EH flow.

        JITDUMP("WriteThru early out: no local vars live across EH flow.")
        return;
    }
    else
    {
        // We found work to do.  Since we're doing work and we don't 
        // want to duplicate between exceptVars and finallyVars go ahead
        // and create a union vector to work on.

        VarSetOps::Assign(comp, EHVars, comp->exceptVars);
        VarSetOps::UnionD(comp, EHVars, comp->finallyVars);
        VarSetOps::UnionD(comp, EHVars, comp->filterVars);
    }

    // Generate proxy local vars based on liveness info.
    JITDUMP("Creating enregisterable proxies:")
    CreateEnregisterableProxies();

    // Walk the IR making updates to replace EH local var appearances with
    // enregisterable proxies and write thru definitions.
    JITDUMP("Performing write thru of EH local vars:")
    WriteThruLocalVars();
}

// WriteThruLocalVars
//
// Notes: 
//
//   Iterate through the method and rewrite appearances.  This preserves 
//   original liveness of the local var on stack across EH flow but creates 
//   a local proxy that can be register allocated with in the main body as
//   as well as in the handler.
//
void WriteThru::WriteThruLocalVars()
{
    // for each statement iterate through apperances
    // - foreach def, replace with the proxy and writethru to the original local var.
    // - foreach use, replace with the proxy.
    // - for handler entries, reload live local vars to proxies.
    // - for each finally funclet exit (returning from exceptional path) reload live local ars to proxies.
    bool changed = false;
    WriteThruData data = { proxyVarsMap, &changed };

    BasicBlock* currentBlock = nullptr;
    foreach_block(comp, currentBlock)
    {
        // Set the block we're working on.
        comp->compCurBB = currentBlock;

        // Iterate each statement in the block

        for (GenTreeStmt* statement = currentBlock->firstStmt();
             statement; 
             statement = statement->getNextStmt())
        {
            // Find each assignment and if the destination is local var live across EH write it thru.
            // Find each use and substitute the write thru proxy.

            comp->fgWalkTreePost(&(statement->gtStmtExpr), RewriteEHLocalVarApperance, &data);

            if (changed)
            {
                comp->fgMorphBlockStmt(currentBlock, statement DEBUGARG("EHWriteThru"));
                changed = false;
            }
        }

        // Identify handler entry and finally exit blocks. This are locations
        // where stack value of the local var needs to be reestablished in the
        // proxy.  For the handler entry this means reloads at the entry of the
        // block, for finallys this means at the head of the sink block (successor)
        // where normal execution resumes.

        if (IsHandlerEntry(currentBlock))
        {
            // Insert proxy reloads for local vars live on entry to block;

            InsertProxyReloads(currentBlock);
        }

        if (IsEHExit(currentBlock))
        {
            // Foreach successor insert reloads of EH vars.
            AllSuccessorIter succsEnd = currentBlock->GetAllSuccs(comp).end();
            for (AllSuccessorIter succs = currentBlock->GetAllSuccs(comp).begin(); succs != succsEnd; ++succs)
            {
                BasicBlock* reloadBlock = (*succs);
                InsertProxyReloads(reloadBlock);
            }
        }
    }

#if DEBUG
    // Zero out current basic block for cleanliness.
    comp->compCurBB = nullptr;
#endif // DEBUG

    // After walking the method, insert definitions for proxies of incoming parameters
    // that cross EH flow.

    InsertParameterProxyDef();
}

void WriteThru::InsertParameterProxyDef()
{
    BasicBlock* firstBlock = comp->fgFirstBB;
    unsigned localId = 0;
    const LclVarDsc* endParams = comp->lvaTable + comp->info.compArgsCount;
    for (LclVarDsc* localVar = comp->lvaTable; localVar < endParams; localId++, localVar++)
    {
        assert(localVar->lvIsParam);
        unsigned proxyId;
        bool foundProxy = proxyVarsMap->TryGetValue(localId, &proxyId);

        if (foundProxy)
        {
            InsertProxyReload(firstBlock, firstBlock->FirstNonPhiDef(), proxyId, localId);
        }
    }
}

// IsHandlerEntry
// 
// Parameters:
//
//   block        - Block to test as a handler entry.
//
// Notes:
//
//   Utility function to isolate testing for handler entry block.  For 
//   funclet style this is a check of the block flag check.  For x86 this
//   includes <TODO>
//
// Returns:
//
//   True if this is a handler entry
//
bool WriteThru::IsHandlerEntry(BasicBlock* block)
{
#if FEATURE_EH_FUNCLETS
        // if block is a handler entry, insert reloads of live proxies.
        if (block->bbFlags & BBF_FUNCLET_BEG)
        {
            return true;
        }
#else

#endif // FEATURE_EH_FUNCLETS
    return false;
}

// IsFinallyExit
// 
// Parameters:
//
//   block        - Block to test as a finally exit.
//   reloadBlock  - Out paramter that identifies the sink block
//                  where reloads should be inserted
//
// Notes:
//
//   Utility function to isolate testing for finally exit block and
//   returning the reload insertion block.  For funclet style this is
//   a check of the block jump kind.  For x86 this includes <TODO>
//
// Returns:
//
//   True if this is a finally exit.  Out parameter for reload block returned 
//   via passed ptr as well.
//
bool WriteThru::IsEHExit(BasicBlock* block)
{
#if FEATURE_EH_FUNCLETS
        // if block is a finally exit, add sink block to worklist.
        if ((block->bbJumpKind == BBJ_EHFINALLYRET) || (block->bbJumpKind == BBJ_EHFILTERRET) ||
            (block->bbJumpKind == BBJ_EHCATCHRET))
        {
            return true;
        }     
#else

#endif // FEATURE_EH_FUNCLETS

    return false;
}

// FindInsertionPoint
// 
// Parameters:
//
//   reloadBlock - Target block for reload of live EHVars.
//
// Notes:
//
//   Find the first insertion point for reloads.  This needs to skip
//   over expected first statements like the catch arg.
//
GenTreePtr WriteThru::FindInsertionPoint(BasicBlock* exitBlock, BasicBlock** reloadBlock)
{
    assert(comp->fgComputePredsDone);

    // Find the appropraite block;

    if (((exitBlock->bbFlags & BBF_KEEP_BBJ_ALWAYS) == 1) 
        && exitBlock->bbPrev->isBBCallAlwaysPair()) 
    {
        successorBlock = exitBlock->GetUniqueSucc();

        if (successorBlock->GetUniquePred(comp) == nullptr)
        {
            // If successor a join block, split the edge and
            // insert a airlock block.
            BasicBlock* newBlock = comp->fgNewBBinRegion(BBJ_ALWAYS, successorBlock->bbTryIndex, 
                successorBlock->bbHndIndex, exitBlock);
        }
    }

    GenTreePtr insertionPoint = reloadBlock->FirstNonPhiDef();

    if (insertionPoint == nullptr) 
    {
        // Empty block degenerate case - return nullptr as the sentinel.

        return nullptr;
    }

    if (insertionPoint->AsStmt()->gtStmtList->gtOper == GT_CATCH_ARG)
    {
        insertionPoint = insertionPoint->gtNext;
        assert(insertionPoint != nullptr);
    }

    return insertionPoint;
}

// InsertProxyReloads
// 
// Parameters:
//
//   reloadBlock - Target block for reload of live EHVars.
//
// Notes:
//
//   Look at all local vars that are live into this block.  If any are in the EHVars set,
//   produce a new statement implementing a reload from original local var to new proxy at
//   the beginning of the block.
//
void WriteThru::InsertProxyReloads(BasicBlock* exitBlock)
{
    VARSET_TP lookupSet = VarSetOps::Intersection(comp, EHVars, reloadBlock->bbLiveIn);
    // Initialize insert point.  All reloads will be inserted before this point by assending
    // local index number.  Note that reload block is an out param and make be a different
    // block than the exit block.
    BasicBlock* reloadBlock = nullptr;
    GenTreePtr insertPoint = FindInsertionPoint(exitBlock, &reloadBlock);

    // foreach EHVar that is live into this block, do a look up to see if it has a proxy.
    // - if a proxy is found do a reload from the original local var on the stack to the
    //   proxy.

    VARSET_ITER_INIT(comp, iter, lookupSet, localId);
    while (iter.NextElem(comp, &localId))
    {
        unsigned proxyId;
        bool foundProxy = proxyVarsMap->TryGetValue(localId, &proxyId);

        if (foundProxy)
        {
            InsertProxyReload(reloadBlock, insertPoint, proxyId, localId);
        }
    }
}

// InsertProxyReload
// 
// Parameters:
//
//   reloadBlock     - Address of GenTree node ptr to process.
//   insertionPoint  - Custom WriteThru walk data. Primarly this is phase context.
//   proxyId         - Var index of the proxy local var.
//   localId         - Var index of the original local var.
//
// Notes:
//
//   Insert a reload assignment from the origial local var to the new proxy var.
//
// Returns:
//
//   Inserted reload assignment statement.
//
GenTreePtr WriteThru::InsertProxyReload(BasicBlock* reloadBlock, GenTreePtr insertionPoint, unsigned proxyId, unsigned localId)
{
    var_types localType = comp->lvaTable[localId].lvType;
    assert(localType == comp->lvaTable[proxyId].lvType);
    GenTreePtr localVar = comp->gtNewLclvNode(localId, localType);
    GenTreePtr proxyVar = comp->gtNewLclvNode(proxyId, localType);
    GenTreePtr assign = comp->gtNewAssignNode(proxyVar, localVar);
    GenTreePtr assignStmt = comp->gtNewStmt(assign);

    if (insertionPoint != nullptr)
    {
        comp->fgInsertStmtBefore(reloadBlock, insertionPoint, assignStmt);
    }
    else
    {
        // No defined insertion point so insert at the beginning.
        comp->fgInsertStmtAtBeg(reloadBlock, assignStmt->AsStmt());
    }

    comp->fgMorphBlockStmt(reloadBlock, assignStmt->AsStmt() DEBUGARG("EHWriteThru"));

    JITDUMP("Inserting reload:")
    DISPTREE(assignStmt);

    // Insert new assignment as it's own statement
    return assignStmt;
}

// RewriteEHLocalVarApperance
// 
// Parameters:
//
//   pTree         - Address of GenTree node ptr to process.
//   data          - Custom WriteThru walk data. Primarly this is phase context.
//
// Notes:
//
//   Visitor function for tree post order walk.  Visits each node rewriting def/use
//   appearances of EH local vars to a write thru form (see header comments for
//   toplevel algorithm). 
//
// Returns:
//
//   CONTINUE_WALK - To completely process whole tree.
//
Compiler::fgWalkResult WriteThru::RewriteEHLocalVarApperance(GenTreePtr* pTree, Compiler::fgWalkData* data)
{
    GenTreePtr node = *pTree;
    unsigned kind = node->OperKind();
    Compiler* comp = data->compiler;
    ProxyVarsMap* proxyVarsMap = ((WriteThruData*)(data->pCallbackData))->proxyVarsMap;
    bool* changed = ((WriteThruData*)(data->pCallbackData))->changed;

    if (node->OperIsAssignment())
    {
        GenTreePtr destination = node->gtGetOp1();

        // Assignment of local var case.  Rewrite in terms of the proxy and then store the value
        // back to the original local var on the stack.
        if (destination->OperIsScalarLocal() && ((destination->gtFlags & GTF_VAR_DEF) == 1))
        {
            unsigned lclVarNum = destination->AsLclVarCommon()->GetLclNum();
            LclVarDsc* dstLclVarDsc = &comp->lvaTable[lclVarNum];

            if (!comp->lvaIsFieldOfDependentlyPromotedStruct(dstLclVarDsc))
            {
                unsigned proxyId;
                bool found = proxyVarsMap->TryGetValue(lclVarNum, &proxyId);
                
                if (found) 
                {
                    // Assignment operator.  If the destination of the ASGOP is a local var
                    // that crosses EH then it needs to be written through.
                    GenTreePtr replacement = RewriteAssignment(comp, node, destination, proxyId);
                    *pTree = replacement;
                    *changed = true;
                }
            }
        }
    }
    // Local use case.  Replace local var with enregisterable proxy.
    else if (node->OperIsScalarLocal() && ((node->gtFlags & GTF_VAR_DEF) == 0))
    {
        unsigned proxyLclNum;
        bool found = proxyVarsMap->TryGetValue(node->gtLclVarCommon.gtLclNum, &proxyLclNum); 

        if (found)
        {
            JITDUMP("Found EH local var:\n");
            DISPTREE(node);

            // Decrement counts for the local var about to be replaced.
            comp->lvaDecRefCnts(node);
            // Local var use.  Just hammer the GT node in place to the new proxy.
            node->gtLclVarCommon.SetLclNum(proxyLclNum);
            // Increment counts for the new local var apperance.
            comp->lvaIncRefCnts(node);
            
            JITDUMP("Rewritten to proxy:\n")
            DISPTREE(node);
        }
    }

    // continue walk to cover entire tree
    return Compiler::WALK_CONTINUE;
}

// RewriteAssignment
// 
// Parameters:
//
//   node        - Assignment node to rewrite.
//   destination - Op1 destination node from the assignment node.  Passed this way for
//                    Conenience as the validation code in the caller needs to chase the 
//                    pointers already.
//   proxyId     - Proxy local var index.
//
// Notes:
//
//   Rewrite assignment trees containing definitions of local vars live across EH flow to
//   a comma which first assigns to an enregisterable proxy and then back to the original
//   local var on the stack.
//
// Returns:
//
//   New tree implementing the double assign via comma operator.
//
GenTreePtr WriteThru::RewriteAssignment(Compiler* comp, GenTreePtr node, GenTreePtr destination, unsigned proxyId)
{
    unsigned lclVarNum = destination->AsLclVarCommon()->GetLclNum();
    var_types type = node->gtType;
    GenTreePtr frameLclVar = comp->gtNewLclvNode(lclVarNum, type);
    GenTreePtr proxy = comp->gtNewLclvNode(proxyId, type);
    GenTreePtr frameAssign = comp->gtNewAssignNode(frameLclVar, proxy);

    // Rewrite destination in place to be the proxy local var.
    destination->gtLclVarCommon.SetLclNum(proxyId);
    GenTreePtr comma = comp->gtNewOperNode(GT_COMMA, node->gtType, node, frameAssign);

    JITDUMP("After rewriting assignment:\n")
    DISPTREE(comma);

    return comma;
}

void WriteThru::CreateEnregisterableProxies()
{
    proxyVarsMap = new ProxyVarsMap(comp);

    VARSET_ITER_INIT(comp, iter, EHVars, varIndex);
    while (iter.NextElem(comp, &varIndex))
    { 
        unsigned varNum = comp->lvaTrackedToVarNum[varIndex];
        LclVarDsc* localVar = &comp->lvaTable[varNum];

        // Only create proxies for tracked non-struct local vars.
        if (localVar->lvTrackedNonStruct())
        {
            unsigned proxyVarNum = comp->lvaGrabTemp(false DEBUGARG(" Add proxy for EH Write Thru."));  
            comp->lvaTable[proxyVarNum].lvType = comp->lvaTable[varNum].lvType;

            JITDUMP("Creating proxy V%02u for local var V%02u\n", proxyVarNum, varNum);

            bool added = proxyVarsMap->AddOrUpdate(varNum, proxyVarNum);

            // All new proxies should be adds not updates.
            assert(added);
        }
    }
}
