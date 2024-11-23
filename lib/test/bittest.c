/**
 * Small unit test for ensuring our SPS/PPS bit writing functions are accurate
 */

#include <byteswap.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gamepad/video.h"

int simple()
{
    uint8_t test = 0;
    size_t offset = 0;

    write_bits(&test, sizeof(test), &offset, 1, 1);

    if (test == 128) {
        printf("SUCCESS\n");
        return 0;
    } else {
        printf("FAIL (got %i)\n", test);
        return 1;
    }
}

int simple_double()
{
    uint8_t test = 0;
    size_t offset = 0;

    write_bits(&test, sizeof(test), &offset, 1, 1);

    offset = 7;
    write_bits(&test, sizeof(test), &offset, 1, 1);

    if (test != 129) {
        printf("FAIL (got %i)\n", test);
        return 1;
    }

    printf("SUCCESS\n");
    return 0;
}

int simple_exp_golomb()
{
    uint8_t test = 0;
    size_t index = 0;

    write_exp_golomb(&test, sizeof(test), &index, 1);

    if (test != 64) {
        printf("FAIL (got %i)\n", test);
        return 1;
    }

    printf("SUCCESS\n");
    return 0;
}

int complex_exp_golomb()
{
    // 00000110110
    uint16_t test = 0;
    size_t index = 5;

    write_exp_golomb(&test, sizeof(test), &index, 53);

    test = __bswap_16(test);

    if (test != 0b110110) {
        printf("FAIL (got %x)\n", test);
        return 1;
    }

    printf("SUCCESS\n");
    return 0;
}

int complex_bit_test()
{
    const char *expected = "\x67\x64\x00\x20";
    uint8_t data[4];
    size_t bit_index = 0;

    // forbidden_zero_bit
    write_bits(data, sizeof(data), &bit_index, 0, 1);

    // nal_ref_idc = 3 (important/SPS)
    write_bits(data, sizeof(data), &bit_index, 3, 2);

    // nal_unit_type = 7 (SPS)
    write_bits(data, sizeof(data), &bit_index, 7, 5);

    // profile_idc = 100 (not sure if this is correct, seems to work)
    write_bits(data, sizeof(data), &bit_index, 100, 8);

    // constraint_set0_flag
    write_bits(data, sizeof(data), &bit_index, 0, 1);

    // constraint_set1_flag
    write_bits(data, sizeof(data), &bit_index, 0, 1);

    // constraint_set2_flag
    write_bits(data, sizeof(data), &bit_index, 0, 1);

    // constraint_set3_flag
    write_bits(data, sizeof(data), &bit_index, 0, 1);

    // constraint_set4_flag
    write_bits(data, sizeof(data), &bit_index, 0, 1);

    // constraint_set5_flag
    write_bits(data, sizeof(data), &bit_index, 0, 1);

    // reserved_zero_2bits
    write_bits(data, sizeof(data), &bit_index, 0, 2);

    // level_idc (not sure if this is correct, seems to work)
    write_bits(data, sizeof(data), &bit_index, 0x20, 8);

    for (int i = 0; i < sizeof(data); i++) {
        if (i > 0) {
            printf(" ");
        }
        printf("%X", data[i] & 0xFF);
    }
    printf("\n");

    if (!memcmp(expected, data, sizeof(data))) {
        printf("SUCCESS\n");
        return 0;
    } else {
        printf("FAIL\n");
        return 1;
    }
}

int full_test()
{
    // const char *expected = "\x67\x64\x00\x20\xAC\x2B\x40\x6C\x1E\xF3\x68";

    //uint8_t buffer[0xB];
    uint8_t buffer[0x100];
    size_t size = generate_sps_params(buffer, sizeof(buffer));

    for (int i = 0; i < sizeof(buffer); i++) {
        if (i > 0) {
            printf(" ");
        }
        printf("%02X", buffer[i] & 0xFF);
    }
    printf("\n");
    printf("Size: %zu\n", size);

    /*if (!memcmp(expected, buffer, sizeof(buffer))) {
        printf("SUCCESS\n");
        return 0;
    } else {
        printf("FAIL\n");
        return 1;
    }*/
   return 0;
}

int main()
{
    if (simple()) {
        return 1;
    }
    
    if (simple_double()) {
        return 1;
    }

    if (complex_bit_test()) {
        return 1;
    }

    if (simple_exp_golomb()) {
        return 1;
    }

    if (complex_exp_golomb()) {
        return 1;
    }
    
    if (full_test()) {
        return 1;
    }

    return 0;
}