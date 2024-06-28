#include <stdio.h>

int re(int n);

int main(void) {
    int num;
    num = re(10);

    return num;
}

int re(int n) {
    if (n == 1) return 1;
    else return n+re(n-1);
}