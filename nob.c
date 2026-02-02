// nob.c
#define NOB_IMPLEMENTATION
#define NOB_NO_ECHO
#include "nob.h"

#define EXEC "sort"
#define INPUT_FILE "input.txt"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);
    Nob_Cmd cmd = { 0 };
    nob_cmd_append(&cmd, "gcc", "-Wall", "-L\"C:\\Kode\\lib\"", "-Wextra", "-std=c23", "-g", "-DDEBUG", "-o", EXEC, "main.c");
    if(!nob_cmd_run(&cmd)) return 1;
    // ./nob build = do not run executable
    // ./nob debug = run gdb
    Nob_Cmd exe = { 0 };

    if(argc >= 2) {
        if(strncmp("build", argv[1], strlen("build")) == 0) return 0;
        if(strncmp("debug", argv[1], strlen("debug")) == 0) nob_cmd_append(&exe, "gdb", EXEC);
    } else {
        nob_cmd_append(&exe, "./"EXEC, "input.txt");
        // printf("\n\n\n\n"); // put a space between program output and gcc output
    }

    if(!nob_cmd_run(&exe)) return 1;
    return 0;
}