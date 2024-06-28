#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include "structure.h"

/*
binary file, high hex to high address
order is reversed
e.g. 0x12345678 32bit(4byte) hex number stored as
0byte = 78
1byte = 56
2byte = 34
3byte = 12

메모리 용량 = 0~0x1000000(sp) (2^24byte = 16Mbyte)
int 크기 4바이트로 나누기 0x400000
int memory[0x400000]
메모리 개개인의 주소는 하나의 바이트 단위

jal (0x0c000000) -> (0x0c000010)
jal 함수의 위치 0x00000040인 경우
0x00000040 >> 2 = 0x00000010
*/

int memory[0x400000]= {0,};
int R[32] = {0,};
int PC = 0; // program counter
int *SP = &R[29];
int *RA = &R[31]; // -1

static statistic st;
static instruction in;

void decode_instruction(int instruction);
bool check_overflow_add(int x, int y);
bool check_overflow_sub(int x, int y);

int main(int argc, char *argv[]) {
    FILE *fp = fopen("input.bin", "rb");
    int instruction = 0;
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
    int check_point = 0;

    while (1) {
        // insturction fetch
        if (PC == 0xffffffff) {
            printf("End of program\n");
            break;
        } else if (PC < 0 || PC > 0xFFFFFC) {
            printf("Wrong PC value with out of program arrange\n");
            break;
        } else if (i*4 > *SP) {
            printf("Stack overflow exception... End program\n");
            break;
        }
        decode_instruction(memory[PC/4]);
        // 'PC += 4;' must not be located in this loop
        // C language uses sequential logic = end of de_instruction, PC will increase
        // MIPS uses dataflow which means data increase automatically = as soon as de_instruction call, PC will increase
    }

    printf("\nNumber of cycles : %d\
            \nNumber of R type instructions : %d\
            \nNumber of I type instructions : %d\
            \nNumber of J type instructions : %d\
            \nNumber of memory read : %d\
            \nNumber of memory write : %d\
            \nNumber of branch : %d\
            \nNumber of branch_taken : %d\n",\
            st.no_cycles, st.no_R, st.no_I, st.no_J, st.no_mem_read, st.no_mem_write,\
            st.no_branch, st.no_branch_taken);
    printf("\nFinal return value : %d\n", R[2]);
    return 0;
}

void decode_instruction(int instruction) { // Instruction decode and execute
    int from = PC;
    PC += 4;

    // instuction decode
    // int opcode, rs, rt, rd, shamt, funct, imm, s_imm, z_imm, addr, b_addr, j_addr;
    in.opcode = (instruction >> 26) & 0x3f;
    in.rs = (instruction >> 21) & 0x1f;
    in.rt = (instruction >> 16) & 0x1f;
    in.rd = (instruction >> 11) & 0x1f;
    in.shamt = (instruction >> 6) & 0x1f;
    in.funct = instruction & 0x3f;
    in.imm = instruction & 0xffff;
    in.s_imm = (in.imm & 0x8000) ? (in.imm | 0xffff0000) : (in.imm);
    in.z_imm = in.imm & 0x00001111;
    in.addr = instruction & 0x3ffffff;
    in.b_addr = (in.imm & 0x8000) ? ((in.imm << 2) | 0xfffc0000) : (in.imm << 2);
    in.j_addr = (PC & 0xf0000000) | (in.addr << 2);

    // execute and ALU operation
    switch (in.opcode) {
    case 0x0: // R type instruction
        switch (in.funct) {
        case 0x20: // add
            if (check_overflow_add(R[in.rs], R[in.rt])) printf("overflow occur... ");
            R[in.rd] = R[in.rs] + R[in.rt];
            printf("(add) R[%d] = %d\n", in.rd, R[in.rd]);
            st.no_cycles++; st.no_R++;
            break;
        case 0x21: // addu
            R[in.rd] = R[in.rs] + R[in.rt];
            printf("(addu) R[%d] = %d\n", in.rd, R[in.rd]);
            st.no_cycles++; st.no_R++;
            break;
        case 0x24: // and
            R[in.rd] = R[in.rs] & R[in.rt];
            printf("(and) R[%d] = %d\n", in.rd, R[in.rd]);
            st.no_cycles++; st.no_R++;
            break;
        case 0x08: // jr
            PC = R[in.rs];
            printf("(jr) J to %d\n", PC);
            st.no_cycles++; st.no_R++;
            break;
        case 0x27: // nor
            R[in.rd] = ~(R[in.rs] | R[in.rt]);
            printf("(nor) R[%d] = %d\n", in.rd, R[in.rd]);
            st.no_cycles++; st.no_R++;
            break;
        case 0x25: // or
            R[in.rd] = R[in.rs] | R[in.rt];
            printf("(or) R[%d] = %d\n", in.rd, R[in.rd]);
            st.no_cycles++; st.no_R++;
            break;
        case 0x2a: // slt
            R[in.rd] = (R[in.rs] < R[in.rt]) ? 1 : 0;
            printf("(slt) R[%d] = %d\n", in.rd, R[in.rd]);
            st.no_cycles++; st.no_R++;
            break;
        case 0x2b: // sltu
            R[in.rd] = ((unsigned int)R[in.rs] < (unsigned int)R[in.rt]) ? 1 : 0;
            printf("(sltu) R[%d] = %d\n", in.rd, R[in.rd]);
            st.no_cycles++; st.no_R++;
            break;
        case 0x00: // sll
            R[in.rd] = R[in.rt] << in.shamt;
            printf("(sll) R[%d] = %d\n", in.rd, R[in.rd]);
            st.no_cycles++; st.no_R++;
            break;
        case 0x02: // srl
            R[in.rd] = R[in.rt] >> in.shamt;
            printf("(srl) R[%d] = %d\n", in.rd, R[in.rd]);
            st.no_cycles++; st.no_R++;
            break;
        case 0x22: // sub
            if (check_overflow_sub(R[in.rs], R[in.rt])) printf("overflow occur... ");
            R[in.rd] = R[in.rs] - R[in.rt];
            printf("(sub) R[%d] = %d\n", in.rd, R[in.rd]);
            st.no_cycles++; st.no_R++;
            break;
        case 0x23: // subu
            R[in.rd] = R[in.rs] - R[in.rt];
            printf("(subu) R[%d] = %d\n", in.rd, R[in.rd]);
            st.no_cycles++; st.no_R++;
            break;
        default:
            printf("Invalid funct\n");
            break;
        }
        break;
    case 0x8: // addi
        if (check_overflow_add(R[in.rs], in.s_imm)) printf("overflow occur... ");
        R[in.rt] = R[in.rs] + in.s_imm;
        printf("(addi) R[%d] = %d\n", in.rt, R[in.rt]);
        st.no_cycles++; st.no_I++;
        break;
    case 0x9: // addiu
        R[in.rt] = R[in.rs] + in.s_imm;
        printf("(addiu) R[%d] = %d\n", in.rt, R[in.rt]);
        st.no_cycles++; st.no_I++;
        break;
    case 0xc: // andi
        R[in.rt] = R[in.rs] & in.z_imm;
        printf("(andi) R[%d] = %d\n", in.rt, R[in.rt]);
        st.no_cycles++; st.no_I++;
        break;
    case 0x4: // beq
        if (R[in.rs] == R[in.rt]) {
                PC = PC + in.b_addr; // 상대 주소, already increased PC + 4
                printf("(beq) J to %d from %d\n", PC, from);
                st.no_branch_taken++;
        } else printf("(beq) Do not jump\n");
        st.no_cycles++; st.no_I++; st.no_branch++;
        break;
    case 0x5: // bne
        if (R[in.rs] != R[in.rt]) {
                PC = PC + in.b_addr; // 상대 주소, already increased PC + 4
                printf("(bne) J to %d from %d\n", PC, from);
                st.no_branch_taken++;
        } else printf("(bne) Do not jump\n");
        st.no_cycles++; st.no_I++; st.no_branch++;
        break;
    case 0x2: // j
        PC = in.j_addr; // 절대 주소
        printf("(J) J to %d from %d\n", PC, from);
        st.no_cycles++; st.no_J++;
        break;
    case 0x3: // jal
        *RA = PC+4; // already increased PC + 4
        PC = in.j_addr; // 절대 주소
        printf("(JAL) J to %d from %d\n", PC, from);
        st.no_cycles++; st.no_J++;
        break;
    case 0x24: // lbu
        R[in.rt] = memory[(R[in.rs]+in.s_imm)/4] & 0xff;
        printf("(lbu) R[%d] = %d\n", in.rt, R[in.rt]);
        st.no_cycles++; st.no_I++; st.no_mem_read++;
        break;
    case 0x25: // lhu
        R[in.rt] = memory[(R[in.rs]+in.s_imm)/4] & 0xffff;
        printf("(lhu) R[%d] = %d\n", in.rt, R[in.rt]);
        st.no_cycles++; st.no_I++; st.no_mem_read++;
        break;
    case 0x30: // ll
        R[in.rt] = memory[(R[in.rs]+in.s_imm)/4];
        R[in.rt] = (R[in.rt] == memory[(R[in.rs]+in.s_imm)/4]) ? 1 : 0;
        printf("(ll) R[%d] = %d\n", in.rt, R[in.rt]);
        st.no_cycles++; st.no_I++; st.no_mem_read++;
        break;
    case 0xf: // lui
        R[in.rt] = in.imm << 16;
        printf("(lui) R[%d] = %d\n", in.rt, R[in.rt]);
        st.no_cycles++; st.no_I++;
        break;
    case 0x23: // lw
        R[in.rt] = memory[(R[in.rs]+in.s_imm)/4];
        printf("(lw) R[%d] = %d\n", in.rt, R[in.rt]);
        st.no_cycles++; st.no_I++; st.no_mem_read++;
        break;
    case 0xd: // ori
        R[in.rt] = R[in.rs] | in.z_imm;
        printf("(ori) R[%d] = %d\n", in.rt, R[in.rt]);
        st.no_cycles++; st.no_I++;
        break;
    case 0xa: // slti
        R[in.rt] = (R[in.rs] < in.s_imm) ? 1 : 0;
        printf("(slti) R[%d] = %d\n", in.rt, R[in.rt]);
        st.no_cycles++; st.no_I++;
        break;
    case 0xb: // sltiu
        R[in.rt] = ((unsigned int)R[in.rs] < (unsigned int)in.s_imm) ? 1 : 0;
        printf("(sltiu) R[%d] = %d\n", in.rt, R[in.rt]);
        st.no_cycles++; st.no_I++;
        break;
    case 0x28: // sb
        memory[(R[in.rs]+in.s_imm)/4] = (memory[(R[in.rs]+in.s_imm)/4] & 0xffffff00) | (R[in.rt] & 0xff);
        printf("(sb) M[%08x] = %d\n", (R[in.rs]+in.s_imm)/4, memory[(R[in.rs]+in.s_imm)/4]);
        st.no_cycles++; st.no_I++; st.no_mem_write++;
        break;
    case 0x38: // sc
        memory[(R[in.rs]+in.s_imm)/4] = R[in.rt];
        R[in.rt] = (memory[(R[in.rs]+in.s_imm)/4] == R[in.rt]) ? 1 : 0;
        printf("(sc) M[%08x] = %d\n", (R[in.rs]+in.s_imm)/4, memory[(R[in.rs]+in.s_imm)/4]);
        st.no_cycles++; st.no_I++; st.no_mem_write++;
        break;
    case 0x29: // sh
        memory[(R[in.rs]+in.s_imm)/4] = (memory[(R[in.rs]+in.s_imm)/4] & 0xffff0000) | (R[in.rt] & 0xffff);
        printf("(sh) M[%08x] = %d\n", (R[in.rs]+in.s_imm)/4, memory[(R[in.rs]+in.s_imm)/4]);
        st.no_cycles++; st.no_I++; st.no_mem_write++;
        break;
    case 0x2b: // sw
        memory[(R[in.rs]+in.s_imm)/4] = R[in.rt];
        printf("(sw) M[%08x] = %d\n", (R[in.rs]+in.s_imm)/4, memory[(R[in.rs]+in.s_imm)/4]);
        st.no_cycles++; st.no_I++; st.no_mem_write++;
        break;
    case 0x1c: // mul
        if (in.funct == 0x2) {
            R[in.rd] = R[in.rs] * R[in.rt];
            printf("(mul) R[%d] = %d\n", in.rd, R[in.rd]);
            st.no_cycles++; st.no_R++;
        }
        break;
    default:
        printf("Invalid opcode\n");
        break;
    }
    return;
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