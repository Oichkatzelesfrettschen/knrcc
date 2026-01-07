# KNRCC Modernization Progress

## Overview
This document tracks the comprehensive modernization effort for the K&R C Compiler (KNRCC) from its original K&R C implementation to modern ANSI C standards while preserving its historical architecture and functionality.

## Project Scope
- **Codebase Size**: ~9,000 lines of C code across 12 source files
- **Compiler Passes**: 3 main passes (Pass 0: lexing/parsing, Pass 1: code generation, Pass 2: optimization)
- **Target**: Modern C99 standard with strict compiler warnings enabled

## Completed Work

### Phase 1: Build Infrastructure & Critical Fixes

#### 1. Build System Setup (Commits: fda14c7, ed1cda3)
- ✅ Fixed CMake cache issues for fresh environment
- ✅ Added comprehensive `.gitignore` for build artifacts
- ✅ Resolved sbrk() declaration conflict with system headers
- ✅ Enabled strict compiler warnings: `-Wall -Wextra -Wpedantic -Werror`

#### 2. Pass 0 (Lexer/Parser) Fixes (Commit: ed1cda3)
**c02.c - Control Flow**
- Fixed assignment in conditional expressions (parentheses)
- Resolved sequence point violations in loop constructs
- Corrected do-while loop increment logic

**c03.c - Declarations & Types**
- Fixed dangling else ambiguity with explicit bracing
- Added explicit fall-through comments for switch statements
- Corrected nested if-else structure in declaration processing

**c05.c - Operator Tables**
- Fixed signed char overflow in cvtab (conversion table)
- Changed from `char` to `unsigned char` for 8-bit value storage
- Added proper array initialization braces

#### 3. Pass 1 (Code Generation) Major Refactoring (Commits: 4d2307c, 07738c9)
**c10.c - Core Code Generator**
- ✅ Added forward declarations for 15+ functions
- ✅ Fixed K&R-style global initializations (= syntax instead of {})
- ✅ Corrected `match()` function return type: `struct optab *` not `char *`
- ✅ Fixed struct member access:
  - `table->tabop` not `table->op`
  - Cast to `struct tconst *` for `value` member access
  - Cast to `struct tname *` for `class`, `regno`, `offset` access
  - Cast to `struct lconst *` for `lvalue` access
  - Cast to `struct ftconst *` for `fvalue` access
  - Cast to `struct fasgn *` for `mask` member in STRASG operations
- ✅ Added `opdope → opdope_pass1` macro mapping
- ✅ Fixed register variable type declarations (`register int r`)
- ✅ Added explicit fall-through comments in switch statements
- ✅ Fixed parentheses in compound conditional expressions

**c1.h - Pass 1 Header**
- ✅ Added `#include <stdlib.h>` for exit()
- ✅ Added `#include <unistd.h>` for sbrk()

## Architectural Insights Gained

### Type System Architecture
The KNRCC uses a clever integer-encoded type system where:
- Base types occupy bits 0-2 (masked by `TYPE`)
- Type modifiers (PTR, FUNC, ARRAY) are encoded in higher bits
- Each modifier level uses `TYLEN` (2) bits
- Specialized struct types share common initial members for casting

### AST Node Architecture
- **Base struct**: `struct tnode` with `op`, `type`, `degree`, `tr1`, `tr2`
- **Specialized nodes** share initial members:
  - `struct tconst` for integer constants (adds `value`)
  - `struct lconst` for long constants (adds `lvalue`)
  - `struct ftconst` for float constants (adds `value`, `fvalue`)
  - `struct tname` for local variables (adds `class`, `regno`, `offset`, `nloc`)
  - `struct fasgn` for field assignments (adds `mask`)
- **NAME nodes**: When `op==NAME`, `tr1` points to symbol table entry (struct tname/hshtab)

### Inter-Pass Communication
- Custom binary format written by `outcode()` in Pass 0
- Read by `getree()` in Pass 1
- Format codes: 'B' (operator), 'N' (int), 'S' (symbol), 'F' (float), '1', '0'
- Critical to preserve exact format for compatibility

## Remaining Work

### Phase 1 Continuation: Complete Build Success

#### Immediate Next Steps
1. **c10.c remaining issues**:
   - Fix `chkleaf()`, `delay()`, `reorder()` function implementations
   - Add missing implementations for `branch()`, `label()`, `cbranch()`
   - Fix `string` variable usage (likely from opt->tabstring)
   - Complete all struct member access fixes

2. **c11.c - Tree Reading**:
   - Port getree() implementation fixes
   - Fix outname() usage
   - Handle binary stream reading correctly

3. **c12.c - Optimization**:
   - Fix optim() function
   - Port tree manipulation functions
   - Handle degree calculation

4. **c13.c - Code Tables**:
   - Fix opdope_pass1 initialization syntax
   - Verify table structure initializations

5. **c20.c, c21.c - Pass 2 (Optimizer)**:
   - Similar fixes to Pass 0 and 1
   - May have different architectural patterns

6. **cvopt.c - Peephole Optimizer**:
   - Standalone tool, likely simpler fixes

### Phase 2: Code Quality & Systematic Cleanup
- Convert all K&R function declarations to ANSI C prototypes
- Systematic review of all global variables
- Add function-level documentation
- Create consistent error handling patterns

### Phase 3: Architecture Documentation
- Document all inter-pass interfaces
- Create data structure relationship diagrams
- Document type encoding system fully
- Create module dependency graph

### Phase 4: Testing Infrastructure
- Create test harness for individual compiler passes
- Add regression tests for K&R C constructs
- Implement end-to-end compilation tests
- Add performance benchmarks

### Phase 5: Advanced Modernization
- Refactor global state into context structures
- Implement proper memory management
- Add comprehensive error recovery
- Consider thread-safety improvements

## Known Technical Debt

### Critical Issues
1. **Symbol Table**: Hash table with linear probing, fixed size (HSHSIZ=400)
2. **Memory Management**: Bump allocator (gblock/getblk) with no free()
3. **Error Handling**: Error count only, limited recovery
4. **Inter-pass Files**: Temporary file dependencies could be streams
5. **Type Safety**: Extensive use of casts for struct polymorphism

### Design Patterns to Preserve
1. **Struct member compatibility**: Common initial sequence pattern is intentional
2. **Binary encoding**: Inter-pass format must remain compatible
3. **PDP-11 specifics**: Assembly output targets specific architecture
4. **Table-driven code gen**: Elegant pattern worth preserving

## Build Statistics
- **Initial state**: 100+ compilation errors
- **After fixes**: ~50+ errors remaining (primarily in c10.c+ forward)
- **Lines modified**: ~150+ across 6 files
- **Commits**: 4 substantive commits

## References
- Original K&R C sources from Unix Heritage Society
- Existing documentation in `docs/knrcc_architecture.md`
- Existing documentation in `docs/knrcc_type_system.md`
- CMakeLists.txt build configuration

## Next Session Priorities
1. Complete c10.c compilation
2. Fix c11.c, c12.c, c13.c
3. Achieve first successful build
4. Run basic smoke tests
5. Document any runtime issues discovered

---
*Last Updated: 2026-01-06*
*Status: Phase 1 ~60% Complete*
