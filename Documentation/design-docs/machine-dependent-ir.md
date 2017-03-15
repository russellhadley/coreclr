# Machine Dependent Representation and Optimization in RyuJIT
## Introduction
RyuJIT is built on a tested and production proven base that utilizes a lean and factored tree form – GenTree nodes – as an intermediate representation.  This form is maintained and annotated throughout the compiler and used as a base for both high and low level optimizations, including register allocation, with CodeGen at end expanding and collecting all the needed information to emit machine code bytes.  While this form allows developers to work largely at a higher level it does make certain kind of transformations more difficult – a particular concern as more collaborators work to add and improve new targets.  At a high level these transformations include code selection for machine specific operations, forming addressing modes, scheduling (a particular concern for arm32 where it matters) and optimization of constants for encoding size in the instruction stream.  Additionally these transformations can expose secondary opportunities for late copy and constant prop as well as basic block scope value tracking.  Today these details are behind a higher level abstraction and only surfaced at the very end of compilation making optimization more difficult.  This document shows how the GenTree node representation can be extended to capture this information and provide a basis for us and our partners to optimize more easily for machine targets.

## The need for a machine dependent representation and optimizations

### Identifying Target Instructions Before Register Allocation

### Representing Target Instructions On GenTree Nodes

### Machine Targets of Interest

Machine targets that are of interest and have varing levels of support in RyuJIT are:
-	X64.  X64 is the premier target on .NET Core and is a complex instruction set machine for which there is a substancial opportunity for instruction selection improvements.  Highest priority opportunitiues are boolean/cmp operations, forming more complex address modes, and removing redundant register operations.
-	X86.  X86 has been open sourced and will be supported as a production quality JIT for .NET Core 2.0.  This ISA is nearly identical to X64 with respect to opportunity with an added emphasis on being able to rework instruction selction to avoid register pressure.
-	ARM32.  ARM32 is currently only preview support via the legacy code gen passes.  This instruction set is a 16bit/32bit hybrid load/store architecture where the primary opportunities are in address mode building, immediate handling, and cmp optimizations.
-	ARM64. ARM64 support is in early preview support.  It's a fixed 32 bit word design that exhibits a number of the same issues as ARM32.  Address mode building and immediate handling.  With the very same emphasis on cmp opts that we've seen in all the other targets.

### Opportunites

Case study 1: Kestrel
One of the premier work loads for .NET Core is ASP.NET.  The primary component of this for performance is Kestrel, the web server.  The following samples come from a dev branch of Kestrel that is running on head of tree CoreCLR on 3/14/2017


Case study 2: Forming bt

Case study 3: ARM64 integer encoding (cross statement constant exploding)

Case study 4: ARM32 instruction scheduling

# Machine Level Instruction Representation

Design

Phase Ordering

Register Allocation Impact

# Conclusion
