#include <stdio.h>

int
main (int argc, char** argv) {
        int i = 0;
        int k = 0xff;

        for (int j = 0; j < k; j++) {
                i++;
        }

        printf("Result: %d\n", i);

        return 0;
}