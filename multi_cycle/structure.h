#include <stdio.h>

typedef struct _statistic {
    int no_cycles;
    int no_inst;
    int no_mem;
    int no_reg;
    int no_branch;
    int no_branch_not;
    int no_jump;
} statistic;

typedef struct _IFID_latch {
    int PC;
    int inst;
} IFID_latch;

typedef struct _IDEX_latch {
    int PC;
    int opcode;
    int funct;
    int b_addr;
    int j_addr;
    int v1;
    int v2;
    int R_write; // rt, rd, 31
    int R_read1;
    int R_read2;
} IDEX_latch;

typedef struct _EXMEM_latch {
    int PC;
    int opcode;
    int funct;
    int ALU_result;
    int R_write; // rt, rd, 31
    bool MEM_read;
    bool MEM_write;
    int R_read1;
    int R_read2;
} EXMEM_latch;

typedef struct _MEMWB_latch {
    int PC;
    int opcode;
    int funct;
    int R_return;
    int R_write; // rt, rd, 31
    int R_write_value;
} MEMWB_latch;