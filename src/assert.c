
#include<plibc/stdio.h>

#define assert(expr)	((void) 0)

#define likely(exp)	((void) 0)
#define unlikely(exp)	((void) 0)


void assertion_failed (const char *pExpr, const char *pFile, unsigned nLine)
{
    printf("Assertion failed: Expr: %s file: %s line: %d \n", pExpr, pFile, nLine);
	for (;;);
}
