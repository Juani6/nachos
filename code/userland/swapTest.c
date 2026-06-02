#include "lib.h"

#define DIM 256
static int A[DIM];

int main() {
    // Primera pasada — trae páginas a memoria
    for (int i = 0; i < DIM; i++) {
        A[i] = i;
    }

    // Forzar que otras páginas expulsen a A al swap
    // (necesitás otro array grande acá)
    static int B[DIM];
    for (int i = 0; i < DIM; i++) {
        B[i] = i * 2;
    }

    // Segunda pasada — A debería volver del swap con los valores originales
    int ok = 1;
    for (int i = 0; i < DIM; i++) {
        if (A[i] != i) ok = 0;
    }
    
    if (ok) {
        puts2("Todo ok\n");
    }
    else {
        puts2("Todo mal\n");
    }
    return 0;
}