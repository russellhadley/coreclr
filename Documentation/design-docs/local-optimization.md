# Local Optimization Phase in RyuJIT

Today in RyuJIT optimizations on trees, the simplest local scope, take place during the Global Morph Phase.  At a high level global morph canonicalizes each IR tree and then performs local tree scope optimizations for each statement in the program. 

This pass of local optimization is a reasonable, common sense, kind of design.  This very reasonableness led to Morph being one of the first pieces of code written in the JITs.  (this code stretches all the way back to JIT32) Over the years this code as grown, and functionality that does canonicalization has been mingled with optimizations to the point where the two concerns are very hard to demarcate.  Additionally the optimizations staged were largely driven by observed issues and there are still gaps, no forward substitution, incomplete algebraic simplification for instance, that can lead to a "peanut butter" performance tax in the generated code.

A few things have changed that raises problems with the current status quo.

- Cloud scenarios have more managed code that is performance critical.  [Micro]service architectures rely on a great deal of data processing and are measured in requests per second.
- More of the underlying stack is being written in managed code to improve portability. More code is being pulled into the managed part of the frameworks to limit the number of separate native components that need to be validated and tested for disparate platforms.
- The JIT is moving to a tiered compilation architecture. To address startup and performance goals the runtime is being modified to allow the JIT to recompile performance critical methods.  It this architecture the JIT will need be as lean as possible in the first compile, and then more aggressive if runtime data shows the method important.

Morph today is a one size fits all design.  All these related concerns run for each invocation of the utility, and it's complexity makes it difficult to add new optimizations.

This leads to the design objective.  How do we make canonicalization faster for tier 1 compiles, while at the same time making optimizations more capable for tier 1+n compiles?

### And the other thing morph does

Morph the utility, the set of functions that canonicalize/optimize trees, is also called in other contexts, most notably the global optimizer, when trees are modified.  This at times can lead to a careful dance where either morph, or the running global opt phase, has to be careful that morph doesn't modify the code in a way that invalidates the analysis or violates an invariant.

# Design

To meet the twin goals of high throughput canonicalization tier 1 and high code quality at higher tiers, the reworked morph phase, or as referred to after this, the local opt phase, will consist of a single walk of the IR in the method where for each block, for each tree, every node will have a list of separate transforms run on it.  The easiest way to think of it is as a phase list that is run on each node in a post order visit.

Outline of the algorithm:

foreach block in method
  foreach statement in block
    foreach node in statement
      vist node in pre-order for pre-order transforms
      visit node in post-order for post-order transforms

The standard walkers we have in the JIT today will be extended to allow the specification of a list of functors that are registered to either be in the pre-order or post-order visit.  These functors are then called in order on the node to effect transformations.

Tier 1:
- Insert class constructor
- Insert GC check obj helpers
- Remove empty blocks
- Mark implicit byref
- Expand field access
- Expand QMark

Tier 2+:
- Tier 1 list and
- EH opts
- Forward substitution
- Canonicalize shape
- Mark address exposed
- Algebraic simplification
- Local redundancy elimination

## Open questions
- What IR adjustments if any should we make to improve the ability to do tree transforms?
- Can we make summary side effect info computed on demand or included as part of the IR tree walk?
- Can some of the local SSA info be computed on the fly as part of this walk?


# Morph phase today (Global Morph)

- Legalize flow graph
  * Insert class constructor
  * Insert GC check obj helpers
  * Remove empty blocks
- Inline
- Add internal (TODO)
- EH opts
  * Remove empty try{}
  * Remove empty finally{} 
  * Merge finally chains
  * Clone finally
- Mark implicit byref
- Promote struct
- Mark address exposed
- Morph blocks/statements/trees
- Expand QMark

## Morph Statements
List of transforms:
- Cast into helper call 
- Processing longs to avoid transformations. (is this still needed in decomp world?)
- QMark processing.  Should support this as first class.
- Fold away casts.  (tied particularly to assignments so not as fully featured as we might need)
- Mark problematic constructs as no CSE - should we just either put these checks in CSE?  Or harden the IR so this isn't needed?
- Optimize EQ/NE checks against constants. (moves constants to one side of compare and do compile time calculation)
- Optimize EQ/NE aginst 0/1
- Family of rewrites for EQ/NE trees to remove comma cases.
- Optimize LT/LE/GT/GE against constant 1/-1/0
- Change sub of const to add of negative const.
- Convert add/mul to jump to overflow (rather than exception in block)
- Re-associate bool ops to combine constants.
- Fold Muls
- Rewrite mod as sub/div/mul
- Recognize rotate OR/XOR and transform to ROL/ROR
- Rewrite FP to helper calls.
- Fold BBJ {COND|SWITCH} of const to BBJ ALWAYS
- Process call arguemnts for ABI
- Expand QMark
- Fold field to local var
- (optional) Reorder nested operators at same precedence to be left recursive.
- (optional) Reorder trees to place constants to the right.
- (optional) replace divide by 1 with numerator
- (optional) Shift by contant opts
- (optional) XOR by 1/-1


Issues:
- Uneven application of simplification rules.
- Lots of defensive code about running during CSE.
- Hand written postorder traversal, makes staging new transforms trickier

