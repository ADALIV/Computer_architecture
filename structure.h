#include <stdio.h>

typedef struct _statistic {
    int no_cycles;
    int no_inst;
    int no_mem;
    int no_reg;
    int no_branch;
    int no_branch_not;
    int no_jump;
    int no_hit;
    int no_miss;
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

typedef struct _cacheline {
    int tag; // 26bit만 사용
    int data[0x10]; // 64bytes
    bool valid; // 초기값 0, false
    bool sca_bit; // 초기값 0, false
} cacheline;