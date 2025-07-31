#
/*
 * C compiler
 *
 *
 */

#include "c0.h"

// Static forward declarations for c04.c
static void treeout(struct tnode *atp, int isstruct);

/*
 * Reduce the degree-of-reference by one.
 * e.g. turn "ptr-to-int" into "int".
 */
int decref(int at)
{
	register int t; // K&R t was implicitly int

	t = at;
	if ((t & ~TYPE) == 0) {
		error("Illegal indirection");
		return(t);
	}
	return (((t >> TYLEN) & ~TYPE) | (t & TYPE)); // Added parentheses
}

/*
 * Increase the degree of reference by
 * one; e.g. turn "int" to "ptr-to-int".
 */
int incref(int t)
{
	return(((t&~TYPE)<<TYLEN) | (t&TYPE) | PTR);
}

/*
 * Make a tree that causes a branch to lbl
 * if the tree's value is non-zero together with the cond.
 */
void cbranch(struct tnode *tree, int label_num, int cond)
{
	treeout(tree, 0);
	outcode("BNNN", CBRANCH, label_num, cond, line);
}

/*
 * Write out a tree.
 */
void rcexpr(struct tnode *tree)
{
	register struct tnode *tp;

	/*
	 * Special optimization
	 */
	if ((tp=tree)->op==INIT && tp->tr1->op==CON) {
		if (tp->type==CHAR) {
			outcode("B1N0", BDATA, ((struct cnode *)tp->tr1)->value); // Cast to cnode
			return;
		} else if (tp->type==INT || tp->type==UNSIGN) {
			outcode("BN", SINIT, ((struct cnode *)tp->tr1)->value); // Cast to cnode
			return;
		}
	}
	treeout(tp, 0);
	outcode("BN", EXPR, line);
}

static void treeout(struct tnode *atp, int isstruct)
{
	register struct tnode *tp;
	register struct hshtab *hp;
	register int nextisstruct;

	if ((tp = atp) == 0) {
		outcode("B", NULLOP);
		return;
	}
	nextisstruct = tp->type==STRUCT;
	switch(tp->op) {

	case NAME:
		hp = (struct hshtab *)tp->tr1; // Cast to hshtab*
		if (hp->hclass==TYPEDEF)
			error("Illegal use of type name");
		outcode("BNN", NAME, hp->hclass==0?STATIC:hp->hclass, tp->type);
		if (hp->hclass==EXTERN)
			outcode("S", hp->name);
		else
			outcode("N", hp->hoffset);
		break;

	case LCON:
		outcode("BNNN", tp->op, tp->type, ((struct lnode *)tp)->lvalue); // Cast to lnode
		break;

	case CON:
		outcode("BNN", tp->op, tp->type, ((struct cnode *)tp)->value); // Cast to cnode
		break;

	case FCON:
		outcode("BNF", tp->op, tp->type, ((struct fnode *)tp)->cstr); // Cast to fnode
		break;

	case STRING:
		// tp->tr1 for STRING op in treeout is already 'int label_for_string_literal'
		// It's not a tnode itself that needs casting for a member.
		outcode("BNNN", NAME, STATIC, tp->type, (int)(long)tp->tr1); // Ensure it's passed as int
		break;

	case FSEL:
		treeout(tp->tr1, nextisstruct);
		// Assuming tp->strp points to a struct field for FSEL, based on how block() creates it
		// and how FFIELD is used in c03.c
		if (tp->strp) { // Should always be true for FSEL
		    outcode("BNNN",tp->op,tp->type, ((struct field *)tp->strp)->bitoffs, ((struct field *)tp->strp)->flen);
		} else {
		    error("FSEL node has no field description"); // Should not happen
		    // Handle error or pass dummy values if necessary
		    outcode("BNNN",tp->op,tp->type, 0, 0);
		}
		break;

	case ETYPE:
		error("Illegal use of type");
		break;

	case AMPER:
		treeout(tp->tr1, 1);
		outcode("BN", tp->op, tp->type);
		break;


	case CALL:
		treeout(tp->tr1, 1);
		treeout(tp->tr2, 0);
		outcode("BN", CALL, tp->type);
		break;

	default:
		treeout(tp->tr1, nextisstruct);
		if (opdope_pass0[tp->op]&BINARY) // Changed opdope to opdope_pass0
			treeout(tp->tr2, nextisstruct);
		outcode("BN", tp->op, tp->type);
		break;
	}
	if (nextisstruct && isstruct==0)
		outcode("BNN", STRASG, STRUCT, tp->strp->ssize);
}

/*
 * Generate a branch
 */
void branch(int label_num)
{
	outcode("BN", BRANCH, label_num);
}

/*
 * Generate a label
 */
void label(int label_num)
{
	outcode("BN", LABEL, label_num);
}

/*
 * ap is a tree node whose type
 * is some kind of pointer; return the size of the object
 * to which the pointer points.
 */
int plength(struct tnode *ap) // Changed parameter to struct tnode *ap
{
	register int t, l; // K&R t, l were implicitly int
	register struct tnode *p;

	p = ap; // No cast needed if ap is already tnode*
	if (p==0 || ((t=p->type)&~TYPE) == 0)		/* not a reference */
		return(1);
	p->type = decref(t);
	l = length(p);
	p->type = t;
	return(l);
}

/*
 * return the number of bytes in the object
 * whose tree node is acs.
 */
int length(struct tnode *acs)
{
	register int t, elsz; // K&R t, elsz were implicitly int
	long n;
	register struct tnode *cs;
	int nd;

	cs = acs;
	t = cs->type;
	n = 1;
	nd = 0;
	while ((t&XTYPE) == ARRAY) {
		t = decref(t);
		n *= cs->subsp[nd++]; // Corrected K&R n =* ...
	}
	if ((t&~TYPE)==FUNC)
		return(0);
	if (t>=PTR)
		elsz = SZPTR;
	else switch(t&TYPE) {

	case INT:
	case UNSIGN:
		elsz = SZINT;
		break;

	case CHAR:
		elsz = 1;
		break;

	case FLOAT:
		elsz = SZFLOAT;
		break;

	case LONG:
		elsz = SZLONG;
		break;

	case DOUBLE:
		elsz = SZDOUB;
		break;

	case STRUCT:
		if ((elsz = cs->strp->ssize) == 0)
			error("Undefined structure");
		break;
	default:
		error("Compiler error (length)");
		return(0);
	}
	n *= elsz;
	if (n >= (unsigned)50000) {
		error("Warning: very large data structure");
		nerror--;
	}
	return(n);
}

/*
 * The number of bytes in an object, rounded up to a word.
 */
int rlength(struct tnode *cs)
{
	return((length(cs)+ALIGN) & ~ALIGN);
}

/*
 * After an "if (...) goto", look to see if the transfer
 * is to a simple label.
 */
int simplegoto(void)
{
	register struct hshtab *csp;

	if ((peeksym=symbol())==NAME && nextchar()==';') {
		csp = csym;
		if (csp->hblklev == 0)
			pushdecl((struct phshtab *)csp); // Cast csp
		if (csp->hclass==0 && csp->htype==0) {
			csp->htype = ARRAY;
			csp->hflag |= FLABL; // Changed =| to |=
			if (csp->hoffset==0)
				csp->hoffset = isn++;
		}
		if ((csp->hclass==0||csp->hclass==STATIC)
		 &&  csp->htype==ARRAY) {
			peeksym = -1;
			return(csp->hoffset);
		}
	}
	return(0);
}

/*
 * Return the next non-white-space character
 */
int nextchar(void)
{
	while (spnextchar()==' ')
		peekc = 0;
	return(peekc);
}

/*
 * Return the next character, translating all white space
 * to blank and handling line-ends.
 */
int spnextchar(void)
{
	register int c;

	if ((c = peekc)==0)
		c = getchar();
	if (c=='\t' || c=='\014')	/* FF */
		c = ' ';
	else if (c=='\n') {
		c = ' ';
		if (inhdr==0)
			line++;
		inhdr = 0;
	} else if (c=='\001') {	/* SOH, insert marker */
		inhdr++;
		c = ' ';
	}
	peekc = c;
	return(c);
}

/*
 * is a break or continue legal?
 */
void chconbrk(int label_num)
{
	if (label_num==0)
		error("Break/continue error");
}

/*
 * The goto statement.
 */
void dogoto(void)
{
	register struct tnode *np;

	*cp++ = tree();
	build(STAR);
	chkw(np = *--cp, -1);
	rcexpr(block(JUMP,0,NULL,NULL,np, NULL)); // Added NULL
}

/*
 * The return statement, which has to convert
 * the returned object to the function's type.
 */
void doret(void)
{
	register struct tnode *t;

	if (nextchar() != ';') {
		t = tree();
		*cp++ = &funcblk;
		*cp++ = t;
		build(ASSIGN);
		cp[-1] = cp[-1]->tr2;
		if (funcblk.type==CHAR)
			cp[-1] = block(ITOC, INT, NULL, NULL, cp[-1], NULL); // Added NULL
		build(RFORCE);
		rcexpr(*--cp);
	}
	branch(retlab);
}

/*
 * Write a character on the error output.
 */
/*
 * Coded output:
 *   B: beginning of line; an operator
 *   N: a number
 *   S: a symbol (external)
 *   1: number 1
 *   0: number 0
 */
void outcode(const char *s, ...) // Matches prototype in c0.h
{
	va_list ap;
	register FILE *bufp;
	int n;          // For string length limits
	char *np;       // For string pointers
	int temp_int;   // For 'B' and 'N' cases if they are truly int
    // LTYPE temp_long; // For 'N' case if it's from an LCON (tp->lvalue) - this is tricky

	bufp = stdout;
	if (strflg)
		bufp = sbufp;

	va_start(ap, s);
	for (;;) {
		switch(*s++) {
		case 'B': // Expects an int (operator code, usually char range but passed as int)
			temp_int = va_arg(ap, int);
			putc(temp_int, bufp);       // First byte (the operator itself)
			putc(0376, bufp);           // Second byte (marker)
			continue;

		case 'N': // Expects an int or LTYPE. The K&R code writes two bytes.
                  // For simplicity and common case (like line numbers, simple values), assume int for now.
                  // LCON case in treeout passes tp->lvalue, which is LTYPE (long).
                  // This needs careful handling. A simple cast to int for putc might truncate.
                  // The original putc(*ap, bufp); putc(*ap++>>8, bufp); implies it took an int and wrote its bytes.
                  // FIXME: This assumes 'int' for 'N'. If LTYPE (long) is passed, this will be incorrect.
			temp_int = va_arg(ap, int); // Assuming int for now. If LTYPE is passed, this is problematic.
			putc(temp_int & 0xFF, bufp);        // Lower byte
			putc((temp_int >> 8) & 0xFF, bufp); // Upper byte
			continue;

		case 'F': // Expects char * (from tp->cstr after FCON in treeout)
			n = 1000; // Default max length from original
			np = va_arg(ap, char *);
			goto str_out; // Common string output logic

		case 'S': // Expects char * (symbol name from hp->name or an empty string)
			n = NCPS; // Max length from original NCPS
			np = va_arg(ap, char *);
			if (np && *np) // Check if string is not null and not empty before putc('_')
				putc('_', bufp);
		str_out: // Label for common string output
			if (np) { // Ensure np is not NULL
				while (n-- > 0 && *np) { // Check n and *np
					putc(*np++ & 0177, bufp);
				}
			}
			putc(0, bufp); // Null terminator for the string segment
			continue;

		case '1':
			putc(1, bufp);
			putc(0, bufp);
			continue;

		case '0':
			putc(0, bufp);
			putc(0, bufp);
			continue;

		case '\0': // End of format string
			if (ferror(bufp)) {
				error("Write error on temp"); // error() is already variadic
				exit(1);
			}
			va_end(ap);
			return;

		default:
			// To handle the previous char if it wasn't a format specifier (e.g. literal char in format string)
			// This case should ideally not be hit if format strings are well-formed.
			// The original K&R didn't have this, it would have printed the char.
			// For safety, let's assume format strings are composed only of known codes.
			error("Botch in outcode format string");
			va_end(ap);
			exit(1); // Exit on unknown format code
		}
	}
}
