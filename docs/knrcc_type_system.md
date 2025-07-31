# KNRCC Type System Model

## 1. Introduction
The K&R C Compiler (KNRCC) employs an integer-based type encoding system. This system represents base types and type modifiers (like pointers, functions, and arrays) as bit patterns within a single integer. This approach was common in early C compilers due to memory constraints and the desire for efficient type manipulation through bitwise operations. The understanding of this system is crucial for modernizing KNRCC while preserving its ability to compile K&R C code.

## 2. Core Constants & Definitions
These constants are primarily found in `c/c0.h` and `c/c1.h`.

*   **`#define TYPE 07`** (octal: `0b111`):
    *   **Meaning:** A mask for the lowest 3 bits of a type-integer.
    *   **Function:** Extracts the base data type (e.g., INT, CHAR).

*   **`#define TYLEN 2`**:
    *   **Meaning:** Represents the number of bits to shift when adding or removing a level of type modification (e.g., a pointer or function modifier).
    *   **Function:** Defines the "width" of a type modifier "syllable" in the integer encoding. Each level of type modification (pointer to, function returning, array of) occupies `TYLEN` bits.

*   **Base Type Values (Bits 0-2, masked by `TYPE`):**
    *   `INT: 0` (`0b000`)
    *   `CHAR: 1` (`0b001`)
    *   `FLOAT: 2` (`0b010`)
    *   `DOUBLE: 3` (`0b011`)
    *   `STRUCT: 4` (`0b100`)
    *   `RSTRUCT: 5` (`0b101`) (Specific to `c1.h`, likely "register struct" or related to struct return conventions)
    *   `LONG: 6` (`0b110`)
    *   `UNSIGN: 7` (`0b111`)
    *   `UNION` is defined as `8` in `c0.h` but a comment suggests it's "adjusted later to struct", likely meaning it's remapped to `STRUCT` (4) or handled via `strp` (pointer to struct/union description).

*   **Type Modifier Constants (used with bitwise OR and shifts):**
    These define the *kind* of modification being applied at a given level. They are shifted into position using `TYLEN`.
    *   `#define PTR 010` (octal: `0b001000` in `c0.h`) - Represents a pointer.
    *   `#define FUNC 020` (octal: `0b010000` in `c0.h`) - Represents a function.
    *   `#define ARRAY 030` (octal: `0b011000` in `c0.h`) - Represents an array.
    *(Note: The actual values `010`, `020`, `030` are important as they are distinct bit patterns. If `TYLEN` is 2, these values would typically be smaller, like `01`, `02`, `03`, to fit into 2 bits before shifting. However, the KNRCC code uses these larger values and incorporates them via direct ORing after shifting the existing type, implying these might be absolute bit flags for the *first* level of modification, or that `incref` logic is more subtle than just `(modifier_kind << TYLEN_shift_amount)`.)*
    A more accurate interpretation from `incref` and `decref` is that `PTR`, `FUNC`, `ARRAY` are *values* that are OR'd in after the existing type is shifted, not kinds that are themselves shifted.

*   **`#define XTYPE (03<<3)`** (octal `030` or `0b011000`):
    *   **Meaning:** This constant is used as a mask, typically `(t & XTYPE)`.
    *   **Function:** Originally thought to check the kind of the current outermost type modifier. However, given `TYLEN` is 2, and `PTR`, `FUNC`, `ARRAY` are `010`, `020`, `030` respectively, `XTYPE` (030) might be specifically masking for `ARRAY` or a combination. More likely, `t & ~TYPE` is used to isolate all modifier bits. `XTYPE`'s precise role needs further clarification by observing its use in context, especially in relation to `TYLEN`. If `PTR` is `(1<<3)`, `FUNC` is `(2<<3)`, `ARRAY` is `(3<<3)`, then `XTYPE` being `(03<<3)` would mask these "kind" bits.

## 3. Type Encoding Scheme

A type is represented as an integer. The base type (INT, CHAR, etc.) resides in the lowest 3 bits (masked by `TYPE`). Modifiers (PTR, FUNC, ARRAY) are encoded in higher bits. Each level of modification effectively shifts the existing *entire type integer* (including its base type and any previous modifiers) to the left by `TYLEN` bits and then ORs the new modifier *kind* (PTR, FUNC, ARRAY) into the bits that were previously the base type (now shifted).

Let's re-evaluate `incref` based on common K&R practices and the provided constants:

**Corrected `incref(t, mod)` interpretation (hypothetical, assuming `mod` is `PTR`, `FUNC`, or `ARRAY`):**
`return ((t << TYLEN) | mod | (original_base_type));`  -- This is still not quite right.

**Let's use the derived logic from the successfully refactored `incref` and `decref` in `c04.c`:**

*   **`incref(t)` (Adds a PTR modifier specifically):**
    *   `return (((t & ~TYPE) << TYLEN) | (t & TYPE) | PTR);`
    *   `t & ~TYPE`: Isolates all existing modifier bits (e.g., if `t` is `int[]`, this gets `ARRAY` appropriately shifted).
    *   `<< TYLEN`: Shifts these existing modifiers left by `TYLEN` (2 bits), making space for the new `PTR` modifier.
    *   `| (t & TYPE)`: ORs back the original base type into the lowest 3 bits.
    *   `| PTR`: ORs in the `PTR` flag. The key is how `PTR` (010) fits. If `TYLEN` is 2, the bits made available by the shift are bits 3 and 4 (assuming 0-indexed). `PTR` (010 = 8 decimal) would set bit 3.
    *   **Example:** `CHAR` is `1` (`0b001`).
        *   `incref(CHAR)` (to get `char *`):
            *   `t & ~TYPE` = `1 & ~07` = `0`.
            *   `0 << 2` = `0`.
            *   `t & TYPE` = `1 & 07` = `1`.
            *   `0 | 1 | 010` (PTR) = `1 | 010` = `011` (octal). This is `char *`.
    *   **Example:** `char *` is `011` (octal).
        *   `incref(char *)` (to get `char **`):
            *   `011 & ~07` = `010`.
            *   `010 << 2` = `040`.
            *   `011 & 07` = `001`.
            *   `040 | 001 | 010` (PTR) = `041 | 010` = `051` (octal). This is `char **`.

*   **`decref(t)` (Removes one level of indirection):**
    *   `return (((t >> TYLEN) & ~TYPE) | (t & TYPE));`
    *   `t >> TYLEN`: Shifts the entire type integer right by `TYLEN` bits. This effectively discards the modifier at the "lowest" modifier level (e.g., the first `PTR` in `char **`) and moves the next level of modifiers (e.g., the second `PTR`) and the original base type down.
    *   `& ~TYPE`: This correctly isolates the higher-level modifiers *after* they have been shifted down. For example, if `t` was `char **` (051), `t >> TYLEN` is `051 >> 2 = 012`. Then `012 & ~07` = `010` (which is `PTR`).
    *   `| (t & TYPE)`: This ORs back the original base type (which was part of the initial `t` and also got shifted, but `& TYPE` will still extract it correctly if it was in the lower bits of the original `t`).
    *   **Example:** `char *` is `011`.
        *   `decref(char *)` (to get `CHAR`):
            *   `011 >> 2` = `02` (octal).
            *   `02 & ~07` = `0`.
            *   `011 & 07` = `1`.
            *   `0 | 1` = `1` (`CHAR`).
    *   **Example:** `char **` is `051`.
        *   `decref(char **)` (to get `char *`):
            *   `051 >> 2` = `012` (octal).
            *   `012 & ~07` = `010` (this is `PTR`).
            *   `051 & 07` = `1` (this is `CHAR`).
            *   `010 | 1` = `011` (`char *`).

This encoding scheme allows for types like `int`, `char *`, `int **`, `char *()[]` (array of function returning pointer to char) to be represented. The `TYLEN` constant determines how many bits each "syllable" of pointer/function/array takes.

## 4. Key Type Manipulation Functions (from `c/c04.c`)
(K&R Signatures shown, with notes on ANSI conversion based on successful refactoring)

*   **`length(acs)` -> `int length(struct tnode *acs)`**
    *   Calculates the size of a type in bytes.
    *   Uses `acs->type` to determine the base type and modifiers.
    *   Handles base types using size constants like `SZCHAR`, `SZINT` (defined in `c0.h`).
    *   For arrays (`(acs->type & XTYPE) == ARRAY`): Recursively calls `length()` on the element type (obtained via `decref(acs->type)`) and multiplies by element count (obtained from `acs->subsp[0]`, assuming `subsp` holds dimension info).
    *   For structs/unions (`(acs->type & XTYPE) == STRUCT`): Uses `acs->strp->ssize` (size stored in a `struct str` pointed to by `strp`).
    *   For pointers (`(acs->type & XTYPE) == PTR`): Returns `SZPTR`.
    *   Returns `0` for functions (`(acs->type & XTYPE) == FUNC`).

*   **`incref(t)` -> `int incref(int t)`**
    *   As described in "Type Encoding Scheme". Adds a `PTR` modifier to the type integer `t`.

*   **`decref(at)` -> `int decref(int at)`**
    *   As described in "Type Encoding Scheme". Removes one level of indirection from the type integer `at`.
    *   Includes an error check: `if (((at&~TYPE)&(ARRAY|FUNC|PTR))==0) error("indirection");`

*   **`plength(ap)` -> `int plength(struct tnode *ap)`**
    *   (K&R definition took `struct tname *ap`). Calculates the size of the type pointed to.
    *   **Logic:**
        1.  Saves the original type: `register int t_orig = ap->type;`
        2.  Checks if `ap` is null or its type is not a reference (pointer/array/function). If so, returns `1` (default size, perhaps for `char` or an error condition). The check `((t_orig & ~TYPE) == 0)` is likely more accurate here.
        3.  Temporarily modifies `ap->type` to the dereferenced type: `ap->type = decref(t_orig);`
        4.  Calls `length(ap)` to get the size of this dereferenced type.
        5.  Restores `ap->type` to its original value: `ap->type = t_orig;`
        6.  Returns the calculated length.
    *   Relies on `struct tname` and `struct tnode` having a common initial sequence (`op`, `type`) for the implicit cast/access to `ap->type` to work correctly, especially if `ap` was originally `struct tname *`.

## 5. Structural Relationships (`tnode`, `tname`)

*   **`struct tnode`**: General parse tree node. Its exact definition varies between `c0.h` (for Pass 0) and `c1.h` (for Pass 1), indicating pass-specific interpretations or that `tnode` is part of a union.
    *   **`c0.h` version (Pass 0):** `int op, type; int *subsp; struct str *strp; struct tnode *tr1, *tr2;`
        *   `subsp`: Likely for array dimensions or other subscript info.
        *   `strp`: Pointer to a `struct str` which describes struct/union members and size.
    *   **`c1.h` version (Pass 1):** `int op, type; int degree; struct tnode *tr1, *tr2;`
        *   `degree`: Possibly related to expression tree complexity or register allocation needs.

*   **`struct tname` (defined in `c1.h`):** Represents named entities, specifically local variables or parameters in Pass 1.
    *   `int op, type; char class, regno; int offset, nloc;`
        *   `class`: Storage class.
        *   `regno`: Assigned register number.
        *   `offset`: Offset from stack or frame pointer.
        *   `nloc`: Likely name location/identifier for the symbol.

*   **Interchangeability & Common Initial Sequence:**
    *   Both `struct tnode` (in its `c1.h` variant) and `struct tname` start with `int op, type;`.
    *   This common initial sequence allows a pointer to one type to be cast to a pointer of the other type to access these common initial members. This is crucial for functions like `plength` in `c04.c` (part of Pass 0 utilities but potentially operating on node structures prepared for or by Pass 1 if types are resolved and passed around). More accurately, `plength` as defined in `c04.c` takes `struct tnode *` (after modernization) and would operate on the Pass 0 view of `tnode`. If it originally took `struct tname *`, it implies `tname` was also used or understood in Pass 0 contexts, or `plength` was also a utility for Pass 1. The refactored `plength` in `c04.c` correctly uses `struct tnode *`.

## 6. Open Questions & Further Investigation

*   **`XTYPE` Constant:** The exact usage and meaning of `XTYPE ((03<<3))` needs to be fully clarified by examining all its call sites. Is it a general modifier mask, or specific to `ARRAY` and `FUNC` (as `PTR` `010`, `FUNC` `020`, `ARRAY` `030` suggests `03` could cover `01` and `02` shifted by 3)?
*   **`struct str`:** The full definition and usage of `struct str` (pointed to by `tnode->strp`) is needed to understand struct/union type representation and size calculation.
*   **`opdope` Flags:** The various flags within `opdope` entries (e.g., `BINARY`, `LVALUE`, `RVALUE`, type conversion flags) are integral to how types are handled during expression evaluation and code generation. A full map of these flags is needed.
*   **Pass-Specific Structures:** Confirm how `tnode` variants and other structures like `struct hshtab` (symbol table) and `struct phshtab` are used and transformed between compiler passes.

This document provides a foundational understanding of the KNRCC type system. It will be updated as further investigations yield more details.
