/**
 * @file
 * Simple program to generate correct floating point type names for a given
 * byte size.
 */

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv) {
  if (argc == 2) {
    int bytes = atoi(argv[1]) / 8;
    if (sizeof(long double) == bytes) {
      printf("long double");
    }
    else if (sizeof(double) == bytes) {
      printf("double");
    }
    else if (sizeof(float) == bytes) {
      printf("float");
    }
  }
}

