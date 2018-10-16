#include <stdio.h>

#include "crt.h"

/*
 * Example of a generator using co-routines 
 */

int primes(crt_t *crt)
{
    /* Return list of prime numbers */
    CRT(crt)
    {
        CRT_YIELD(2);
        CRT_YIELD(3);
        CRT_YIELD(5);
        CRT_YIELD(7);
        CRT_YIELD(11);
        CRT_YIELD(13);
    }
    CRT_END;

    return 0;
}

int main(void)
{
    crt_t c;
    int n;

    CRT_INIT(&c);

    /* Print all numbers yielded by primes() */
    while ((n = primes(&c)) > 0)
    {
        printf("Number = %d\n", n);
    }
}
