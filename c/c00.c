#
/* C compiler
 *
 *
 *
 * Called from cc:
 *   c0 source temp1 temp2 [ profileflag ]
 * temp1 gets most of the intermediate code;
 * strings are put on temp2, which c1 reads after temp1.
 */

#include "c0.h"
// Forward declarations for static functions in this file
/* static int lookup(void); // Now extern via c0.h */
static int findkw(void);
// int symbol(void); // Global, declared in c0.h
static int getnum(void);
static int subseq(int c, int a, int b);
/* static void putstr(int lab, int amax); // Now extern via c0.h, and definition below will be active */
static int getcc(void);
static int mapch(int ac);
// char *copnum(int len); // Global, declared in c0.h
// struct hshtab *xprtype(struct hshtab *atyb); // Global, declared in c0.h
// struct tnode *tree(void); // Global, declared in c0.h


int	isn = 1;
int	peeksym = -1;
int	line = 1;
struct	tnode	funcblk = { NAME, 0, NULL, NULL, NULL, NULL };

struct kwtab {
	char	*kwname;
	int	kwval;
} kwtab[] = {
	{ "int",	INT	},
	{ "char",	CHAR	},
	{ "float",	FLOAT	},
	{ "double",	DOUBLE	},
	{ "struct",	STRUCT	},
	{ "long",	LONG	},
	{ "unsigned",	UNSIGN	},
	{ "union",	UNION	},
	{ "short",	INT	},
	{ "auto",	AUTO	},
	{ "extern",	EXTERN	},
	{ "static",	STATIC	},
	{ "register",	REG	},
	{ "goto",	GOTO	},
	{ "return",	RETURN	},
	{ "if",		IF	},
	{ "while",	WHILE	},
	{ "else",	ELSE	},
	{ "switch",	SWITCH	},
	{ "case",	CASE	},
	{ "break",	BREAK	},
	{ "continue",	CONTIN },
	{ "do",		DO	},
	{ "default",	DEFAULT },
	{ "for",	FOR	},
	{ "sizeof",	SIZEOF	},
	{ "typedef",	TYPEDEF },
	{ "enum",	ENUM	},
	{ 0,		0	}
};

int main(int argc, char *argv[])
{
	register char *sp;
	register int i; // Added int
	register struct kwtab *ip;

	if(argc<4) {
		error("Arg count");
		exit(1);
	}
	if (freopen(argv[1], "r", stdin)==NULL) {
		error("Can't find %s", argv[1]);
		exit(1);
	}
	if (freopen(argv[2], "w", stdout)==NULL || (sbufp=fopen(argv[3],"w"))==NULL) {
		error("Can't create temp");
		exit(1);
	}
	setbuf(sbufp, sbuf);
	if (argc>4)
		proflg++;
	/*
	 * The hash table locations of the keywords
	 * are marked; if an identifier hashes to one of
	 * these locations, it is looked up in in the keyword
	 * table first.
	 */
	for (ip=kwtab; (sp = ip->kwname); ip++) {
		i = 0;
		while (*sp)
			i += *sp++; // Corrected =+ to +=
		hshtab[i%HSHSIZ].hflag = FKEYW;
	}
	coremax = funcbase = curbase = sbrk(0);
	while(!eof)
		extdef();
	outcode("B", EOFC);
	strflg++;
	outcode("B", EOFC);
	exit(nerror!=0);
}

/*
 * Look up the identifier in symbuf in the symbol table.
 * If it hashes to the same spot as a keyword, try the keyword table
 * first.  An initial "." is ignored in the hash.
 * Return is a ptr to the symbol table entry.
 */
int lookup(void) // No longer static
{
	int ihash;
	register struct hshtab *rp;
	register char *sp, *np;

	ihash = 0;
	sp = symbuf;
	while (sp<symbuf+NCPS)
		ihash += *sp++&0177; // Corrected =+ to +=
	rp = &hshtab[ihash%HSHSIZ];
	if (rp->hflag&FKEYW)
		if (findkw()) // findkw is static, prototype added
			return(KEYW);
	while (*(np = rp->name)) {
		for (sp=symbuf; sp<symbuf+NCPS;)
			if (*np++ != *sp++)
				goto no;
		if (mossym != (rp->hflag&FMOS))
			goto no;
		csym = rp;
		return(NAME);
	no:
		if (++rp >= &hshtab[HSHSIZ])
			rp = hshtab;
	}
	if(++hshused >= HSHSIZ) {
		error("Symbol table overflow"); // error will be handled by c0.h
		exit(1); // exit will be handled by c0.h (via stdlib.h)
	}
	rp->hclass = 0;
	rp->htype = 0;
	rp->hoffset = 0;
	rp->hsubsp = NULL; // Corrected subsp to hsubsp
	rp->hstrp = NULL;  // Corrected strp to hstrp
	rp->hpdown = NULL;
	rp->hblklev = blklev;
	rp->hflag |= mossym; // Corrected =| to |=
	sp = symbuf;
	for (np=rp->name; sp<symbuf+NCPS;)
		*np++ = *sp++;
	csym = rp;
	return(NAME);
}

/*
 * Search the keyword table.
 * Ignore initial "." to avoid member-of-structure
 * problems.
 */
static int findkw(void)
{
	register struct kwtab *kp;
	register char *p1, *p2;
	char *wp;
	int firstc;

	wp = symbuf;
	firstc = *wp;
	for (kp=kwtab; (p2 = kp->kwname); kp++) {
		p1 = wp;
		while (*p1 == *p2++)
			if (*p1++ == '\0') {
				cval = kp->kwval;
				return(1);
			}
	}
	*wp = firstc;
	return(0);
}


/*
 * Return the next symbol from the input.
 * peeksym is a pushed-back symbol, peekc is a pushed-back
 * character (after peeksym).
 * mosflg means that the next symbol, if an identifier,
 * is a member of structure or a structure tag, and it
 * gets a "." prepended to it to distinguish
 * it from other identifiers.
 */
int symbol(void) {
	register int c; // Added int
	register char *sp;
	register int tline; // Added int

	if (peeksym>=0) {
		c = peeksym;
		peeksym = -1;
		if (c==NAME)
			mosflg = 0;
		return(c);
	}
	if (peekc) {
		c = peekc;
		peekc = 0;
	} else
		if (eof)
			return(EOFC);
		else
			c = getchar();
loop:
	if (c==EOF) {
		eof++;
		return(EOFC);
	}
	switch(ctab[c]) {

	case SHARP:
		if ((c=symbol())!=CON) {
			error("Illegal #");
			return(c);
		}
		tline = cval;
		while (ctab[peekc]==SPACE)
			peekc = getchar();
		if (peekc=='"') {
			sp = filename;
			while ((c = mapch('"')) >= 0)
				*sp++ = c;
			*sp++ = 0;
			peekc = getchar();
		}
		if (peekc != '\n') {
			error("Illegal #");
			while (getchar()!='\n' && eof==0)
				;
		}
		peekc = 0;
		line = tline;
		return(symbol());

	case INSERT:		/* ignore newlines */
		inhdr = 1;
		c = getchar();
		goto loop;

	case NEWLN:
		if (!inhdr)
			line++;
		inhdr = 0;
		/* fallthrough */
	case SPACE:
		c = getchar();
		goto loop;

	case PLUS:
		return(subseq(c,PLUS,INCBEF));

	case MINUS:
		return(subseq(c,subseq('>',MINUS,ARROW),DECBEF));

	case ASSIGN:
		c = spnextchar();
		peekc = 0;
		if (c=='=')
			return(EQUAL);
		if (c==' ')
			return(ASSIGN);
		if (c=='<' || c=='>') {
			if (spnextchar() != c) {
				peeksym = ctab[c];
				return(ASSIGN);
			}
			peekc = 0;
			return(c=='<'? ASLSH: ASRSH);
		}
		if (ctab[c]>=PLUS && ctab[c]<=EXOR) {
			if (spnextchar() != ' '
			 && (c=='-' || c=='&' || c=='*')) {
				error("Warning: %c= operator assumed", c);
				nerror--;
			}
			c = ctab[c];
			return(c+ASPLUS-PLUS);
		}
		peekc = c;
		return(ASSIGN);

	case LESS:
		if (subseq(c,0,1)) return(LSHIFT);
		return(subseq('=',LESS,LESSEQ));

	case GREAT:
		if (subseq(c,0,1)) return(RSHIFT);
		return(subseq('=',GREAT,GREATEQ));

	case EXCLA:
		return(subseq('=',EXCLA,NEQUAL));

	case BSLASH:
		if (subseq('/', 0, 1))
			return(MAX);
		goto unkn;

	case DIVIDE:
		if (subseq('\\', 0, 1))
			return(MIN);
		if (subseq('*',1,0))
			return(DIVIDE);
		while ((c = spnextchar()) != EOFC) {
			peekc = 0;
			if (c=='*') {
				if (spnextchar() == '/') {
					peekc = 0;
					c = getchar();
					goto loop;
				}
			}
		}
		eof++;
		error("Nonterminated comment");
		return(0);

	case PERIOD:
	case DIGIT:
		peekc = c;
		return(getnum());

	case DQUOTE:
		cval = isn++;
		return(STRING);

	case SQUOTE:
		return(getcc());

	case LETTER:
		sp = symbuf;
		while(ctab[c]==LETTER || ctab[c]==DIGIT) {
			if (sp<symbuf+NCPS)
				*sp++ = c;
			c = getchar();
		}
		while(sp<symbuf+NCPS)
			*sp++ = '\0';
		mossym = 0;
		if (mosflg) {
			mossym = FMOS;
			mosflg = 0;
		}
		peekc = c;
		if ((c=lookup())==KEYW && cval==SIZEOF)
			c = SIZEOF;
		return(c);

	case AND:
		return(subseq('&', AND, LOGAND));

	case OR:
		return(subseq('|', OR, LOGOR));

	case UNKN:
	unkn:
		error("Unknown character");
		c = getchar();
		goto loop;

	}
	return(ctab[c]);
}

/*
 * Read a number.  Return kind.
 */
static int getnum(void)
{
	register char *np;
	register int c, base; // Added int
	int expseen, sym, ndigit;
	char *nsyn;
	int maxdigit;

	nsyn = "Number syntax";
	lcval = 0;
	base = 10;
	maxdigit = 0;
	np = numbuf;
	ndigit = 0;
	sym = CON;
	expseen = 0;
	if ((c=spnextchar()) == '0')
		base = 8;
	for (;; c = getchar()) {
		*np++ = c;
		if (ctab[c]==DIGIT || ((base==16) && (('a'<=c && c<='f') || ('A'<=c && c<='F')))) { // Fully parenthesized
			if (base==8)
				lcval <<= 3;
			else if (base==10)
				lcval = ((lcval<<2) + lcval)<<1;
			else
				lcval <<= 4; // Corrected =<<
			if (ctab[c]==DIGIT)
				c -= '0'; // Corrected =-
			else if (c>='a')
				c -= 'a'-10;
			else
				c -= 'A'-10;
			lcval += c; // Corrected =+ to +=
			ndigit++;
			if (c>maxdigit)
				maxdigit = c;
			continue;
		}
		if (c=='.') {
			if (base==16 || sym==FCON)
				error(nsyn);
			sym = FCON;
			base = 10;
			continue;
		}
		if (ndigit==0) {
			sym = DOT;
			break;
		}
		if ((c=='e'||c=='E') && expseen==0) {
			expseen++;
			sym = FCON;
			if (base==16 || maxdigit>=10)
				error(nsyn);
			base = 10;
			*np++ = c = getchar();
			if (c!='+' && c!='-' && ctab[c]!=DIGIT)
				break;
		} else if (c=='x' || c=='X') {
			if (base!=8 || lcval!=0 || sym!=CON)
				error(nsyn);
			base = 16;
		} else if ((c=='l' || c=='L') && sym==CON) {
			c = getchar();
			sym = LCON;
			break;
		} else
			break;
	}
	peekc = c;
	if (maxdigit >= base)
		error(nsyn);
	if (sym==FCON) {
		np[-1] = 0;
		cval = np-numbuf;
		return(FCON);
	}
	if (sym==CON && (lcval<0 || (lcval>MAXINT && base==10) || ((lcval>>1)>MAXINT))) { // Fully parenthesized
		sym = LCON;
	}
	cval = lcval;
	return(sym);
}

/*
 * If the next input character is c, return b and advance.
 * Otherwise push back the character and return a.
 */
static int subseq(int c, int a, int b)
{
	if (spnextchar() != c)
		return(a);
	peekc = 0;
	return(b);
}

/*
 * Write out a string, either in-line
 * or in the string temp file labelled by
 * lab.
 */
// #if 0 // Definition now active
void putstr(int lab, int amax) // No longer static
{
	register int c, max;

	nchstr = 0;
	max = amax;
	if (lab) {
		strflg++;
		outcode("BNB", LABEL, lab, BDATA);
		max = 10000;
	} else
		outcode("B", BDATA);
	while ((c = mapch('"')) >= 0) {
		if (nchstr < max) {
			nchstr++;
			if (nchstr%15 == 0)
				outcode("0B", BDATA);
			outcode("1N", c & 0377);
		}
	}
	if (nchstr < max) {
		nchstr++;
		outcode("10");
	}
	outcode("0");
	strflg = 0;
}
// #endif

/*
 * read a single-quoted character constant.
 * The routine is sensitive to the layout of
 * characters in a word.
 */
static int getcc(void)
{
	register int c; // Removed unused 'cc'
    char cval_buf[LNCPW];
    int char_idx = 0;
	int chars_read_count = 0;

	cval = 0;
	while((c=mapch('\'')) >= 0) {
		if(char_idx < LNCPW) {
            cval_buf[char_idx++] = (char)c;
        }
        chars_read_count++;
    }

	if (chars_read_count > LNCPW) {
		error("Long character constant");
	}

    if (char_idx == 0) {
        cval = 0;
    } else if (char_idx == 1) {
        cval = (signed char)cval_buf[0];
    } else { // char_idx >= 2, typically up to LNCPW
        cval = (unsigned char)cval_buf[0];
        if (LNCPW > 1 && char_idx > 1) {
             cval |= ((unsigned char)cval_buf[1] << 8);
        }
    }
	return(CON);
}

/*
 * Read a character in a string or character constant,
 * detecting the end of the string.
 * It implements the escape sequences.
 */
static int mapch(int ac)
{
	register int a, mapped_c, n; // Renamed c to mapped_c to avoid conflict with ac
	static int mpeek_static; // Renamed mpeek to avoid conflict if mpeek is a global

	mapped_c = ac; // Use mapped_c for the parameter 'c' in original logic
	if ((a = mpeek_static)) // Corrected assignment in condition
		mpeek_static = 0;
	else
		a = getchar();
loop:
	if (a==mapped_c) // Use mapped_c
		return(-1);
	switch(a) {

	case '\n':
	case '\0':
		error("Nonterminated string"); // error will be handled by c0.h
		peekc = a;
		return(-1);

	case '\\':
		switch (a=getchar()) {

		case 't':
			return('\t');

		case 'n':
			return('\n');

		case 'b':
			return('\b');

		case 'f':
			return('\014');

		case 'v':
			return('\013');

		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			n = 0;
			mapped_c = 0; // Use mapped_c for original 'c' counter
			while (++mapped_c<=3 && '0'<=a && a<='7') {
				n <<= 3; // Corrected =<<
				n += a-'0'; // Corrected =+
				a = getchar();
			}
			mpeek_static = a; // Use mpeek_static
			return(n);

		case 'r':
			return('\r');

		case '\n':
			if (!inhdr)
				line++;
			inhdr = 0;
			a = getchar();
			goto loop;
		}
	}
	return(a);
}

/*
 * Read an expression and return a pointer to its tree.
 * It's the classical bottom-up, priority-driven scheme.
 * The initflg prevents the parse from going past
 * "," or ":" because those delimiters are special
 * in initializer (and some other) expressions.
 */
#if 0 // Temporarily exclude tree(), xprtype(), copnum()
struct tnode *
tree(void)
{
	int *op, opst[SSIZE], *pp, prst[SSIZE];
	register int andflg, o; // o is int
	register struct hshtab *cs;
	int p, ps, os; // p, ps, os are int
	struct tnode *cmst[CMSIZ];
	/* struct lnode *lcp; // lcp seems unused in the original code, but if used, needs definition */

	curbase = funcbase;
	op = opst;
	pp = prst;
	cp = cmst;
	*op = SEOF;
	*pp = 06;
	andflg = 0;

advanc:
	switch (o=symbol()) {

	case NAME:
		cs = csym;
		if (cs->hclass==TYPEDEF)
			goto atype;
		if (cs->hclass==ENUMCON) {
			*cp++ = (struct tnode *)cblock(cs->hoffset); // Ensure cast
			goto tand;
		}
		if (cs->hclass==0 && cs->htype==0) { // Added braces for dangling else
			if(nextchar()=='(') {
				/* set function */
				cs->hclass = EXTERN;
				cs->htype = FUNC;
			} else {
				cs->hclass = STATIC;
				error("%.8s undefined; func. %.8s", cs->name, funcsym->name);
				if (initflg) {
					cs->hclass = EXTERN;
					error("(Warning only)");
					nerror =- 2;
				}
			}
		*cp++ = nblock(cs);
		goto tand;

	case FCON:
		*cp++ = (struct tnode *)fblock(DOUBLE, copnum(cval)); // Cast already present from previous attempt
		goto tand;

	case LCON:
		cs = (struct hshtab *)gblock(sizeof(struct lnode));
		if (cs) {
		    ((struct lnode *)cs)->op = LCON;
		    ((struct lnode *)cs)->type = LONG;
		    ((struct lnode *)cs)->lvalue = lcval;
		}
		*cp++ = (struct tnode *)cs;
		goto tand;

	case CON:
		*cp++ = (struct tnode *)cblock(cval); // Cast already present
		goto tand;

	/* fake a static char array */
	case STRING:
		putstr(cval, 0);
		cs = (struct hshtab *)gblock(sizeof(struct hshtab));
		if (cs) {
		    cs->hclass = STATIC;
		    cs->hoffset = cval;
		}
		*cp++ = (struct tnode *)block(NAME, ARRAY+CHAR, &nchstr, NULL, cs); // Cast already present

	tand:
		if(cp>=cmst+CMSIZ) {
			error("Expression overflow");
			exit(1);
		}
		if (andflg)
			goto syntax;
		andflg = 1;
		goto advanc;

	case KEYW:
	atype:
		if (*op != LPARN || andflg)
			goto syntax;
		peeksym = o;
		*cp++ = (struct tnode *)xprtype((struct hshtab *)gblock(sizeof(struct hshtab)));
		if ((o=symbol()) != RPARN)
			goto syntax;
		o = CAST;
		--op;
		--pp;
		if (*op == SIZEOF) {
			andflg = 1;
			*pp = 100;
			goto advanc;
		}
		goto oponst;

	case INCBEF:
	case DECBEF:
		if (andflg)
			o =+ 2;
		goto oponst;

	case COMPL:
	case EXCLA:
	case SIZEOF:
		if (andflg)
			goto syntax;
		goto oponst;

	case MINUS:
		if (!andflg)
			o = NEG;
		andflg = 0;
		goto oponst;

	case AND:
	case TIMES:
		if (andflg)
			andflg = 0;
		else if (o==AND)
			o = AMPER;
		else
			o = STAR;
		goto oponst;

	case LPARN:
		if (andflg) {
			o = symbol();
			if (o==RPARN)
				o = MCALL;
			else {
				peeksym = o;
				o = CALL;
				andflg = 0;
			}
		}
		goto oponst;

	case RBRACK:
	case RPARN:
		if (!andflg)
			goto syntax;
		goto oponst;

	case DOT:
	case ARROW:
		mosflg++;
		break;

	case ASSIGN:
		if (andflg==0 && PLUS<=*op && *op<=EXOR) {
			o = *op-- + ASPLUS - PLUS;
			pp--;
			goto oponst;
		}
		break;

	}
	/* binaries */
	if (andflg==0)
		goto syntax;
	andflg = 0;

oponst:
	p = (opdope_pass0[o]>>9) & 077;
opon1:
	ps = *pp;
	if (p>ps || (p==ps && (opdope_pass0[o]&RASSOC)!=0)) { // Parentheses already correct
		switch (o) {

		case INCAFT:
		case DECAFT:
			p = 37;
			break;
		case LPARN:
		case LBRACK:
		case CALL:
			p = 04;
		}
		if (initflg) {
			if ((o==COMMA && *op!=LPARN && *op!=CALL)
			 || (o==COLON && *op!=QUEST)) {
				p = 00;
				goto opon1;
			}
		}
		if (op >= &opst[SSIZE-1]) {
			error("expression overflow");
			exit(1);
		}
		*++op = o;
		*++pp = p;
		goto advanc;
	}
	--pp;
	switch (os = *op--) {

	case SEOF:
		peeksym = o;
		build(0);		/* flush conversions */
		return(*--cp);

	case COMMA:
		if (*op != CALL)
			os = SEQNC;
		break;

	case CALL:
		if (o!=RPARN)
			goto syntax;
		build(os);
		goto advanc;

	case MCALL:
		*cp++ = NULL;	/* empty arglist */
		os = CALL;
		break;

	case INCBEF:
	case INCAFT:
	case DECBEF:
	case DECAFT:
		*cp++ = (struct tnode *)cblock(1);
		break;

	case LPARN:
		if (o!=RPARN)
			goto syntax;
		goto advanc;

	case LBRACK:
		if (o!=RBRACK)
			goto syntax;
		build(LBRACK);
		goto advanc;
	}
	build(os);
	goto opon1;

syntax:
	error("Expression syntax");
	errflush(o);
	return(0);
} // Closing brace for tree()
// Removed extra brace here

struct hshtab *
xprtype(struct hshtab *atyb)
{
	register struct hshtab *tyb;
	struct hshtab typer;
	int sc;
	register char *md, *fb;
	struct tnode **scp_ptr;

	tyb = atyb;
	fb = funcbase;
	md = maxdecl;
	scp_ptr = cp;
	funcbase = curbase;
	sc = DEFXTRN;		/* will cause error if class mentioned */
	getkeywords(&sc, &typer);
	tyb->hclass = 0;
	tyb->hblklev = 0;
	decl1(&sc, &typer, 0, tyb);
	funcbase = fb;
	maxdecl = md;
	cp = scp_ptr;
	((struct tnode *)tyb)->op = ETYPE; // Cast already present
	return(tyb);
}

char *
copnum(int len)
{
	register char *s1, *s2, *s3;

	s1 = s2 = (char *)gblock((len+LNCPW-1) & ~(LNCPW-1)); // Cast already present
	s3 = numbuf;
	while ((*s2++ = *s3++));
	return(s1);
}
#endif // End of exclusion for tree(), xprtype(), copnum()
