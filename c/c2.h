#ifndef C2_H
#define C2_H

/*
 * Header for object code improver
 */

#include <stdio.h>
#include "array_sizes.h" /* Added for consistency, may or may not be used directly by c2.h */

#define	JBR	1
#define	CBR	2
#define	JMP	3
#define	LABEL	4
#define	DLABEL	5
#define	EROU	7
#define	JSW	9
#define	MOV	10
#define	CLR	11
#define	COM	12
#define	INC	13
#define	DEC	14
#define	NEG	15
#define	TST	16
#define	ASR	17
#define	ASL	18
#define	SXT	19
#define	CMP	20
#define	ADD	21
#define	SUB	22
#define	BIT	23
#define	BIC	24
#define	BIS	25
#define	MUL	26
#define	DIV	27
#define	ASH	28
#define	XOR	29
#define	TEXT	30
#define	DATA	31
#define	BSS	32
#define	EVEN	33
#define	MOVF	34
#define	MOVOF	35
#define	MOVFO	36
#define	ADDF	37
#define	SUBF	38
#define	DIVF	39
#define	MULF	40
#define	CLRF	41
#define	CMPF	42
#define	NEGF	43
#define	TSTF	44
#define	CFCC	45
#define	SOB	46
#define	JSR	47
#define	END	48

#define	JEQ	0
#define	JNE	1
#define	JLE	2
#define	JGE	3
#define	JLT	4
#define	JGT	5
#define	JLO	6
#define	JHI	7
#define	JLOS	8
#define	JHIS	9

#define	BYTE	100
#define	LSIZE	512

struct node {
	char	op;
	char	subop;
	struct	node	*forw;
	struct	node	*back;
	struct	node	*ref;
	int	labno;
	char	*code;
	int	refc;
};

struct optab {
	char	*opstring;
	int	opcode;
} optab[];

char	line[LSIZE];
struct	node	first;
char	*curlp;
int	nbrbr;
int	nsaddr;
int	redunm;
int	iaftbr;
int	njp1;
int	nrlab;
int	nxjump;
int	ncmot;
int	nrevbr;
int	loopiv;
int	nredunj;
int	nskip;
int	ncomj;
int	nsob;
int	nrtst;
int	nlit;

int	nchange;
int	isn;
int	debug;
int	lastseg;
char	*lasta;
char	*lastr;
char	*firstr;
char	revbr[];
char	regs[12][20];
char	conloc[20];
char	conval[20];
char	ccloc[20];

#define	RT1	10
#define	RT2	11
#define	FREG	5
#define	NREG	5
#define	LABHS	127
#define	OPHS	57

#include <unistd.h> // For sbrk (if still used directly) and other POSIX functions
#include <stdlib.h> // For exit, malloc, free (if alloc uses them)

struct optab *ophash[OPHS];

// Functions from c20.c
int main(int argc, char *argv[]); // Standard main signature
int input(void);
int getline(void);
int getnum(char *ap);
void output(void);
void reducelit(struct node *at);
char *copy(int na, const char *s1, const char *s2); // Tentative: handles 1 or 2 strings based on na
void opsetup(void);
int oplook(void);
void refcount(void);
void iterate(void);
void xjump(struct node *p1);
struct node *insertl(struct node *oldp);
struct node *codemove(struct node *p);
void comjump(void);
void backjmp(struct node *ap1, struct node *ap2);

// Functions from c21.c (add all as extern for now, will be refined)
void rmove(void);
int jumpsw(void);
void addsob(void);
int toofar(struct node *p);
int ilen(struct node *p);
int adrlen(char *s);
extern int kk_abs(int x); // Note: conflicts with stdlib.h abs, may need renaming or static.
int equop(struct node *ap1, struct node *p2);
void decref(struct node *p); // Note: name conflict with c04.c/c11.c decref
struct node *nonlab(struct node *p);
char *alloc(int n);
void clearreg(void);
void savereg(int ai, char *as);
void dest(char *as, int flt);
void singop(struct node *ap);
void dualop(struct node *ap);
int findrand(char *as, int flt);
int isreg(char *as);
void check(void); // Potentially for debugging
int source(char *ap);
void repladdr(struct node *p, int f, int flt);
void movedat(void);
void redunbr(struct node *p);
char *findcon(int i);
int compare(int oper, char *cp1, char *cp2);
void setcon(char *ar1, char *ar2);
int equstr(char *ap1, char *ap2);
void setcc(char *ap);
int natural(char *ap);

#endif /* C2_H */
