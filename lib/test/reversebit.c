/**
 * Small unit test for ensuring our SPS/PPS bit writing functions are accurate
 */

#include <stdint.h>
#include <stdio.h>

#include "util.h"

int test1()
{
    const unsigned int control = 0b0000001111001100;
    const unsigned int expected = 0b0000000011001111;
    
    unsigned int test = reverse_bits(control, 10);
    if (test != expected) {
        printf("FAIL (got %x, expected %x)\n", test, expected);
        return 1;
    }

    printf("SUCCESS\n");
    return 0;
}

int test2()
{
    const unsigned int control = 0b10110;
    const unsigned int expected = 0b01101;
    
    unsigned int test = reverse_bits(control, 5);
    if (test != expected) {
        printf("FAIL (got %x, expected %x)\n", test, expected);
        return 1;
    }

    printf("SUCCESS\n");
    return 0;
}

int test3()
{
    const unsigned int control = 0b110011101010011;
    const unsigned int expected = 0b110010101110011;
    
    unsigned int test = reverse_bits(control, 15);
    if (test != expected) {
        printf("FAIL (got %x, expected %x)\n", test, expected);
        return 1;
    }

    printf("SUCCESS\n");
    return 0;
}

int main()
{
    if (test1()) {
        return 1;
    }

    if (test2()) {
        return 1;
    }

    if (test3()) {
        return 1;
    }

    return 0;
}