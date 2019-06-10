#include "/include/shell"
#include "/include/xtoa_atoi"
int main(int argc, char **argv) {
    int i = 0;
    if (argc > 1) {
        i = atoi32(argv[1]);
    }
    switch (i) {
        case 0: shell("/usr/test_command");
        case 1: shell("/usr/test_rec");
        case 2: shell("/usr/test_fork");
        case 3: shell("/usr/test_input");
        case 4: shell("/usr/test_resize");
        case 5: shell("/usr/test_malloc");
        case 6: shell("/usr/test_struct");
        case 7: shell("/usr/test_xtoa");
        case 8: shell("/usr/test_vector");
        case 9: shell("/usr/test_gui");
        case 10: shell("/usr/test_map");
    }
    return 0;
}