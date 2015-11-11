#ifndef _AJJ_PRIV_H_
#define _AJJ_PRIV_H_
#include "ajj.h"
#include "util.h"

#include <ctype.h>
#include <assert.h>
#include <stdio.h>

#define MINIMUM_CODE_PAGE_SIZE 256
#define LOOP_CONTINUE 0
#define LOOP_BREAK 1


/* Helper macro to insert into double linked list */
#define LINIT(X) \
  do { \
    (X)->next = (X); \
    (X)->prev = (X); \
  } while(0)

#define LINSERT(X,T) \
  do { \
    (X)->next = (T); \
    (X)->prev = (T)->prev; \
    (T)->prev->next = (X); \
    (T)->prev = (X); \
  } while(0)

#define LREMOVE(X) \
  do { \
    (X)->prev->next = (X)->next; \
    (X)->next->prev = (X)->prev; \
  } while(0)

/* =================================
 * Forward
 * ================================*/
struct gc_scope;

/* ==================================
 * Tokenizer
 * ================================*/
struct tokenizer {
  const char* src;
  size_t pos;
  int tk;
  size_t tk_len;
  int mode; /* tell me which tokenize mode need to go into */
  struct strubuf lexme; /* for lexme */
  double num_lexme; /* only useful when the token is TK_NUMBER */
};

enum {
  TOKENIZE_JINJA,
  TOKENIZE_SCRIPT,
  TOKENIZE_RAW
};

#define TOKEN_LIST(X) \
  X(TK_TEXT,"<text>") \
  X(TK_COMMENT,"<comment>") \
  X(TK_LSTMT,"{%") \
  X(TK_RSTMT,"%}") \
  X(TK_LEXP,"{{") \
  X(TK_REXP,"}}") \
  X(TK_IF,"if") \
  X(TK_ELIF,"elif") \
  X(TK_ELSE,"else") \
  X(TK_ENDIF,"endif") \
  X(TK_FOR,"for") \
  X(TK_ENDFOR,"endfor") \
  X(TK_MACRO,"macro") \
  X(TK_ENDMACRO,"endmacro") \
  X(TK_CALL,"call") \
  X(TK_ENDCALL,"endcall") \
  X(TK_FILTER,"filter") \
  X(TK_ENDFILTER,"endfilter") \
  X(TK_DO ,"do") \
  X(TK_SET,"set") \
  X(TK_ENDSET,"endset") \
  X(TK_MOVE,"move") \
  X(TK_BLOCK,"block") \
  X(TK_ENDBLOCK,"endblock") \
  X(TK_RAW,"raw") \
  X(TK_ENDRAW,"endraw") \
  X(TK_EXTENDS,"extends") \
  X(TK_IMPORT,"import") \
  X(TK_INCLUDE,"include") \
  X(TK_FROM,"from") \
  X(TK_IN,"in") \
  X(TK_AS,"as") \
  X(TK_RECURSIVE,"recursive") \
  X(TK_CONTINUE,"continue") \
  X(TK_BREAK,"break") \
  X(TK_UPVALUE,"upvalue") \
  X(TK_OVERRIDE,"override") \
  X(TK_FIXED,"fix") \
  X(TK_LPAR,"(") \
  X(TK_RPAR,")") \
  X(TK_LSQR,"[") \
  X(TK_RSQR,"]") \
  X(TK_LBRA,"{") \
  X(TK_RBRA,"}") \
  X(TK_ADD,"+") \
  X(TK_SUB,"-") \
  X(TK_MUL,"*") \
  X(TK_DIV,"/") \
  X(TK_DIVTRUCT,"//") \
  X(TK_MOD,"%") \
  X(TK_POW,"**") \
  X(TK_ASSIGN,"=") \
  X(TK_EQ,"==") \
  X(TK_NE,"!=") \
  X(TK_LT,"<") \
  X(TK_LE,"<=") \
  X(TK_GT,">") \
  X(TK_GE,">=") \
  X(TK_AND,"and") \
  X(TK_OR,"or") \
  X(TK_NOT,"not") \
  X(TK_PIPE,"|") \
  X(TK_DOT,".") \
  X(TK_COMMA,",") \
  X(TK_COLON,":") \
  X(TK_STRING,"<string>") \
  X(TK_NUMBER,"<number>") \
  X(TK_VARIABLE,"<variable>") \
  X(TK_TRUE,"<true>") \
  X(TK_FALSE,"<false>") \
  X(TK_NONE,"<none>") \
  X(TK_EOF,"<eof>") \
  X(TK_UNKNOWN,"<unknown>")

#define X(A) A ,
enum {
  TOKEN_LIST(A),
  SIZE_OF_TOKENS
};
#undef X

const char* tk_get_name( int );

int tk_lex( struct tokenizer* tk );
int tk_move( struct tokenizer* tk );

static inline
int tk_init( struct tokenizer* tk , const char* src ) {
  tk->src = src;
  tk->pos = 0;
  tk->mode = TOKENIZE_JINJA;
  strbuf_init(&(tk->lexme));
  return tk_lex(tk);
}

static inline
void tk_destroy( struct tokenizer* tk ) {
  strbuf_destroy(&(tk->lexme));
}

static inline
int tk_expect( struct tokenizer* tk , int t ) {
  if( tk_lex(tk) == t ) {
    tk_move(tk);
    return 0;
  } else {
    return -1;
  }
}

static inline
int tk_id_ichar( char c ) {
  return c == '_' || isalpha(c) ;
}

static inline
int tk_not_id_ichar( char c ) {
  return !tk_id_ichar(c);
}

static inline
int tk_id_rchar( char c ) {
  return c == '_' || isalpha(c) || isdigit(c);
}

static inline
int tk_not_id_rchar( char c ) {
  return !tk_id_rchar(c);
}

static inline
int tk_body_escape( char c ) {
  return c == '{';
}

/* =============================
 * Virtual Machine
 * ============================*/

#define VM_INSTRUCTIONS(X) \
  X(VM_ADD,"add") \
  X(VM_SUB,"sub") \
  X(VM_DIV,"div") \
  X(VM_MUL,"mul") \
  X(VM_MOD,"mod") \
  X(VM_POW,"pow") \
  X(VM_EQ,"eq") \
  X(VM_NE,"ne") \
  X(VM_LT,"lt") \
  X(VM_LE,"le") \
  X(VM_GT,"gt") \
  X(VM_GE,"ge") \
  X(VM_AND,"and") \
  X(VM_OR,"or") \
  X(VM_NOT,"not") \
  X(VM_NEG,"neg") \
  X(VM_DIVTRUCT,"divtruct") \
  X(VM_TENARY,"tenary") \
  X(VM_CALL,"call") \
  X(VM_RET,"ret") \
  X(VM_PRINT,"print") \
  X(VM_POP,"pop") \
  X(VM_TPUSH,"tpush") \
  X(VM_TSWAP,"tswap") \
  X(VM_TLOAD,"tload") \
  X(VM_BPUSH,"bpush") \
  X(VM_BSWAP,"bswap") \
  X(VM_BLOAD,"bload") \
  X(VM_BT_MOVE,"bt_move") \
  X(VM_BB_MOVE,"bb_move") \
  X(VM_TB_MOVE,"tb_move") \
  X(VM_TT_MOVE,"tt_move") \
  X(VM_LSTR,"lstr") \
  X(VM_LTRUE,"ltrue") \
  X(VM_LFALSE,"lfalse") \
  X(VM_LNUM,"lnum") \
  X(VM_LZERO,"lzero") \
  X(VM_LNONE,"lnone") \
  X(VM_LIMM,"limm") \
  X(VM_LLIST,"llist") \
  X(VM_LDICT,"ldict") \
  X(VM_ATTR_SET,"attrset") \
  X(VM_ATTR_PUSH,"attrpush") \
  X(VM_ATTR_GET,"attrget") \
  X(VM_ATTR_CALL,"attrcall") \
  X(VM_UPVALUE_SET,"upvalueset") \
  X(VM_UPVALUE_GET,"upvalueget") \
  X(VM_UPVALUE_DEL,"upvaluedel") \
  X(VM_LOOP,"loop") \
  X(VM_LOOPREC,"looprec") \
  X(VM_JMP,"jmp") \
  X(VM_JT,"jt") \
  X(VM_JEPT,"jept") \
  X(VM_ENTER,"enter") \
  X(VM_EXIT,"exit") \
  X(VM_PIPE,"pipe") \
  X(VM_INCLUDE,"include") \
  X(VM_IMPORT,"import") \
  X(VM_IMPORT_SYMBOL,"importsymbol") \
  X(VM_EXTENDS,"extends")

enum vm_instructions {
#define X(A,B) A,
  VM_INSTRUCTIONS(X),
  SIZE_OF_INSTRUCTIONS
#undef X
};

const char* vm_get_instr_name( int );

extern struct string THIS = { "__this__",8};
extern struct string ARGNUM = {"__argnum__",10};
extern struct string MAIN = { "__main__",8 };
extern struct string CALLER = {"__caller__",10};
extern struct string SUPER  = {"super",5};
extern struct string SELF   = {"self",4};

struct program {
  void* codes;
  size_t len;
  struct string str_tbl[ AJJ_LOCAL_CONSTANT_SIZE ];
  size_t str_len;
  double num_tbl[AJJ_LOCAL_CONSTANT_SIZE];
  size_t num_len;
  /* parameter prototypes. Program is actually a script based
   * function routine. Each program will have a prototypes */
  struct {
    struct string name;
    struct ajj_value def_val; /* Default value for function parameters.
                               * These values are owned by the scope that owns
                               * this template object. It is typically global
                               * scope */
  } par_list[ AJJ_FUNC_PAR_MAX_SIZE ];
  size_t par_size;
};

static inline
void program_init( struct program* prg ) {
  prg->len = 0;
  prg->str_len = 0;
  prg->num_len = 0;
  prg->par_size =0;
}

static inline
int program_add_par( struct program* prg ,
    struct string name ,
    int own,
    const struct ajj_value* val ) {
  if( prg->par_size == AJJ_FUNC_PAR_MAX_SIZE )
    return -1;
  else {
    assert(name.len < AJJ_SYMBOL_NAME_MAX_SIZE );
    prg->par_list[prg->par_size].def_val = *val; /* owned the value */
    prg->par_list[prg->par_size].name = own ? name : string_dup(&name);
    ++prg->par_size;
    return 0;
  }
}

/* emitter for byte codes */
struct emitter {
  struct program* prg;
  size_t cd_cap;
};


static inline
void reserve_code_page( struct emitter* em , size_t cap ) {
  void* nc;
  assert( em->cd_cap < cap );
  if( em->prg->len == 0 ) {
    /* for the first time, we allocate a large code page */
    cap = MINIMUM_CODE_PAGE_SIZE;
  }
  nc = malloc(cap);
  if( em->prg->codes ) {
    memcpy(nc,em->prg->codes,em->prg->len);
    free(em->prg->codes);
  }
  em->cd_cap = cap;
}

static inline
void emit0( struct emitter* em , int bc ) {
  if( em->cd_cap <= em->prg->len + 9 ) {
    reserve_code_page(em,2*em->cd_cap);
  }
  *((unsigned char*)(em->prg->codes) + em->prg->len)
    = (unsigned char)(bc);
  ++(em->prg->len);
}

static inline
void emit_int( struct emitter* em , int arg ) {
  int l;
  assert( em->cd_cap > em->prg->len + 4 ); /* ensure we have enough space to run */
  l = (em->prg->len) & 4;
  switch(l) {
    case 0:
      *((int*)(em->prg->codes) + em->prg->len) = arg;
      break;
    case 1:
      {
        int lower = (arg & 0xff000000) >> 24;
        int higher= (arg & 0x00ffffff);
        *((unsigned char*)(em->prg->codes) + em->prg->len)
          = (unsigned char)lower;
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len+1))
          = higher;
        break;
      }
    case 2:
      {
        int lower = (arg & 0xffff0000) >> 16;
        int higher= (arg & 0x0000ffff);
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len))
          = lower;
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len+2))
          = higher;
        break;
      }
    case 3:
      {
        int lower = (arg & 0xffffff00) >> 8;
        int higher= (arg & 0x000000ff);
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len))
          = lower;
        *((int*)((unsigned char*)(em->prg->codes) + em->prg->len+3))
          = higher;
        break;
      }
    default:
      UNREACHABLE();
      break;
  }
  em->prg->len += 4;
}

static inline
void emit1( struct emitter* em , int bc , int a1 ) {
  emit0(em,bc);
  emit_int(em,a1);
}

static inline
void emit2( struct emitter* em , int bc , int a1 , int a2 ) {
  emit1(em,bc,a1);
  emit_int(em,a2);
}

static inline
int emitter_put( struct emitter* em , int arg_sz ) {
  int ret;
  size_t add;
  assert( arg_sz == 0 || arg_sz == 1 || arg_sz == 2 );
  ret = em->prg->len;
  add = arg_sz * 4 + 1;
  if( em->cd_cap <= em->prg->len + add ) {
    reserve_code_page(em,em->cd_cap * 2 );
  }
  em->prg->len += add;
  return ret;
}

static inline
void emit0_at( struct emitter* em , int pos , int bc ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emit0(em,bc);
  em->prg->len = save;
}

static inline
void emit1_at( struct emitter* em , int pos , int bc , int a1 ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emit1(em,bc,a1);
  em->prg->len = save;
}

static inline
void emit2_at( struct emitter* em , int pos , int bc , int a1 , int a2 ) {
  int save = em->prg->len;
  em->prg->len = pos;
  emit2(em,bc,a1,a2);
  em->prg->len = save;
}

static inline
int emitter_label( struct emitter* em ) {
  return (int)(em->prg->len);
}

int vm_run( struct ajj* , /* ajj environment */
            struct ajj_object* , /* scope */
            const struct program* , /* program to be executed */
            ajj_value* output ); /* output for this program */

/* Dump an program to output */
void vm_dump( const struct program* prg, FILE* output );


/* ===============================
 * Parser
 * ==============================*/
int parse( struct ajj* , struct gc_scope* ,
    const char* src, const char* key, ajj_value* output );

/* =================================
 * Json
 * ================================*/

/* Parse a json document into a OBJECT object. The json and ajj object
 * has a 1:1 mapping. The internal mapping is as follow :
 * json:string --> string
 * json:null   --> None
 * json:boolean--> boolean
 * json:number --> number
 * json:list   --> list
 * json:object --> dictionary */

int json_decode ( struct ajj* , struct gc_scope* ,
    const char* , struct ajj_value* );

/* Serialize the object to a json document insides of struct strbuf.
 * User need to initialize output strbuf object before calling it */
int json_encode ( const struct ajj_value* , struct strbuf* );

/* ===============================
 * Object representation
 * =============================*/

struct c_closure {
  void* udata; /* user data */
  ajj_method func; /* function */
};

static inline
void c_closure_init( struct c_closure* cc ) {
  cc->udata = NULL;
  cc->func = NULL;
}

enum {
  C_FUNCTION,
  C_METHOD,
  JJ_BLOCK,
  JJ_MACRO
};

struct function {
  union {
    struct c_closure c_fn; /* C function */
    ajj_method c_mt; /* C method */
    struct program jj_fn; /* JJ_BLOCK/JJ_MACRO */
  } f;
  struct string name;
  int tp;
};

struct func_table {
  struct function func_buf[ AJJ_FUNC_LOCAL_BUF_SIZE ];
  struct function* func_tb; /* function table */
  size_t func_len;
  size_t func_cap;
  ajj_class_dtor dtor;
  ajj_class ctor ctor;
  void* udata;
  struct string name; /* object's name */
};

/* This function will initialize an existed function table */
static inline
void func_table_init( struct func_table* tb ,
    const struct ajj_dtor* dtor ,
    struct string* name , int own ) {
  tb->func_tb = tb->func_buf;
  tb->func_len = 0;
  tb->func_cap = AJJ_FUNC_LOCAL_BUF_SIZE;
  tb->dtor = dtor ;
  tb->name = own ? *name : string_dup(name);
}

/* Clear the GUT of func_table object */
static inline
void func_table_clear( struct func_table* tb ) {
  if( tb->func_cap > AJJ_FUNC_LOCAL_BUF_SIZE ) {
    free(tb->func_tb); /* on heap */
  }
  string_destroy(&(tb->name));
}

/* Add a new function into the func_table */
static inline
struct function* func_table_add_func( struct func_table* tb ) {
  if( tb->func_len == tb->func_cap ) {
   void* nf = malloc( sizeof(struct function)*(tb->func_cap)*2 );
   memcpy(nf,tb->func_tb,tb->func_len*sizeof(struct function));
   if( tb->func_tb != tb->func_buf ) {
     free(tb->func_tb);
   }
   tb->func_tb = nf;
   tb->func_cap *= 2;
  }
  return tb->func_tb + (tb->func_len++);
}

static inline
void func_table_shrink_to_fit( struct func_table* tb ) {
  if( tb->func_cap > AJJ_FUNC_LOCAL_BUF_SIZE ) {
    if( tb->func_len < tb->func_cap ) {
      struct function* f = malloc(tb->func_len*sizeof(struct function));
      memcpy(f,tb->func_tb,sizeof(struct function)*tb->func_len);
      free(tb->func_tb);
      tb->func_tb = f;
      tb->func_cap = func_len;
    }
  }
}

/* Find a new function in func_table */
static
struct function* func_table_find_func( struct func_table* tb,
    const struct string* name );

/* Get function from func table */
static
struct function* func_table_get_func( const struct func_table* tb,
    const struct string* name );

static inline
struct c_closure*
func_table_add_c_clsoure( struct func_table* tb , struct string* name , int own ) {
  struct function* f = func_table_get_func(tb,name);
  if( f != NULL )
    return NULL;
  f = func_table_add_func(tb);
  f->name = own ? *name : string_dup(name);
  f->tp = C_FUNCTION;
  c_closure_init(&(f->f,c_fn));
  return &(f->f.c_fn);
}

static inline
ajj_method*
func_table_add_c_method( struct func_table* tb , struct string* name , int own ) {
  struct function* f = func_table_get_func(tb,name);
  if( f != NULL )
    return NULL;
  f = func_table_add_func(tb);
  f->name = own ? *name : string_dup(name);
  f->tp = C_METHOD;
  return &(f->f.c_mt);
}

static inline
struct program*
func_table_add_jj_block( struct func_table* tb, struct string* name , int own ) {
  struct function* f = func_table_get_func(tb,name);
  if( f != NULL )
    return NULL;
  f = func_table_add_func(tb);
  f->name = own ? *name : string_dup(name);
  f->tp = JJ_BLOCK;
  program_init(&(f->f.jj_fn));
  return &(f->f.jj_fn);
}

static inline
struct program*
func_table_add_jj_macro( struct func_table* tb, struct string* name , int own ) {
  struct function* f = func_table_get_func(tb,name);
  if( f != NULL )
    return NULL;
  f = func_table_add_func(tb);
  f->name = own ? *name : string_dup(name);
  f->tp = JJ_MACRO;
  program_init(&(f->f.jj_fn));
  return &(f->f.jj_fn);
}

struct object {
  struct dict prop; /* properties of this objects */
  const struct func_table* fn_tb; /* This function table can be NULL which
                                   * simply represents this object doesn't have
                                   * any defined function related to it */
  void* data; /* object's data */
};

/* Create an object object */
static inline
void object_create( struct object* obj ,
    const struct func_table* func_tb, void* data ) {
  dict_create(&(obj->prop));
  obj->fn_tb = func_tb;
  obj->data = data;
}

struct ajj_object {
  struct ajj_object* prev;
  struct ajj_object* next;
  struct ajj_object* parent[AJJ_EXTENDS_MAX_SIZE]; /* for extends */
  unsigned short parent_len;
  unsigned short tp;
  union {
    struct string str; /* string */
    struct dict d;     /* dictionary */
    struct list l;     /* list */
    struct object obj; /* object */
  } val;
  struct gc_scope* scp;
};

/* Create a single ajj_object which is NOT INITIALZIED with any type
 * under the scope object "scope" */
static inline
struct ajj_object*
ajj_object_create ( struct ajj* , struct gc_scope* scope );

static inline
struct ajj_object*
ajj_object_create_child( struct ajj* , struct ajj_object* obj ) {
  return ajj_object_create(ajj,obj->scp);
}

/* Delete routine. Delete a single object from its GC. This deletion
 * function will only delete the corresponding object , not its children,
 * it is not safe to delete children, since the parent object doesn't
 * really have the ownership */
static inline
void ajj_object_destroy( struct ajj* , struct ajj_object* obj );

/* Initialize an created ajj_object to a certain type */
static inline
void ajj_object_string( struct ajj_object* obj,
    const char* str , size_t len , int own ) {
  obj->val.str.str = own ? str : strdup(str);
  obj->val.str.len = len;
  obj->tp = AJJ_VALUE_STRING;
}

static inline
struct ajj_object*
ajj_object_create_string( struct ajj* a, struct gc_scope* scp,
    const char* str, size_t len , int own ) {
  return ajj_object_string( ajj_object_create(a,scp),
      str,len,own);
}

static inline
void ajj_object_dict( struct ajj_object* obj ) {
  dict_create(&(obj->val.d));
  obj->tp = AJJ_VALUE_DICT;
}

static inline
struct ajj_object*
ajj_object_create_dict( struct ajj* a, struct gc_scope* scp ) {
  return ajj_object_dict(ajj_object_create(a,scp));
}

static inline
void ajj_object_list( struct ajj_object* obj ) {
  list_create(&(obj->val.l));
  obj->tp = AJJ_VALUE_LIST;
}

static inline
struct ajj_object*
ajj_object_create_list( struct ajj* a, struct gc_scope* scp ) {
  return ajj_object_list(ajj_object_create(a,scp));
}

static inline
void ajj_object_obj( struct ajj_object* obj ,
    const struct func_table* fn_tb, void* data ) {
  object_create(&(obj_val.o),fn_tb,data);
  obj->tp = AJJ_VALUE_OBJECT;
}

static inline
struct ajj_object*
ajj_object_create_obj( struct ajj* a, struct gc_scope* scp ) {
  return ajj_object_obj(ajj_object_create(a,scp));
}

/* ===================================================
 * Value wrapper for internal use
 * =================================================*/

static inline
void ajj_value_destroy( struct ajj* a , struct ajj_value* val ) {
  assert(val->type != AJJ_VALUE_NOT_USE);
  if( val->type != AJJ_VALUE_BOOLEAN &&
      val->type != AJJ_VALUE_NONE &&
      val->type != AJJ_VALUE_NUMBER )
    ajj_object_destroy(a,val->value.object);
}

static inline
const struct string* ajj_value_to_string( const struct ajj_value* val ) {
  assert(val->type == AJJ_VALUE_STRING);
  return &(val->value.object->val.str);
}


static inline
struct ajj_value ajj_value_assign( struct ajj_object* obj ) {
  struct ajj_value val;
  assert(obj->tp != AJJ_VALUE_NOT_USE);
  val.type = obj->tp;
  val.value.object = obj;
  return val;
}

/* ================================
 * GC Scope
 * ===============================*/
struct gc_scope {
  struct ajj_object gc_tail; /* tail of the GC objects list */
  struct gc_scope* parent;   /* parent scope */
  unsigned int scp_id;       /* scope id */
};

static inline
struct gc_scope* gc_scope_create( struct ajj*, struct gc_scope* scp );

/* This function will destroy all the gc scope allocated memory and also
 * the gc_scope object itself */
static
void gc_scope_destroy( struct ajj* , struct gc_scope* );


/* =================================
 * AJJ
 * ===============================*/
struct ajj {
  struct slab obj_slab; /* object slab */
  struct slab gc_slab;  /* gc_scope slab */
  struct gc_scope gc_root; /* root of the gc scope */
};

static inline
struct gc_scope* gc_root( struct ajj* a ) {
  return &(a->gc_root);
}


/* ==========================================
 * INLINE implementation
 * =========================================*/
static inline
struct gc_scope* gc_scope_create( struct ajj* a, struct gc_scope* scp ) {
  struct gc_scope* new_scp = slab_malloc(&(a->gc_slb));
  new_scp->parent = scp;
  new_scp->scp_id = scp->scp_id+1;
  LINIT(&(new_scp->gc_tail));
  return new_scope;
}

static inline
struct ajj_object*
ajj_object_create( struct ajj* a , struct gc_scope* scp ) {
  struct ajj_object* obj = slab_malloc(&(a->obj_slab));
  /* linked the new object into the gc_list */
  LINSERT(obj,&(scp->gc_tail));
  obj->tp = AJJ_VALUE_NOT_USE; /* invalid object that is not used */
  return obj;
}

#endif /* _AJJ_PRIV_H_ */
