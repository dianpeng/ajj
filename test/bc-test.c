#define MINIMUM_CODE_PAGE_SIZE 1
#include "../src/bc.h"
#include "../src/vm.h"

#define EMIT0(I) \
  do { \
    emitter_emit0(&em,0,I); \
  } while(0)

#define EMIT1(I,A1) \
  do { \
    emitter_emit1(&em,0,I,A1); \
  } while(0)

#define EMIT2(I,A1,A2) \
  do { \
    emitter_emit2(&em,0,I,A1,A2); \
  } while(0)

#define EMIT0_AT(I,P) \
  do { \
    emitter_emit0_at(&em,0,P,I); \
  } while(0)

#define EMIT1_AT(I,P,A1) \
  do { \
    emitter_emit1_at(&em,0,P,I,A1); \
  } while(0)

#define EMIT2_AT(I,P,A1,A2) \
  do { \
    emitter_emit_at(&em,0,P,I,A1,A2); \
  } while(0)

static
void test1() {
  {
    struct emitter em;
    struct program prg;
    int instr;
    size_t i = 0;

    program_init(&prg);
    emitter_init(&em,&prg);

    assert(em.cd_cap == 1);
    assert(prg.len==0);

    EMIT0(VM_ADD);
    EMIT0(VM_MUL);
    EMIT0(VM_SUB);
    EMIT0(VM_DIV);
    EMIT1(VM_TPUSH,1);
    EMIT1(VM_BPUSH,2);
    EMIT2(VM_CALL,1,2);

    instr = bc_next(&prg,&i);
    assert(instr == VM_ADD);

    instr = bc_next(&prg,&i);
    assert(instr == VM_MUL);

    instr = bc_next(&prg,&i);
    assert(instr == VM_SUB);

    instr = bc_next(&prg,&i);
    assert(instr == VM_DIV);

    instr = bc_next(&prg,&i);
    assert(instr == VM_TPUSH);
    assert(bc_next_arg(&prg,&i) == 1);

    instr = bc_next(&prg,&i);
    assert(instr == VM_BPUSH);
    assert(bc_next_arg(&prg,&i) == 2);

    instr = bc_next(&prg,&i);
    assert(instr == VM_CALL);
    assert(bc_next_arg(&prg,&i)==1);
    assert(bc_next_arg(&prg,&i)==2);
    assert(bc_next(&prg,&i)==VM_HALT);
  }
}

int main() {
  test1();
  return 0;
}
