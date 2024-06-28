#include <stdio.h>

typedef struct _statistic {
    int no_cycles;
    int no_R;
    int no_I;
    int no_J;
    int no_mem_read;
    int no_mem_write;
    int no_branch;
    int no_branch_taken;
} statistic;

typedef struct _instruction {
    int opcode;
    int rs;
    int rt;
    int rd;
    int shamt;
    int funct;
    int imm;
    int s_imm;
    int z_imm;
    int addr;
    int b_addr;
    int j_addr;
} instruction;