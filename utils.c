#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void find_sum(char sum[4][10], char data[4][4]) {
    for (int i = 0; i < 4; i++) {
        int total = 0;
        for (int j = 0; j < 4; j++) {
            if (data[i][j] != ' '){
                total += (data[i][j] - '0');
            }
            if(total < 10)  
                sprintf(sum[i], "0%d", total);
            else
                sprintf(sum[i], "%d", total);
        }
    }
}
