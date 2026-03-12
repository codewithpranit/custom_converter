#include <stdio.h>
int main() {
    int cur_script = 1; /* Hindi */
    int b1 = 0x20; /* Space */
    if (b1 < 128 || (cur_script == 25 && b1 != 0xEF)) {
        if (cur_script != 25) { cur_script = 25; }
        printf("Space: cur_script=%d\n", cur_script);
    }
    int b2 = 0xB3; /* Ka */
    if (b2 < 128 || (cur_script == 25 && b2 != 0xEF)) {
        if (cur_script != 25) { cur_script = 25; }
        printf("Ka: Passthrough! cur_script=%d\n", cur_script);
    } else {
        printf("Ka: Indic processing! cur_script=%d\n", cur_script);
    }
    return 0;
}
