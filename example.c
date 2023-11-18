// include other header files as needed
#include"mems.h"


#include "mems.h"
#include <stdio.h>

int main(int argc, char const *argv[]) {
    // initialize the MeMS system
    mems_init();
    int *ptr[10];

    printf("\n------- Allocated virtual addresses [mems_malloc] -------\n");
    for (int i = 0; i < 10; i++) {
        ptr[i] = (int *)mems_malloc(sizeof(int) * 250);
        printf("Virtual address: %p\n", (void *)ptr[i]);
    }

    printf("\n------ Assigning value to Virtual address [mems_get] -----\n");
    int *phy_ptr = (int *)mems_get(&ptr[0][1]);
    phy_ptr[0] = 200;
    int *phy_ptr2 = (int *)mems_get(&ptr[0][0]);
    printf("Virtual address: %p\tPhysical Address: %p\n", (void *)ptr[0], (void *)phy_ptr2);
    printf("Value written: %d\n", phy_ptr2[1]);

    printf("\n--------- Printing Stats [mems_print_stats] --------\n");
    mems_print_stats();

    printf("\n--------- Freeing up the memory [mems_free] --------\n");
    mems_free(ptr[3]);
    mems_print_stats();

    ptr[3] = (int *)mems_malloc(sizeof(int) * 250);
    mems_print_stats();

    printf("\n--------- Unmapping all memory [mems_finish] --------\n\n");
    mems_finish();

    return 0;
}

