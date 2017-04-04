## Morph phase (Global Morph)

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

