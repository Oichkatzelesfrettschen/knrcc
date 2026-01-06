# KNRCC Modernization Best Practices

## Overview
This document captures best practices, patterns, and lessons learned during the modernization of KNRCC from K&R C to ANSI C.

## Code Modernization Patterns

### 1. Struct Member Access Pattern

**Problem**: KNRCC uses struct polymorphism via common initial sequences. Different specialized structs (tconst, lconst, ftconst, tname, fasgn) are accessed through base struct tnode pointers.

**Solution**: Use explicit casts when accessing specialized members:

```c
// Correct - cast before accessing specialized member
if (tree->op == CON) {
    int val = ((struct tconst *)tree)->value;
}

// Wrong - direct access through wrong type
int val = tree->value;  // Error: struct tnode has no member value
```

**Pattern Catalog**:
```c
// Integer constants
tree->op == CON → cast to (struct tconst *)tree for ->value

// Long constants  
tree->op == LCON → cast to (struct lconst *)tree for ->lvalue

// Float constants
tree->op == FCON → cast to (struct ftconst *)tree for ->fvalue

// Variable names
tree->op == NAME → cast to (struct tname *)tree for ->class, ->regno, ->offset

// Structure assignments
tree->op == STRASG → cast to (struct fasgn *)tree for ->mask
```

### 2. Function Return Type Corrections

**Problem**: K&R C allowed implicit int return types, causing type mismatches.

**Before**:
```c
char *
match(struct tnode *tree, struct table *table, int nrleft, int nocvt)
{
    ...
    return(opt);  // opt is struct optab *, not char *
}
```

**After**:
```c
struct optab *
match(struct tnode *tree, struct table *table, int nrleft, int nocvt)
{
    ...
    return(opt);  // Type-correct
}
```

### 3. Register Variable Type Declarations

**Problem**: K&R C allowed `register` without type.

**Before**:
```c
register r;  // Implicit int
```

**After**:
```c
register int r;  // Explicit type
```

### 4. Array and Struct Initialization

**Problem**: K&R C initialization syntax differs from ANSI C.

**Before**:
```c
int opdope[SIZE] {
    values...
};

char cvtab[4][4] = {
    val1, val2, val3, val4,  // No row grouping
    val5, val6, val7, val8,
    ...
};
```

**After**:
```c
int opdope[SIZE] = {
    values...
};

unsigned char cvtab[4][4] = {
    {val1, val2, val3, val4},  // Proper row grouping
    {val5, val6, val7, val8},
    ...
};
```

### 5. Struct Global Initialization

**Problem**: K&R C used special syntax for struct initialization.

**Before**:
```c
struct tconst czero { CON, INT, 0};
int nreg	3;
```

**After**:
```c
struct tconst czero = { CON, INT, 0};
int nreg = 3;
```

### 6. Assignment in Conditionals

**Problem**: Assignment in conditional requires explicit parentheses.

**Before**:
```c
if (opt = match(tree, table, nrleft, NOCVL))
    return(opt);
```

**After**:
```c
if ((opt = match(tree, table, nrleft, NOCVL)))
    return(opt);
```

### 7. Switch Statement Fall-Through

**Problem**: Modern compilers warn about implicit fall-through.

**Solution**: Add explicit comments:

```c
switch (op) {
    case STRUCT:
    case UNION:
        // ... code ...
        /* fall through */
    case INT:
        // ... code ...
        break;
}
```

### 8. Sequence Point Violations

**Problem**: Operations that modify and use variable in same expression.

**Before**:
```c
while ((cs = (cs<&hshtab[HSHSIZ-1])? ++cs: hshtab) != endcs);
```

**After**:
```c
do {
    // ... loop body ...
    if (cs < &hshtab[HSHSIZ-1])
        cs++;
    else
        cs = hshtab;
} while (cs != endcs);
```

### 9. Operator Precedence Warnings

**Problem**: Bitwise operators vs logical operators need parentheses.

**Before**:
```c
if (opdope[op]&RELAT && tree->tr2->op==CON)
    ...
```

**After**:
```c
if ((opdope[op]&RELAT) && tree->tr2->op==CON)
    ...
```

### 10. Type-Safe Macro Definitions

**Problem**: opdope table name varies by pass.

**Solution**: Use macro to map to correct table:

```c
// In c10.c (Pass 1)
#define opdope opdope_pass1

// In c00.c (Pass 0) - if needed
#define opdope opdope_pass0
```

## Forward Declaration Strategy

### When to Add Forward Declarations
1. **Mutual recursion**: Functions that call each other
2. **Complex call graphs**: When function order doesn't match usage order
3. **Clarity**: When function is used before definition in large files

### Pattern:
```c
/* Forward declarations */
struct optab *match(struct tnode *atree, struct table *table, int nrleft, int nocvt);
struct tnode *sdelay(struct tnode **ap);
struct tname *ncopy(struct tname *ap);
struct tnode *optim(struct tnode *atree);
int comarg(struct tnode *atree, int *flagp);
int cexpr(struct tnode *atree, struct table *atable, int areg);
void doinit(int atype, struct tnode *atree);
```

## Header File Best Practices

### Include Order
```c
#include <stdio.h>      // Standard C library
#include <stdlib.h>     // exit(), malloc(), etc.
#include <unistd.h>     // POSIX functions (sbrk, etc.)
#include "array_sizes.h"  // Local configuration
#include "c0.h"          // Local headers
```

### Header Guards
All headers should have:
```c
#ifndef C0_H
#define C0_H

// ... content ...

#endif /* C0_H */
```

## Common Pitfalls to Avoid

### 1. Don't Remove Historical Type Casts
The struct casting pattern is intentional and necessary for the polymorphic node system.

### 2. Preserve Binary Format Compatibility
Changes to outcode()/getree() format will break inter-pass communication.

### 3. Maintain struct Member Order
Common initial sequence pattern requires exact member ordering in structs.

### 4. Keep Symbol Table Hash Function
The lookup() function's hash algorithm must remain unchanged for consistency.

### 5. Preserve Global Variable Semantics
Many globals coordinate state between passes - don't casually eliminate them.

## Testing Strategy for Changes

### After Each Change
1. Attempt compilation
2. Fix immediate errors
3. Re-test
4. Commit if progress made

### Before Large Refactoring
1. Document current behavior
2. Create test inputs
3. Verify outputs match before/after

### Integration Points to Test
1. Pass 0 → Pass 1 (intermediate file format)
2. Pass 1 → Pass 2 (assembly format)
3. Symbol table lookups
4. Type encoding/decoding

## Performance Considerations

### Keep What Works
1. **Table-driven code generation**: Elegant and efficient
2. **Bump allocator**: Simple and fast for compiler use case
3. **Hash table with linear probing**: Works well at scale
4. **Binary intermediate format**: Compact and fast

### Potential Improvements (Future)
1. **Memory pools**: Replace bump allocator with proper pools
2. **Better error recovery**: Don't stop at first error
3. **Streaming**: Replace temp files with pipes/streams
4. **Incremental compilation**: Cache intermediate results

## Documentation Standards

### Function Documentation
```c
/*
 * Brief description of function purpose
 * 
 * Parameters:
 *   name - description
 *   
 * Returns:
 *   description of return value
 *   
 * Side effects:
 *   Any global state modifications
 */
```

### Complex Algorithm Documentation
```c
/*
 * Algorithm: [Name]
 * 
 * Overview:
 *   High-level description
 *   
 * Details:
 *   Step-by-step explanation
 *   
 * Example:
 *   Concrete example of operation
 */
```

## Compiler Warning Flags

### Current Configuration
```cmake
-Wall          # Enable all warnings
-Wextra        # Extra warnings
-Wpedantic     # ISO C compliance
-Werror        # Treat warnings as errors
```

### Justification
Strict warnings catch:
- Type mismatches
- Missing return statements
- Unused variables
- Implicit declarations
- Sequence point violations

## Version Control Best Practices

### Commit Message Format
```
[Component] Brief description of change

- Detailed point 1
- Detailed point 2
- Related issue/rationale
```

### Commit Scope
- One logical change per commit
- Keep compilable states when possible
- Group related fixes together

### Example Good Commits
- "Fix compiler warnings in c02.c (parentheses, sequence points)"
- "Add forward declarations and fix c10.c structure member access"
- "Fix critical c10.c issues: function signatures, struct member access, type casts"

## References

### K&R C vs ANSI C Differences
- Function declarations/definitions syntax
- Struct initialization syntax
- Type requirements for variables
- Implicit int elimination

### Standards Documents
- ISO/IEC 9899:1999 (C99)
- Original K&R "The C Programming Language"
- PDP-11 Architecture Manual

---
*This document should be updated as new patterns emerge during modernization.*
