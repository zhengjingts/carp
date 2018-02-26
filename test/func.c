#include <stdio.h>
#include <string.h>
#include "func.h"

void trans_char(char *s, char *d)
{
        switch (*d)
        {
        case  0: *s = '0'; break;
        case  1: *s = '1'; break;
        case  2: *s = '2'; break;
        case  3: *s = '3'; break;
        case  4: *s = '4'; break;
        case  5: *s = '5'; break;
        case  6: *s = '6'; break;
        case  7: *s = '7'; break;
        case  8: *s = '8'; break;
        case  9: *s = '9'; break;
        case 10: *s = 'a'; break;
        case 11: *s = 'b'; break;
        case 12: *s = 'c'; break;
        case 13: *s = 'd'; break;
        case 14: *s = 'e'; break;
        case 15: *s = 'f'; break;
        default: break;
        }
}

int print(int n)
{
	int i;
	char s[5], *pn;

	for (i = 0, pn = (char*)&n; i < sizeof(n); i ++)
		trans_char(s+i, pn+i);
	s[i] = 0;

	printf("Hello World! %s\n", s);
}

int main()
{
	func = &print;
	if (func)
		(*func)(14);
	printf("Short Size: %d\n", sizeof(short));
	return 0;
}
