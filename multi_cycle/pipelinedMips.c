#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include "structure.h"

/*
메모리 용량 = 0~0x1000000(sp) (2^24byte = 16Mbyte)
int 크기 4바이트로 나누기 0x400000
int memory[0x400000]
메모리 개개인의 주소는 하나의 바이트 단위

jal (0x0c000000) -> (0x0c000010)
jal 함수의 위치 0x00000040인 경우
0x00000040 >> 2 = 0x00000010
jal 함수의 위치 0x00000050인 경우
0x00000050 >> 2 = 0x00000014

구조적 해저드 = 하드웨어가 중복 사용되는 경우
데이터 해저드 = 명령어들간 종속성이 부여되는 경우 (연속된 R2레지스터 접근 및 변경)
제어 해저드 = 분기 경우, true인 순간, 의미없는 명령어들이 차례로 들어온 상태
*/

int memory[0x400000]= {0,};
int R[32] = {0,};
int *SP = &R[29];
int *RA = &R[31]; // -1

// 0-for output, 1-for input to next
IFID_latch IFID[2];
IDEX_latch IDEX[2];
EXMEM_latch EXMEM[2];
MEMWB_latch MEMWB[2];
static statistic st;
bool d_hazard = false;
bool c_hazard = false;

bool check_overflow_add(int x, int y);
bool check_overflow_sub(int x, int y);
void data_hazard();
void control_hazard();

void IF(MEMWB_latch *in, IFID_latch *out) {
    out->PC = in->PC + 16;
    out->inst = memory[out->PC/4];
    printf("(IF) 0x%x instruction, inst is 0x%x\n", out->PC, out->inst);
}

void ID(IFID_latch *in, IDEX_latch *out) {
    out->PC = in->PC;
    int inst = in->inst;
    int opcode = out->opcode = (inst >> 26) & 0x3f;
    int imm = inst & 0xffff;
    int addr = inst & 0x3ffffff;
    int rs = (inst >> 21) & 0x1f;
    int rt = (inst >> 16) & 0x1f;
    int rd = (inst >> 11) & 0x1f;
    out->funct = 0;
    out->b_addr = 0;
    out->j_addr = 0;
    out->v1 = 0;
    out->v2 = 0;
    out->R_write = -1;
    out->R_read1 = -1;
    out->R_read2 = -1;

    if (opcode == 0) { // R
        out->funct = inst & 0x3f;
        out->v1 = ((out->funct == 0x00) || (out->funct == 0x02)) ? (inst >> 6) & 0x1f : R[rs]; // shamt to v1
        out->v2 = R[rt];
        out->R_write = rd;
        out->R_read1 = ((out->funct == 0x00) || (out->funct == 0x02)) ? -1 : rs;
        out->R_read2 = (out->funct == 0x08) ? -1 : rt;
    } else if ((opcode == 0x2) || (opcode == 0x3)) { // J
        out->j_addr = (in->PC & 0xf0000000) | (addr << 2);
        out->R_write = (opcode == 0x3) ? 31 : -1;
        out->R_read1 = (opcode == 0x3) ? 31 : -1;
    } else if ((opcode == 0x4) || (opcode == 0x5)) { // I-branch
        out->v1 = R[rs];
        out->v2 = R[rt];
        out->b_addr = (imm & 0x8000) ? ((imm << 2) | 0xfffc0000) : (imm << 2);
        out->R_read1 = rs;
        out->R_read2 = rt;
    } else { // I
        out->v1 = R[rs];
        out->v2 = (imm & 0x8000) ? (imm | 0xffff0000) : (imm);
        out->R_write = rt;
        out->R_read1 = rs;
        if ((opcode == 0x28) || (opcode == 0x38) || (opcode == 0x29) || (opcode == 0x2b)) { // sw
            out->R_write = R[rt];
            out->R_read2 = rt;
        }
    }
    printf("(ID) 0x%x instruction, opcode is 0x%x\n", out->PC, out->opcode);
}

void EX(IDEX_latch *in, EXMEM_latch *out) {
    out->PC = in->PC;
    out->opcode = in->opcode;
    out->funct = in->funct;
    out->ALU_result = 0;
    out->R_write = in->R_write;
    out->MEM_read = false;
    out->MEM_write = false;
    out->R_read1 = in->R_read1;
    out->R_read2 = in->R_read2;
    switch (in->opcode) {
    case 0x0:
        switch (in->funct) {
        case 0x20: // add
            if (check_overflow_add(in->v1, in->v2)) printf("overflow occur... ");
            out->ALU_result = in->v1 + in->v2;
            break;
        case 0x21: // addu
            out->ALU_result = in->v1 + in->v2;
            break;
        case 0x24: // and
            out->ALU_result = in->v1 & in->v2;
            break;
        case 0x08: // jr
            out->PC = in->v1;
            c_hazard = true;
            break;
        case 0x27: // nor
            out->ALU_result = ~(in->v1 | in->v2);
            break;
        case 0x25: // or
            out->ALU_result = in->v1 | in->v2;
            break;
        case 0x2a: // slt
            out->ALU_result = (in->v1 < in->v2) ? 1 : 0;
            break;
        case 0x2b: // sltu
            out->ALU_result = ((unsigned int)in->v1 & (unsigned int)in->v2) ? 1 : 0;
            break;
        case 0x00: // sll
            out->ALU_result = in->v2 << in->v1;
            break;
        case 0x02: // srl
            out->ALU_result = in->v2 >> in->v1;
            break;
        case 0x22: // sub
            if (check_overflow_sub(in->v1, in->v2)) printf("overflow occur... ");
            out->ALU_result = in->v1 - in->v2;
            break;
        case 0x23: // subu
            out->ALU_result = in->v1 - in->v2;
            break;
        default:
            printf("Invalid funct\n");
            break;
        }
        break;
    case 0x8: // addi
        if (check_overflow_add(in->v1, in->v2)) printf("overflow occur... ");
        out->ALU_result = in->v1 + in->v2;
        break;
    case 0x9: // addiu
        out->ALU_result = in->v1 + in->v2;
        break;
    case 0xc: // andi
        out->ALU_result = in->v1 & in->v2;
        break;
    case 0x4: // beq
        if (in->v1 == in->v2) {
            out->PC += in->b_addr + 4;
            c_hazard = true;
            printf("(beq) jump\n");
            st.no_branch++;
        } else {
            out->PC = in->PC;
            c_hazard = false;
            printf("(beq) Do not jump\n");
            st.no_branch_not++;
        }
        break;
    case 0x5: // bne
        if (in->v1 != in->v2) {
            out->PC += in->b_addr + 4;
            c_hazard = true;
            printf("(bne) jump\n");
            st.no_branch++;
        } else {
            out->PC = in->PC;
            c_hazard = false;
            printf("(bne) Do not jump\n");
            st.no_branch_not++;
        }
        break;
    case 0x2: // j
        out->PC = in->j_addr;
        c_hazard = true;
        st.no_jump++;
        break;
    case 0x3: // jal
        out->ALU_result = in->PC + 8; // *RA
        R[out->R_write] = out->ALU_result;
        out->PC = in->j_addr;
        c_hazard = true;
        st.no_jump++;
        break;
    case 0x24: // lbu
        out->ALU_result = in->v1 + in->v2;
        out->MEM_read = true;
        break;
    case 0x25: // lhu
        out->ALU_result = in->v1 + in->v2;
        out->MEM_read = true;
        break;
    case 0x30: // ll
        out->ALU_result = in->v1 + in->v2;
        out->MEM_read = true;
        break;
    case 0xf: // lui
        out->ALU_result = in->v2 << 16;
        break;
    case 0x23: // lw
        out->ALU_result = in->v1 + in->v2;
        out->MEM_read = true;
        break;
    case 0xd: // ori
        out->ALU_result = in->v1 | (in->v2 & 0xffff);
        break;
    case 0xa: // slti
        out->ALU_result = (in->v1 < in->v2) ? 1 : 0;
        break;
    case 0xb: // sltiu
        out->ALU_result = ((unsigned int)in->v1 < (unsigned int)in->v2) ? 1 : 0;
        break;
    case 0x28: // sb
        out->ALU_result = in->v1 + in->v2;
        out->MEM_write = true;
        break;
    case 0x38: // sc
        out->ALU_result = in->v1 + in->v2;
        out->MEM_write = true;
        break;
    case 0x29: // sh
        out->ALU_result = in->v1 + in->v2;
        out->MEM_write = true;
        break;
    case 0x2b: // sw
        out->ALU_result = in->v1 + in->v2;
        out->MEM_write = true;
        break;
    default:
        printf("Invalid opcode\n");
        break;
    }
    printf("(EX) 0x%x instruction, opcode is 0x%x, funct is 0x%x\n", out->PC, out->opcode, out->funct);
}

void MEM(EXMEM_latch *in, MEMWB_latch *out) {
    out->PC = in->PC;
    out->opcode = in->opcode;
    out->funct = in->funct;
    out->R_return = -1;
    out->R_write = 0;
    out->R_write_value = 0;
    if ((in->opcode == 0x0) && (in->funct != 0x8)) {
        d_hazard = true;
        out->R_return = in->R_write;
        out->R_write = in->R_write;
        out->R_write_value = in->ALU_result;
    } else if (in->opcode == 0x3) {
        d_hazard = true;
        out->R_return = in->R_write;
        out->R_write = in->R_write;
        out->R_write_value = in->ALU_result;
    } else if ((in->opcode == 0x4) || (in->opcode == 0x5) || (in->opcode == 0x2) || (in->funct == 0x8)) {
        // no operation
    } else if (in->MEM_read) {
        d_hazard = true;
        out->R_return = in->R_write;
        switch (in->opcode) {
            case 0x24:
                out->R_write = in->R_write;
                out->R_write_value = memory[in->ALU_result/4] & 0xff;
                break;
            case 0x25:
                out->R_write = in->R_write;
                out->R_write_value = memory[in->ALU_result/4] & 0xffff;
                break;
            default:
                out->R_write = in->R_write;
                out->R_write_value = memory[in->ALU_result/4];
                break;
        }
    } else if (in->MEM_write) {
        d_hazard = true;
        out->R_return = -1;
        switch (in->opcode) {
            case 0x28:
                memory[in->ALU_result/4] = (memory[in->ALU_result/4] & 0xffffff00) | (in->R_write & 0xff);
                break;
            case 0x29:
                memory[in->ALU_result/4] = (memory[in->ALU_result/4] & 0xffff0000) | (in->R_write & 0xffff);
                break;
            default:
                memory[in->ALU_result/4] = in->R_write;
                break;
        }
        printf("M[%0x] = %d\n", in->ALU_result/4, memory[in->ALU_result/4]);
        st.no_mem++;
    } else {
        d_hazard = true;
        out->R_return = in->R_write;
        out->R_write = in->R_write;
        out->R_write_value = in->ALU_result;
    }
    printf("(MEM) 0x%x instruction, opcode is 0x%x\n", out->PC, out->opcode);
}

void WB(MEMWB_latch *in, IFID_latch *out) {
    if ((in->opcode == 0x4) || (in->opcode == 0x5) || (in->opcode == 0x2) || (in->funct == 0x8)) {
        // no operation
    } else if ((in->opcode == 0x28) || (in->opcode == 0x38) || (in->opcode == 0x29) || (in->opcode == 0x2b)) {
        // no operation
    } else {
        R[in->R_write] = in->R_write_value;
        printf("R[%d] = %d\n", in->R_write, in->R_write_value);
        st.no_reg++;
    }
    printf("(WB) 0x%x instruction done...\n", in->PC);
    st.no_inst++;
    IF(in, out);
}

int main(int argc, char *argv[]) {
    FILE *fp = fopen("input.bin", "rb");
    int instruction = 0;
    int PC;
    int i = 0;

    if (fp == NULL) {
        perror("File open error\n");
    }
    while (fread(&instruction, sizeof(instruction), 1, fp) != 0) {
        unsigned int b1, b2, b3, b4;
        b1 = instruction & 0xFF; // 0x000000FF
        b2 = instruction & 0xFF00;
        b3 = instruction & 0xFF0000;
        b4 = instruction & 0xFF000000; // or (var >> 24) & 0xFF, int res = ... | b4
        instruction = (b1 << 24) | (b2 << 8) | (b3 >> 8) | (b4 >> 24);
        memory[i++] = instruction; // Instruction fetch
    }
    fclose(fp);

    *SP = 0x1000000; // sp(stack pointer)
    *RA = 0xffffffff; // ra(return address)

    IFID[1].PC = 0;
    IFID[1].inst = memory[0];
    ID(&IFID[1], &IDEX[0]);

    IDEX[1] = IDEX[0];
    IFID[1].PC = 4;
    IFID[1].inst = memory[1];
    EX(&IDEX[1], &EXMEM[0]);
    ID(&IFID[1], &IDEX[0]);

    EXMEM[1] = EXMEM[0];
    IDEX[1] = IDEX[0];
    IFID[1].PC = 8;
    IFID[1].inst = memory[2];
    MEM(&EXMEM[1], &MEMWB[0]);
    EX(&IDEX[1], &EXMEM[0]);
    ID(&IFID[1], &IDEX[0]);

    IFID[0].PC = 12;
    IFID[0].inst = memory[3];
    MEMWB[1] = MEMWB[0];

    st.no_cycles += 4;

    int check;
    while (1) {
        if (d_hazard) {
            data_hazard();
            continue;
        }
        if (c_hazard) {
            control_hazard();
            continue;
        }
        if (i*4 > *SP) {
            printf("\nOverflow\n");
            break;
        }

        st.no_cycles++;
        MEMWB[1] = MEMWB[0];
        EXMEM[1] = EXMEM[0];
        IDEX[1] = IDEX[0];
        IFID[1] = IFID[0];

        printf("\n");
        WB(&MEMWB[1], &IFID[0]);
        MEM(&EXMEM[1], &MEMWB[0]);
        EX(&IDEX[1], &EXMEM[0]);
        ID(&IFID[1], &IDEX[0]);

        PC = EXMEM[0].PC;
        if (PC == 0xffffffff) {
            printf("\nPC is 0xffffffff\n");
            MEMWB[1] = MEMWB[0];
            WB(&MEMWB[1], &IFID[0]);
            st.no_inst++;
            break;
        }
        else if (PC < 0 || PC > 0xFFFFFC) {
            printf("\nPC out of index\n");
            break;
        }
    }

    printf("\nNumber of cycles : %d\
            \nNumber of instructions : %d\
            \nNumber of memory : %d\
            \nNumber of register : %d\
            \nNumber of branch : %d\
            \nNumber of branch not : %d\
            \nNumber of jump : %d\n",\
            st.no_cycles, st.no_inst, st.no_mem, st.no_reg, st.no_branch, st.no_branch_not, st.no_jump);
    printf("\nFinal return value : %d, RA : %d, SP : %d\n", R[2], R[31], R[29]);
    printf("\nEnd of program\n");
    return 0;
}

bool check_overflow_add(int x, int y) {
    if (x > INT_MAX-y) return true;
    if (x < INT_MIN-y) return true;
    return false;
}

bool check_overflow_sub(int x, int y) {
    if (x > INT_MAX+y) return true;
    if (x < INT_MIN+y) return true;
    return false;
}

void data_hazard() {
    printf("\ndata_hazard check...\n");
    if (MEMWB[0].R_return == -1) {
        d_hazard = false;
        return;
    }
    if (MEMWB[0].R_return == IDEX[0].R_read1) {
        IDEX[0].v1 = MEMWB[0].R_write_value;
    }
    if (MEMWB[0].R_return == IDEX[0].R_read2) {
        if ((IDEX[0].opcode == 0x28) || (IDEX[0].opcode == 0x38) || (IDEX[0].opcode == 0x29) || (IDEX[0].opcode == 0x2b)) { // sw
            IDEX[0].R_write = MEMWB[0].R_write_value;
        } else {IDEX[0].v2 = MEMWB[0].R_write_value;}
    }
    if (MEMWB[0].R_return == EXMEM[0].R_read1) {
        IDEX[1].v1 = MEMWB[0].R_write_value;
        EX(&IDEX[1], &EXMEM[0]);
    }
    if (MEMWB[0].R_return == EXMEM[0].R_read2) {
        if ((EXMEM[0].opcode == 0x28) || (EXMEM[0].opcode == 0x38) || (EXMEM[0].opcode == 0x29) || (EXMEM[0].opcode == 0x2b)) { // sw
            IDEX[1].R_write = MEMWB[0].R_write_value;
        } else {IDEX[1].v2 = MEMWB[0].R_write_value;}
        EX(&IDEX[1], &EXMEM[0]);
    }
    printf("end_data_hazard\n");
    d_hazard = false;
}

void control_hazard() {
    c_hazard = false;
    int addr = EXMEM[0].PC;
    printf("\ncontrol_hazard, addr is 0x%x\n", addr);
    MEMWB[1] = MEMWB[0];
    WB(&MEMWB[1], &IFID[0]);

    IFID[1].PC = addr;
    IFID[1].inst = memory[addr/4];
    ID(&IFID[1], &IDEX[0]);

    IDEX[1] = IDEX[0];
    IFID[1].PC = addr + 4;
    IFID[1].inst = memory[addr/4 + 1];
    EX(&IDEX[1], &EXMEM[0]);
    ID(&IFID[1], &IDEX[0]);

    if (c_hazard) {
        st.no_cycles += 2;
        memset(&MEMWB[0], 0, sizeof(MEMWB_latch));
        control_hazard();
        return;
    }

    EXMEM[1] = EXMEM[0];
    IDEX[1] = IDEX[0];
    IFID[1].PC = addr + 8;
    IFID[1].inst = memory[addr/4 + 2];
    MEM(&EXMEM[1], &MEMWB[0]);
    EX(&IDEX[1], &EXMEM[0]);
    ID(&IFID[1], &IDEX[0]);

    if (c_hazard) {
        st.no_cycles += 3;
        control_hazard();
        return;
    }

    st.no_cycles += 4;
    IFID[0].PC = addr + 12;
    IFID[0].inst = memory[addr/4 + 3];
    MEMWB[1] = MEMWB[0];
    printf("end_control_hazard\n");
}