# KNRCC Compiler Architecture (Preliminary)

This document outlines the preliminary understanding of the KNRCC compiler's architecture, focusing on inter-pass communication and key data structures like the symbol table. This is a living document and will be updated as more insights are gained.

## 1. Overall Structure

KNRCC appears to be a multi-pass compiler, traditionally consisting of:
*   **Pass 0 (c0*.c files):** Lexical analysis, parsing, semantic analysis, tree building, and intermediate code generation (likely to a temporary file or pipe).
*   **Pass 1 (c1*.c files):** Code generation from intermediate code to target assembly (PDP-11).
*   **Pass 2 (c2*.c files):** Object code improver / optimizer.
*   **cvopt.c:** A peephole optimizer, possibly used before or after Pass 2.
*   **cc.c:** The compiler driver that orchestrates these passes.

## 2. Inter-Pass Communication

Communication between passes is critical. This section documents known mechanisms.

### 2.1. Global Variables

Several global variables are used to maintain state across function calls within a pass and potentially to communicate information implicitly. Detailed mapping is ongoing. Key candidates identified so far from `c0.h` and usage:

*   **`line` (int):** Current line number being processed. Used for error messages. Defined in `c00.c`.
*   **`peeksym` (int):** Lookahead symbol for the parser. Defined in `c00.c`.
*   **`peekc` (int):** Lookahead character. Defined in `c0.h`, likely managed in `c00.c`.
*   **`eof` (int):** End-of-file flag. Defined in `c0.h`, managed in `c00.c`.
*   **`nerror` (int):** Count of errors. Defined in `c0.h` (used by `error()` in `c01.c` and `c11.c`).
*   **`funcbase`, `curbase`, `coremax` (char *):** Pointers for managing the bump-allocation arena (used by `getblk` in `c12.c` and `gblock` in `c01.c`). `curbase` is the current allocation pointer, `coremax` is the end of the allocated arena. `funcbase` seems to be a marker for memory allocated per function.
*   **`isn` (int):** Used for generating unique label numbers (e.g., in `c00.c` for string literals, `c10.c` for assembly labels).
*   **`sbufp` (FILE *):** Buffer for string literals, written to by `outcode` when `strflg` is set. Managed in `c00.c`.
*   **`strflg` (int):** Flag to direct `outcode` output to string buffer. Managed in `c00.c`.
*   **`proflg` (int):** Profiling flag. Set in `c00.c` (main).
*   **`csym` (struct hshtab *):** Current symbol table entry being processed. Managed in `c00.c` (lookup).
*   **`cval` (int), `lcval` (LTYPE):** Value of current constant token. Managed in `c00.c` (getnum).
*   **`mosflg` (int):** "Member Of Structure" flag during parsing. Managed in `c00.c`.
*   **`blklev` (int):** Current block nesting level. Managed in `c00.c`, `c02.c`, `c03.c`.
*   **`defsym` (struct hshtab *):** Symbol being defined. Used across `c00.c`, `c03.c`.
*   **`funcsym` (struct hshtab *):** Symbol for the current function being compiled. Used in `c00.c`, `c02.c`.
*   **`paraml`, `parame` (struct hshtab **):** Pointers for managing list of parameters during function declaration. Used in `c02.c`, `c03.c`.
*   **`nreg` (int):** Number of registers available/used in code generation (Pass 1). Defined in `c10.c`.
*   **`nstack` (int):** Current stack offset/depth for arguments. Used in `c10.c`.
*   **`nfloat` (int):** Flag indicating if floating point was used. Used in `c10.c`, `c11.c`.
*   **`opdope_pass0`, `opdope_pass1` (int[]):** Operator information tables for different passes.
*   **`sclass`, `typer` (int, struct hshtab):** These were identified as key communication variables for declaration processing in `c02.c`/`c03.c`, set by `getkeywords`. `sclass` holds storage class, `typer` holds type info. Careful handling of their scope and updates is needed.

### 2.2. Temporary Files / Pipes

*   The compiler driver `cc.c` explicitly manages temporary files (e.g., `/tmp/ctm0a...`, `tmp1`, `tmp2`, `tmp3`, `tmp4`, `tmp5`) to pass output from one pass to the input of the next.
    *   `cpp` (preprocessor) output goes to `tmp4` (or stdout if `pflag`).
    *   `c0` (Pass 0) reads `tmp4` (or stdin), writes intermediate code to `tmp1` (main code) and `tmp2` (strings).
    *   `c1` (Pass 1) reads `tmp1` then `tmp2`, writes assembly to `tmp3` (or `tmp5` if `oflag`).
    *   `c2` (Pass 2, optimizer) reads `tmp5` (if `oflag`), writes to `tmp3`.
*   The `outcode()` function in `c04.c` writes a specific binary format to `stdout` (which is redirected to `tmp1` or `tmp2` by `cc.c`). The `getree()` function in `c11.c` reads this format. This is a critical inter-pass communication channel.

#### Intermediate Code Format (`outcode()` to `getree()`)

The primary mechanism for passing structured data (expression trees, intermediate code instructions) from Pass 0 (specifically `c04.c` via `treeout` calling `outcode`) to Pass 1 (`c11.c` via `getree` calling `geti`) is a custom binary stream written to one of the temporary files (`tmp1` or `tmp2`).

*   **`outcode(const char *format, ...)` Function (`c04.c`):**
    *   This variadic function is the core serializer. The `format` string dictates what and how data is written.
    *   It writes data byte by byte, often in pairs to form 16-bit integers (PDP-11 style, little-endian).
    *   **Format Codes Observed:**
        *   `'B'`: Expects an `int` argument (operator code/type). Writes this `int` as a single byte, followed by a marker byte `0376` (octal). This `0376` in the high byte of a 16-bit word acts as a "beginning of item" or "operator" marker for `getree`.
            *   `putc(va_arg(ap, int), bufp); putc(0376, bufp);`
        *   `'N'`: Expects an `int` argument. Writes this `int` as two bytes (lower byte then upper byte, effectively little-endian for a 16-bit word).
            *   `temp_int = va_arg(ap, int); putc(temp_int & 0xFF, bufp); putc((temp_int >> 8) & 0xFF, bufp);`
            *   **FIXME:** If `LTYPE` (long) values are passed via 'N' (e.g., for `LCON` in `treeout`), this will truncate `LTYPE` to `int`, only writing its lower 16 bits. This needs reconciliation.
        *   `'S'`: Expects a `char *` argument (symbol name). Writes `_` if the string is not empty, then up to `NCPS` characters from the string (padded with nulls if shorter, truncated if longer), followed by a final null terminator for the entire string segment written to the stream. The `outname()` function in `c11.c` reads exactly `NCPS` bytes and then a null terminator.
            *   `np = va_arg(ap, char *); if (np && *np) putc('_', bufp); /* ... loop to write NCPS chars ... */ putc(0, bufp);` (Simplified logic shown).
        *   `'F'`: Expects a `char *` argument (string representation of a float/double). Writes characters from the string until a null terminator is found in the source string, then writes a null terminator to the stream.
            *   `np = va_arg(ap, char *); while (n-- > 0 && *np) putc(*np++ & 0177, bufp); putc(0, bufp);`
        *   `'1'`: Writes bytes `1` then `0`. No argument consumed.
        *   `'0'`: Writes bytes `0` then `0`. No argument consumed.
    *   The stream is thus a sequence of these (marker, data) pairs or (string, nullterm) sequences.

*   **`getree()` Function (`c11.c`):**
    *   This function is the deserializer, reconstructing the expression tree and other directives in Pass 1.
    *   It calls `geti()` to read 16-bit integers from the stream. `geti()` reads two bytes and combines them: `i = getchar(); i += getchar()<<8;` (little-endian).
    *   The main loop in `getree()` reads an `op_word` using `geti()`. It expects this `op_word` to have the `0376` marker in its upper byte if it's an operator/directive from `outcode`'s 'B' format: `if ((op_word & 0177400) != 0177000) error("Intermediate file error");`. The actual operator is then extracted: `op = op_word & 0377;`.
    *   A large `switch(op)` statement then handles each specific code:
        *   Cases like `NAME`, `CON`, `LCON`, `FCON`, `STRASG` read further arguments using `geti()` (for integers like type, offset, label number, value) or `outname()` (for symbol names, which reads `NCPS`+null).
        *   `outname(s)` in `c11.c` reads characters into `s` until a null is encountered or `NCPS` limit is hit, then null-terminates `s`. This matches how 'S' strings (symbol names) are written by `outcode`.
        *   The arguments read are used to populate new tree nodes (e.g., `struct tname`, `struct lconst`, `struct ftconst`, `struct fasgn`) allocated via `getblk()`.
        *   Operators that are part of expressions (e.g., `PLUS`, `STAR`, unary operators) are processed, often by pushing operands onto an expression stack (`expstack`) and then creating new nodes that combine them.
    *   The `getree()` function effectively reconstructs Pass 0's expression trees and directives by interpreting this custom binary stream.

This custom binary stream is a compact and efficient, though fragile, method of inter-pass communication, characteristic of early compilers. Any changes to `outcode` must be perfectly mirrored in `getree`.

## 3. Symbol Table Architecture (Preliminary)

The symbol table is primarily managed through `struct hshtab` (defined in `c0.h`).

*   **`struct hshtab` Members:**
    *   `hclass` (char): Storage class (e.g., `AUTO`, `EXTERN`, `STATIC`, `MOS`).
    *   `hflag` (char): Flags like `FKEYW` (keyword), `FMOS` (member of struct), `FFIELD` (bit-field), `FINIT` (initialized), `FLABL` (label).
    *   `htype` (int): Encoded type of the symbol (using the scheme in `knrcc_type_system.md`).
    *   `hsubsp` (int *): Pointer to subscript/dimension info (for arrays).
    *   `hstrp` (struct str *): Pointer to structure/union/enum description (via `struct str`). For bit-fields, this seems to be repurposed to point to a `struct field` like structure (details TBD).
    *   `hoffset` (int): Offset in memory (e.g., stack offset for autos, label number for statics).
    *   `hpdown` (struct phshtab *): Pointer to a pushed-down version of this symbol from an outer block (for handling nested scopes).
    *   `hblklev` (char): Block nesting level where the symbol was defined.
    *   `name[NCPS]` (char[]): Symbol name (8 characters significant).

*   **`struct phshtab` (Pushed-down hshtab):**
    *   A subset of `hshtab` used for saving/restoring symbols when entering/leaving blocks. Defined in `c0.h`.

*   **`h`-prefixed members vs. non-prefixed (e.g. `tnode->type` vs `hshtab->htype`):**
    *   The `h` prefix appears to consistently denote members belonging to `struct hshtab` (the symbol table entry itself).
    *   Tree nodes (`struct tnode`) have their own `type` member, which stores the encoded type of the expression or node.
    *   When `tnode->op == NAME`, `tnode->tr1` often points to a `struct hshtab` entry. Code like `p->tr1->hclass` (where `p` is `tnode*`) accesses symbol table info. This implies a cast: `((struct hshtab *)(p->tr1))->hclass`. This "merged" view of AST nodes and symbol table entries for NAMEs is a key architectural point.

*   **Hashing:**
    *   A simple hash function based on summing character values is used in `lookup()` (`c00.c`) to index into `hshtab[HSHSIZ]`.
    *   Keywords are specially marked in the hash table to be checked first.


### 3.1. `struct hshtab` - Main Symbol Table Entry

Defined in `c0.h`, this structure holds information for each symbol (variables, functions, keywords, tags, members).

```c
struct hshtab {
	char	hclass;		/* storage class (e.g., AUTO, EXTERN, STATIC, MOS, TYPEDEF) */
	char	hflag;		/* various flags (e.g., FKEYW, FMOS, FFIELD, FINIT, FLABL) */
	int	htype;		/* encoded type of the symbol (see knrcc_type_system.md) */
	int	*hsubsp;	/* subscript list (pointer to array dimensions if symbol is an array) */
	struct	str *hstrp;	/* structure description (pointer to struct str if symbol is a struct/union/enum tag, or details for a struct/union member if FFIELD is set) */
	int	hoffset;	/* post-allocation location (e.g., stack offset, label number for statics, enum constant value) */
	struct	phshtab *hpdown;/* Pushed-down name in outer block (for handling nested scopes and redeclarations) */
	char	hblklev;	/* Block nesting level where the symbol was defined (0 for global) */
	char	name[NCPS];	/* ASCII name of the symbol (NCPS, typically 8, characters significant) */
};
```

**Member Explanations:**

*   **`hclass` (char):** Storage class of the symbol. Uses defines like `AUTO`, `EXTERN`, `STATIC`, `REG`, `TYPEDEF`, `MOS` (Member Of Structure/Union), `STRTAG` (Structure Tag), `ENUMCON` (Enumeration Constant), etc.
*   **`hflag` (char):** A bitmask of flags providing additional information:
    *   `FKEYW` (04): Indicates the symbol table slot is pre-filled by a keyword.
    *   `FMOS` (01): Indicates a "Member Of Structure" symbol.
    *   `FFIELD` (020): Indicates the symbol is a bit-field. If set, `hstrp` likely points to a `struct field` like structure (or repurposes `struct str`) to store bit offset and length.
    *   `FINIT` (040): Indicates the symbol has been initialized.
    *   `FLABL` (0100): Indicates the symbol is a label.
*   **`htype` (int):** The core type information, encoded as an integer according to the scheme detailed in `docs/knrcc_type_system.md`. This includes base type and modifiers (pointer, function, array).
*   **`hsubsp` (int *):** If the symbol is an array, this points to an array of integers holding the dimensions. `hsubsp[0]` would be the size of the first dimension, etc.
*   **`hstrp` (struct str *):**
    *   If the symbol is a `struct`, `union`, or `enum` tag (e.g., `hclass == STRTAG` or `ENUMTAG`), `hstrp` points to a `struct str` instance that contains the size (`ssize`) and member list (`memlist`) for that aggregate type.
    *   If the symbol is a bit-field (`hflag & FFIELD`), `hstrp` is repurposed. The K&R code `dsym->hstrp = gblock(sizeof(*fldp));` in `c03.c` (where `fldp` would be `struct field *`) and subsequent access like `dsym->hstrp->bitoffs` and `dsym->hstrp->flen` suggests `hstrp` then points to a memory block compatible with `struct field { int flen; int bitoffs; }` (defined in `c0.h`).
*   **`hoffset` (int):** The meaning depends on `hclass`:
    *   For `AUTO` variables: Stack offset relative to the frame pointer.
    *   For `STATIC` variables and labels: A unique `isn` label number assigned for assembly output.
    *   For `ENUMCON`: The integer value of the enumeration constant.
    *   For `MOS`: The byte offset of the member within the structure/union.
*   **`hpdown` (struct phshtab *):** When a symbol in an outer scope is re-declared in an inner scope, the original `hshtab` entry's relevant details are pushed down (copied) into a `phshtab` structure, and `hpdown` points to it. This forms a linked list allowing the original symbol to be restored when the inner scope exits.
*   **`hblklev` (char):** The lexical block nesting level at which this symbol was defined. Level 0 is global.
*   **`name[NCPS]` (char[8]):** The identifier itself, padded or truncated to `NCPS` characters.

### 3.2. `struct phshtab` - Pushed-Down Symbol Table Entry

Defined in `c0.h`, this structure is used to save the state of a symbol from an outer scope when it's shadowed by a redeclaration in an inner scope. It's a subset of `struct hshtab`.

```c
struct phshtab {
	char	hclass;
	char	hflag;
	int	htype;
	int	*hsubsp;
	struct	str *hstrp;
	int	hoffset;
	struct	phshtab *hpdown; // Allows chaining multiple push-downs for the same original symbol slot
	char	hblklev;
};
```
This structure essentially holds all the semantic information of an `hshtab` entry except for the `name` itself, as it's associated with an existing named slot in the main `hshtab` array. The `hpdown` member in `phshtab` allows for further push-downs if a symbol is re-shadowed in even deeper nested blocks.

### 3.3. `struct str` - Structure/Union/Enum Description

Defined in `c0.h`.

```c
struct str {
	int	ssize;			/* structure size */
	struct hshtab 	**memlist;	/* member list (array of pointers to hshtab entries for members) */
};
```
*   **`ssize` (int):** Total size of the structure/union in bytes.
*   **`memlist` (struct hshtab **):** Pointer to an array of pointers. Each element in this array points to the `hshtab` entry for a member of the structure/union. This list is terminated by a NULL pointer. For enums, this might be used differently or `ssize` would be 0.

### 3.4. `struct field` - Bit-Field Description

Defined in `c0.h`. Used when `hshtab->hflag & FFIELD` is true. The `hshtab->hstrp` then points to memory interpreted as this structure.

```c
struct field {
	int	flen;		/* field width in bits */
	int	bitoffs;	/* shift count (bit offset from the start of the machine word) */
};
```
*   **`flen` (int):** The width of the bit-field in bits.
*   **`bitoffs` (int):** The bit offset of the field from the beginning of the underlying integer type.

### 3.5. Hashing Mechanism

The primary symbol table (`hshtab` array in `c0.h`, size `HSHSIZ`) is accessed as a hash table.

*   **Location:** The hashing logic is implemented in the `static int lookup(void)` function in `c/c00.c`.
*   **Algorithm:**
    1.  The input symbol is assumed to be in the global `symbuf` (char array of `NCPS`+2 size).
    2.  An initial hash value (`ihash`) is calculated by summing the ASCII values of the characters in `symbuf` (up to `NCPS` characters, ANDed with `0177` to ensure 7-bit ASCII).
        ```c
        ihash = 0;
        sp = symbuf;
        while (sp<symbuf+NCPS)
            ihash += *sp++&0177;
        ```
    3.  The initial table index is `rp = &hshtab[ihash % HSHSIZ];`.
    4.  **Keyword Check:** If the `hflag` of the entry at this initial index has the `FKEYW` bit set, `findkw()` is called to check if `symbuf` matches a keyword. If so, `KEYW` is returned.
    5.  **Linear Probing:** If not a keyword or no keyword match, the table is searched via linear probing starting from `rp`:
        *   It compares `symbuf` with `rp->name`.
        *   It also checks `mossym != (rp->hflag&FMOS)` to differentiate Member-Of-Structure symbols from regular symbols even if they have the same name.
        *   If no match, it increments `rp` (`if (++rp >= &hshtab[HSHSIZ]) rp = hshtab;`), wrapping around the table until an empty slot (`*(np = rp->name)` is false, i.e. `rp->name[0] == ' '`) is found or the symbol is located.
    6.  **Insertion:** If an empty slot is found before the symbol is located, the new symbol from `symbuf` is inserted into this slot. `hshused` is incremented. If `hshused >= HSHSIZ`, a "Symbol table overflow" error occurs.
*   **`FMOS` Flag:** The `mossym` global flag (Member Of Structure symbol) plays a role in hashing/lookup, allowing members of different structures to have the same name as global symbols or members of other structures.

### 3.6. Scope Management (Block Structure)

KNRCC handles nested block scopes using the `hblklev` (block level) member in `struct hshtab` and the `hpdown` (pushed-down symbol) chain.

*   **`blklev` (global int):** Tracks the current nesting depth of blocks. 0 is global. Incremented by `blockhead()` (`c02.c`), decremented by `blkend()` (`c02.c`).
*   **`hshtab[i].hblklev` (char):** Stores the block level at which the symbol in this hash slot was defined.
*   **`pushdecl(struct phshtab *asp)` Function (`c03.c`):**
    *   Called when a name is redeclared in an inner scope if that name already exists in an outer scope (i.e., `dsym->hblklev < blklev`).
    *   It allocates a `struct phshtab` using `gblock()`.
    *   Copies the semantic information (class, flags, type, offset, pointers, outer block level) from the existing `hshtab` entry (`sp`) into the new `phshtab` entry (`nsp`).
    *   The original `hshtab` entry (`sp`) is then reset for the new declaration (class and type cleared, `hblklev` set to current `blklev`).
    *   `sp->hpdown` is set to point to the newly allocated `phshtab` (`nsp`), thus saving the state of the shadowed outer-scope symbol.
*   **`blkend()` Function (`c02.c`):**
    *   Called when exiting a block. It decrements `blklev`.
    *   Iterates through the `hshtab`. For symbols defined at a level greater than the new (decremented) `blklev`:
        *   If the symbol has an `hpdown` pointer (meaning it shadowed an outer symbol), the pushed-down symbol's information (from `struct phshtab`) is copied back into the `hshtab` entry, effectively restoring the outer scope's symbol. The `FKEYW` flag is preserved.
        *   If there was no `hpdown` (i.e., it was a new symbol local to the exiting block), its `name[0]` is set to ` ` (marking the slot as empty), `hshused` is decremented, and `FKEYW` flag is cleared.
    *   This mechanism correctly handles shadowing and ensures symbols local to a block are removed from visibility when the block is exited.
*   **Symbol Retention:** Symbols declared `EXTERN` at `blklev > 0` or labels (`FLABL`) are not simply cleared but have their `hblklev` adjusted or are rehashed if necessary to maintain their linkage or definition across scopes as per C rules.
*   **Scope Management:**
    *   `blklev` tracks nesting.
    *   `pushdecl()` and `blkend()` in `c03.c` and `c02.c` manage symbol visibility and lifetime across scopes, using the `hpdown` chain.

## 4. Abstract Syntax Tree (AST) Node Architecture (Preliminary)

*   **`struct tnode` (defined in `c0.h` and `c1.h` with variations):**
    *   The primary structure for expression tree nodes.
    *   `op` (int): Operator (e.g., `PLUS`, `STAR`, `NAME`, `CON`).
    *   `type` (int): Encoded type of the node/expression result.
    *   `tr1`, `tr2` (struct tnode *): Left and right children for binary operators; `tr1` for unary.
    *   Pass 0 version (`c0.h`): includes `subsp` (array subscripts) and `strp` (struct info).
    *   Pass 1 version (`c1.h`): includes `degree` (likely for code generation complexity/register needs). This difference suggests nodes might be transformed or re-interpreted between passes.

*   **Specialized Node Structures (sharing common initial members with `tnode` or used via casting):**
    *   `struct cnode` (for constants)
    *   `struct lnode` (for long constants)
    *   `struct fnode` (for float/double constants)
    *   `struct tname` (in `c1.h`, for local names in Pass 1, includes `class`, `regno`, `offset`)
    *   `struct fasgn` (in `c1.h`, for field assignments)

*   **`tnode->op == NAME`:** As noted, when `op` is `NAME`, `tr1` points to a `struct hshtab` entry. `tr2` is often NULL in this case.

*   **`block()` function (`c01.c`):** The primary constructor for `tnode` instances in Pass 0.
*   **`tnode()` function (`c12.c`):** A constructor for `tnode` instances in Pass 1, using `getblk`.

This section details the structures used to build the Abstract Syntax Tree (AST) and other expression tree components.

### 4.1. `struct tnode` - Primary AST Node

This is the main structure for representing operations and expressions in the tree. Its definition varies slightly between Pass 0 (`c0.h`) and Pass 1 (`c1.h`), indicating a transformation or re-interpretation of nodes as they pass through the compiler.

**Pass 0 Definition (`c0.h`):**
```c
struct tnode {
	int	op;		/* operator (e.g., PLUS, STAR, NAME, CON) */
	int	type;		/* encoded type of the node/expression result */
	int	*subsp;		/* subscript list (pointer to array dimensions if node represents an array access or definition) */
	struct	str *strp;	/* structure description (pointer to struct str if node involves a struct/union) */
	struct	tnode *tr1;	/* left operand or single operand for unary ops */
	struct	tnode *tr2;	/* right operand for binary ops */
};
```
*   **`op` (int):** The operator or node kind (e.g., `PLUS`, `NAME`, `CON`). Defines how other fields are interpreted.
*   **`type` (int):** The KNRCC integer-encoded type of the expression represented by this node.
*   **`subsp` (int *):** Primarily used when `tnode->type` indicates an `ARRAY`. It points to a dynamically allocated block of integers (allocated by `gblock` in `c03.c` during declaration processing, e.g., in `decl1`). This block stores the dimensions of the array. For example, `subsp[0]` would be the size of the first dimension. Functions like `length()` in `c04.c` use `subsp` to calculate array sizes. For non-array type nodes, this is typically `NULL`.
*   **`strp` (struct str *):** Used when `tnode->type` indicates a `STRUCT` or `UNION`. It points to a `struct str` instance (see Symbol Table section for `struct str` definition) which contains the total size of the struct/union (`ssize`) and a pointer to its member list (`memlist`). This allows type-checking and code generation for member access (e.g., with `.` or `->` operators, handled in `build()` in `c01.c`). For non-struct/union type nodes, this is typically `NULL`. It's also repurposed for bit-field information via `struct field` if the `FFIELD` flag is set on the corresponding symbol table entry.
*   **`tr1`, `tr2` (struct tnode *):** Pointers to child nodes. For unary operators, `tr2` is often `NULL`. For leaf nodes like `NAME` or `CON`, their interpretation is special (see below).

**Pass 1 Definition (`c1.h`):**
```c
struct tnode {
	int	op;
	int	type;
	int	degree;		/* Node complexity for register allocation or code generation strategy */
	struct	tnode *tr1, *tr2;
};
```
*   **`degree` (int):** This field replaces `subsp` and `strp` from the Pass 0 `tnode` definition. It is computed in Pass 1, primarily by the `degree()` function in `c/c11.c` (and `dcalc()`). The `degree` seems to represent a heuristic for the complexity or "weight" of an expression subtree, influencing register allocation and instruction selection during code generation (`c/c10.c`).
    *   Leaf nodes like `CON`, `NAME`, `AMPER` get specific small degree values.
    *   Operator nodes' degrees are often calculated based on the degrees of their children and the type of operation (e.g., `d1==d2 ? d1+islong(tree->type) : max(d1, d2)` in `optim()` in `c12.c`).
    *   A higher degree might indicate a subexpression that is more complex or would benefit from being computed into a register. For example, `dcalc()` returns 20 or 24 if a node's degree exceeds available registers (`nrleft`), suggesting it should be stored.
    *   The `optim()` function in `c12.c` extensively uses `degree` to reorder expressions and make optimization choices.

**`tnode->op == NAME` Interaction with Symbol Table:**
A crucial aspect of KNRCC's architecture is how `NAME` nodes in the AST link to the symbol table.
*   When `tnode->op == NAME`, the `tr1` field does **not** point to another `struct tnode`. Instead, it is a cast pointer to the corresponding `struct hshtab` entry for that identifier.
*   `tr2` is typically `NULL` in this case.
*   Code accessing symbol information from a `NAME` node would look like: `struct hshtab *symbol_entry = (struct hshtab *)(the_name_tnode->tr1); char storage_class = symbol_entry->hclass;`
*   This design merges the AST representation of an identifier directly with its semantic information from the symbol table, likely for efficiency and to reduce indirections in a memory-constrained environment. The `nblock()` function in `c01.c` exemplifies creating such a NAME node: `return(block(NAME, ds->htype, ds->hsubsp, ds->hstrp, (struct tnode *)ds, NULL));` where `ds` is `struct hshtab *`.

### 4.2. Specialized Leaf Node Structures

These structures are used for specific leaf types in the expression tree. They often share the initial `op` and `type` members with `struct tnode`, allowing them to be cast to `struct tnode *` when manipulated generally in the tree, but their specific members are accessed when their `op` indicates their kind. They are typically allocated via `gblock()` (Pass 0) or `getblk()` (Pass 1) and then cast.

**`struct cnode` - Constant Node (`c0.h`)**
For integer constants.
```c
struct cnode {
	int	op;     // Typically CON
	int	type;   // Usually INT or UNSIGN
	int	*subsp; // NULL for simple constants
	struct	str *strp;  // NULL for simple constants
	int	value;  // The integer value of the constant
};
```
*   Created by `cblock()` in `c01.c`.

**`struct lnode` - Long Constant Node (`c0.h`)**
For long integer constants.
```c
struct lnode {
	int	op;     // Typically LCON
	int	type;   // LONG
	int	*subsp;
	struct	str *strp;
	LTYPE	lvalue; // The long value (LTYPE is a typedef, usually long)
};
```

**`struct fnode` - Floating Point Constant Node (`c0.h`)**
For float/double constants. The actual value is stored as a string initially.
```c
struct fnode {
	int	op;     // Typically FCON (or SFCON for "short float")
	int	type;   // FLOAT or DOUBLE
	int	*subsp;
	struct	str *strp;
	char	*cstr;  // String representation of the float/double (e.g., "3.14")
};
```
*   Created by `fblock()` in `c01.c`. The `cstr` is converted to `double` by `atof()` later, e.g., in `getree()` (`c11.c`).
*   `SFCON` (short float constant) appears to be an optimized representation where the float can be held in an int, seen in `optim()` (`c12.c`) and `pname()` (`c11.c`). `struct ftconst` in `c1.h` seems related to this, holding both `value` (int) and `fvalue` (double).

**`struct tname` - Local Name Node (Pass 1 - `c1.h`)**
Represents local variables or parameters, primarily in Pass 1.
```c
struct tname {
	int	op;     // Typically NAME
	int	type;
	char	class;  // Storage class (e.g., AUTO, REG, OFFS)
	char	regno;  // Assigned register number if class is REG or OFFS
	int	offset; // Stack offset or NULOC for registers
	int	nloc;   // Name location / identifier from symbol table (often an index or label no)
};
```
*   Shares `op` and `type` with `tnode`, allowing casts for common access.

**`struct xtname` - External Name Node (Pass 1 - `c1.h`)**
Similar to `tname` but specifically for externals, includes the name directly.
```c
struct xtname {
	int	op;
	int	type;
	char	class;  // Usually EXTERN or STATIC
	char	regno;
	int	offset; // Often a label number generated by Pass 0
	char	name[NCPS]; // The external name
};
```

**`struct fasgn` - Field Assignment Node (Pass 1 - `c1.h`)**
Used for bit-field assignments.
```c
struct fasgn {
	int	op;     // Typically FSELA (field select assign) or STRASG (if struct assign involves fields)
	int	type;
	int	degree;
	struct	tnode *tr1, *tr2; // Operands of the assignment
	int	mask;   // Bitmask for the field
};
```

These specialized structures illustrate how KNRCC handles different kinds of terminal symbols in its expression trees, often relying on the `op` field to determine how to interpret the rest of the node's data.
---
This initial structure will be expanded with more details as the investigation and modernization of KNRCC progresses, particularly from Step 8 onwards (Resolve `c00.c` `tree()` Parser Issue) and during the refactoring of semantic analysis in Pass 0.
