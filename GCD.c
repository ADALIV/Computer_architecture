#include <stdio.h>

int gcd(int a, int b);

int main(void)
{
	int a = 0x1298;
	int b = 0x9387;
	int res;

	res = gcd (a, b);

    return res;
}

int gcd(int a, int b)
{
	if (a==b) return a;
	else if (a>b) return gcd(a-b, b);
	else return gcd(b-a, a);
}