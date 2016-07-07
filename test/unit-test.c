#include <vm.h>
#include <parse.h>
#include <lex.h>
#include <ajj-priv.h>
#include <object.h>
#include <bc.h>
#include <opt.h>
#include <util.h>
#include <builtin.h>
#include <stdlib.h>
#include <sys/time.h>
#include <inttypes.h>
#include <dirent.h>

#ifdef NDEBUG
#include <stdlib.h>
#include <stdio.h>
#undef assert
#define assert(X) \
  do { \
    if(!(X)) { \
      fprintf(stderr,"\n%s\n","Assertion:"#X); \
      abort(); \
    } \
  } while(0)
#else
#include <assert.h>
#endif /* NDEBUG */

#define STRINGIFY(...) #__VA_ARGS__


/* ===============================================================
 * Byte Code
 * =============================================================*/

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
void bc_test_main() {
    struct emitter em;
    struct program prg;
    int instr;
    size_t i = 0;

    program_init(&prg);
    emitter_init(&em,&prg);

    assert(prg.len==0);

    EMIT0(VM_ADD);
    EMIT0(VM_MUL);
    EMIT0(VM_SUB);
    EMIT0(VM_DIV);
    EMIT1(VM_TPUSH,1);
    EMIT1(VM_BPUSH,2);
    EMIT2(VM_CALL,1,2);

    instr = bc_next(&prg,&i);
    assert(bc_instr(instr) == VM_ADD);

    instr = bc_next(&prg,&i);
    assert(bc_instr(instr) == VM_MUL);

    instr = bc_next(&prg,&i);
    assert(bc_instr(instr) == VM_SUB);

    instr = bc_next(&prg,&i);
    assert(bc_instr(instr) == VM_DIV);

    instr = bc_next(&prg,&i);
    assert(bc_instr(instr) == VM_TPUSH);
    assert(bc_1st_arg(instr) == 1);

    instr = bc_next(&prg,&i);
    assert(bc_instr(instr) == VM_BPUSH);
    assert(bc_1st_arg(instr) == 2);

    instr = bc_next(&prg,&i);
    assert(bc_instr(instr) == VM_CALL);
    assert(bc_1st_arg(instr)==1);
    assert(bc_2nd_arg(&prg,&i)==2);
}
#undef EMIT0
#undef EMIT1
#undef EMIT2
#undef EMIT0_AT
#undef EMIT1_AT
#undef EMIT2_AT


/* =========================================================
 * Util
 * =======================================================*/

struct list_node {
  struct list_node* prev;
  struct list_node* next;
  int value;
};

struct list {
  struct list_node end;
  size_t len;
};

static void list_init( struct list* l ) {
  l->len = 0;
  LINIT(&(l->end));
}

static void list_push( struct list* l , int val ) {
  struct list_node* n = malloc(sizeof(*n));
  n->value = val;
  LINSERT(n,&(l->end));
  ++l->len;
}

static void list_remove( struct list* l , struct list_node* n ) {
  LREMOVE(n);
  free(n);
  --l->len;
}

static void list_clear( struct list* l ) {
  struct list_node* n = l->end.next;
  struct list_node* t;
  while(n != &(l->end)) {
    t = n->next;
    free(n);
    n = t;
  }
}

static
void test_list_macro() {
  int a[12];
  assert( ARRAY_SIZE(a) == 12 );

  {
    struct list l;
    struct list_node* n;
    int i;
    list_init(&l);
    for(i = 0;i< 10; ++i) {
      list_push(&l,i);
    }
    /* travel through the list */
    n = l.end.next;
    i = 0;
    while( n != &(l.end) ) {
      assert(n->value == i);
      ++i;
      n = n->next;
    }

    /* remove some nodes */
    n = l.end.next;
    i = 0;
    while( n != &(l.end) ) {
      if( i % 2 == 0 ) {
        struct list_node* nd;
        nd = n->next;
        list_remove(&l,n);
        n = nd;
      } else {
        n = n->next;
      }
      ++i;
    }
    assert(i == 10);
    n = l.end.next;
    i = 0;
    while( n != &(l.end) ) {
      assert(n->value == i+1);
      i += 2;
      n = n->next;
    }
    list_clear(&l);
  }
}

static
void test_string() {
  assert( string_null(&NULL_STRING) );
  assert( string_eqc(&TRUE_STRING,"True") );
  assert( string_eqc(&FALSE_STRING,"False") );
  assert( string_eqc(&NONE_STRING,"None") );
  {
    struct string Dup1 = string_dup(&TRUE_STRING);
    assert( string_eq(&Dup1,&TRUE_STRING) );
    string_destroy(&Dup1);
    Dup1 = string_dupc("True");
    assert( string_eq(&Dup1,&TRUE_STRING) );
    string_destroy(&Dup1);
  }
}

#define STRBUF_INIT_SIZE INITIAL_MEMORY_SIZE

static
void test_strbuf() {
  {
    struct strbuf b1;
    int i;
    strbuf_init(&b1); /* initialize the buffer */
    assert( b1.len == 0 );
    for( i = 0 ; i < 8; ++i ) {
      strbuf_push_rune(&b1,'c');
    }
    assert( b1.len == 8 );
    assert( b1.cap > 8 );
    assert( strcmp(strbuf_tostring(&b1).str,"cccccccc") == 0 );
    strbuf_append(&b1,"aaaa",4);
    assert( strcmp(strbuf_tostring(&b1).str,"ccccccccaaaa")==0);
    strbuf_reset(&b1);
    assert( b1.len == 0 );
    assert( b1.cap > 0  );
    strbuf_destroy(&b1);
    assert( b1.len == 0 );
    assert( b1.cap == 0 );
  }
  {
    struct strbuf b1;
    int i;
    strbuf_init(&b1);
    for( i = 0 ; i < 256; ++i )
      strbuf_push_rune(&b1,'c');
    assert(b1.len == 256);
    assert(b1.cap > 256);
    for( i = 0 ; i < 256; ++i )
      assert(b1.str[i] == 'c');
    assert(b1.str[256] == 0);
    strbuf_destroy(&b1);
  }
  {
    struct strbuf b1;
    struct string str;
    int i;

    strbuf_init(&b1);
    strbuf_append(&b1,"abcd",4); /* short string */
    strbuf_move(&b1,&str);
    assert(str.len == 4);
    assert(string_eqc(&str,"abcd"));
    assert(b1.cap == STRBUF_INIT_SIZE);
    assert(b1.len == 0);
    assert(strcmp(b1.str,"abcd")==0); /* undefined behavior */
    string_destroy(&str);

    for( i = 0 ; i < STRBUF_INIT_SIZE/2+1; ++i ) {
      strbuf_push_rune(&b1,'d');
    }
    assert(b1.len == STRBUF_INIT_SIZE/2+1);
    assert(b1.cap == STRBUF_INIT_SIZE);

    strbuf_move(&b1,&str);
    assert(str.len == STRBUF_INIT_SIZE/2+1);
    for( i = 0 ; i < STRBUF_INIT_SIZE/2+1; ++i ) {
      assert(str.str[i] == 'd');
    }
    assert(str.str[i]==0);
    assert(b1.cap == 0);
    assert(b1.len == 0);
    assert(b1.str == NULL);

    /* Adding a large string larger than the threshold */
    for( i = 0 ; i < STRBUF_MOVE_THRESHOLD*2 ; ++i ) {
      strbuf_push_rune(&b1,'c');
    }
    assert(b1.cap > STRBUF_MOVE_THRESHOLD);
    assert(b1.len == STRBUF_MOVE_THRESHOLD*2);
    string_destroy(&str);

    strbuf_move(&b1,&str);
    assert(str.len == STRBUF_MOVE_THRESHOLD*2);
    for( i = 0 ; i < STRBUF_MOVE_THRESHOLD*2 ; ++i ) {
      assert(str.str[i]=='c');
    }
    assert(str.str[i]==0);
    strbuf_destroy(&b1); /* should be dummy */
    string_destroy(&str);
  }
}

/* Collisioned hash key:
 * when cap is 4.
 * ABCDE --> 0
 * BBCCD --> 2
 * UUVVX --> 2
 * Zvbcc --> 0
 */

void test_map() {
  {
    /* testing collision hash key */
    struct map d;
    int val;
    void* find;
    int itr;
    map_create(&d,sizeof(int),4);
    assert(d.cap == 4);
    assert(d.len == 0);
    assert(d.obj_sz == sizeof(int));
    assert(d.value!=NULL);
    assert(d.entry!=NULL);
    val=1;
    assert(!map_insert_c(&d,"ABCDE",&val));
    val=2;
    assert(!map_insert_c(&d,"BBCCD",&val));
    val=3;
    assert(!map_insert_c(&d,"UUVVX",&val));
    val=4;
    assert(!map_insert_c(&d,"Zvbcc",&val));

    assert(d.cap ==4);
    assert(d.use ==4);

    assert((find=map_find_c(&d,"ABCDE")));
    assert(*(int*)(find)==1);

    assert((find=map_find_c(&d,"BBCCD")));
    assert(*(int*)(find)==2);

    assert((find=map_find_c(&d,"UUVVX")));
    assert(*(int*)(find)==3);

    assert((find=map_find_c(&d,"Zvbcc")));
    assert(*(int*)(find)==4);

    itr = map_iter_start(&d);
    while( map_iter_has(&d,itr) ) {
      struct map_pair entry =
        map_iter_deref(&d,itr);
      if(strcmp(entry.key->str,"ABCDE")==0)
        assert(*(int*)(entry.val)==1);
      else if( strcmp(entry.key->str,"BBCCD")==0)
        assert(*(int*)(entry.val)==2);
      else if( strcmp(entry.key->str,"UUVVX")==0)
        assert(*(int*)(entry.val)==3);
      else if( strcmp(entry.key->str,"Zvbcc")==0)
        assert(*(int*)(entry.val)==4);
      else
        assert(0);
      itr = map_iter_move(&d,itr);
    }
    map_clear(&d);
    assert(d.use==0);
    assert(d.cap==4);
    map_destroy(&d);
    assert(d.use==0);
    assert(d.cap==0);
  }
  {
    /* trigger rehashing here */
    struct map d;
    int i;
    map_create(&d,sizeof(int),4);
    for( i = 0 ; i < 128 ; ++i ) {
      char name[1024];
      struct string k;
      sprintf(name,"MyNameIs:%d",i);
      k = string_dupc(name);
      assert(!map_insert(&d,&k,1,&i));
    }
    for( i = 0 ; i < 128 ; ++i ) {
      char name[1024];
      void* ptr;
      sprintf(name,"MyNameIs:%d",i);
      assert((ptr=map_find_c(&d,name)));
      assert(*(int*)(ptr)==i);
    }
    assert(d.use==128);
    assert(d.cap==128);
    for( i = 0 ; i < 128 ; ++i ) {
      char name[1024];
      int val;
      sprintf(name,"MyNameIs:%d",i);
      assert(!map_remove_c(&d,name,&val));
      assert(val==i);
    }
    assert(d.use==0);
    assert(d.cap==128);

    for( i = 0 ; i < 128 ; ++i ) {
      char name[1024];
      void* ptr;
      sprintf(name,"MyNameIs:%d",i);
      assert((ptr=map_find_c(&d,name))==NULL);
    }

    map_clear(&d);
    assert(d.use==0);
    map_destroy(&d);
  }
}

static
void test_slab() {
  {
    struct slab slb;
    struct map m;
    int i;

    slab_init(&slb,128,sizeof(int));
    map_create(&m,sizeof(int*),4);

    for( i = 0 ; i < 1024 ; ++i ) {
      char name[1024];
      int* ptr;
      sprintf(name,"HelloWorld:%d",i);
      ptr = slab_malloc(&slb);
      *ptr = i;
      assert(!map_insert_c(&m,name,&ptr));
    }
    assert(m.use==1024);
    assert(m.cap==1024);

    for( i = 0 ; i < 1024; ++i ) {
      char name[1024];
      int** ptr;
      sprintf(name,"HelloWorld:%d",i);
      ptr = map_find_c(&m,name);
      assert(**ptr == i);
    }

    for( i = 0 ; i < 512 ; ++i ) {
      char name[1024];
      int* ptr = NULL;
      sprintf(name,"HelloWorld:%d",i);
      assert(!map_remove_c(&m,name,&ptr));
      slab_free(&slb,ptr);
    }

    for( i = 512 ; i < 1024; ++i ) {
      char name[1024];
      int** ptr;
      sprintf(name,"HelloWorld:%d",i);
      ptr = map_find_c(&m,name);
      assert(**ptr == i);
    }

    for( i = 512 ; i < 1024; ++i ) {
      char name[1024];
      int* ptr;
      sprintf(name,"HelloWorld:%d",i);
      assert(!map_remove_c(&m,name,&ptr));
      slab_free(&slb,ptr);
    }

    assert(m.cap==1024);
    assert(m.use==0);

    slab_destroy(&slb);
    map_destroy(&m);
  }
}

void util_test_main() {
  test_list_macro();
  test_string();
  test_strbuf();
  test_map();
  test_slab();
}

/* ====================================================
 * Lexer
 * ==================================================*/

static
void lex_test_basic() {
  {
    struct tokenizer tk;
    const char* source= \
      "ThisIsAText \\{{\\{{\\{ AlsoThisIsAText!" \
      "{{ new.object }}\n" \
      "ASD asd sad" \
      "{% for endfor variable else elif endif macro endmacro macroX %}" \
      "{% call endcall filter endfilter do set endset with endwith "\
      "move block endblock extends import endimport include endinclude in as " \
      "continue break upvalue endupvalue json override optional %}";

    int i;
    token_id token[] = {
      TK_TEXT,
      TK_LEXP,
      TK_VARIABLE,
      TK_DOT,
      TK_VARIABLE,
      TK_REXP,
      TK_TEXT, /* That whitespace */
      TK_LSTMT,
      TK_FOR,
      TK_ENDFOR,
      TK_VARIABLE,
      TK_ELSE,
      TK_ELIF,
      TK_ENDIF,
      TK_MACRO,
      TK_ENDMACRO,
      TK_VARIABLE,
      TK_RSTMT,
      TK_LSTMT,
      TK_CALL,
      TK_ENDCALL,
      TK_FILTER,
      TK_ENDFILTER,
      TK_DO,
      TK_SET,
      TK_ENDSET,
      TK_WITH,
      TK_ENDWITH,
      TK_MOVE,
      TK_BLOCK,
      TK_ENDBLOCK,
      TK_EXTENDS,
      TK_IMPORT,
      TK_ENDIMPORT,
      TK_INCLUDE,
      TK_ENDINCLUDE,
      TK_IN,
      TK_AS,
      TK_CONTINUE,
      TK_BREAK,
      TK_UPVALUE,
      TK_ENDUPVALUE,
      TK_JSON,
      TK_OVERRIDE,
      TK_OPTIONAL,
      TK_RSTMT
    };

    tk_init(&tk,source);
    i = 0;
    while(tk.tk != TK_EOF) {
      assert(tk.tk == token[i]);
      ++i;
      tk_move(&tk);
    }
    tk_destroy(&tk);
  }
  {
    struct tokenizer tk;
    struct string lexeme;
    const char* source = \
      "TextAHahaha{{ a.b }}YouAreCorrect asd!";
    token_id token[] = {
      TK_TEXT,
      TK_LEXP,
      TK_VARIABLE,
      TK_DOT,
      TK_VARIABLE,
      TK_REXP,
      TK_TEXT
    };
    tk_init(&tk,source);
    assert(tk.tk == TK_TEXT);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"TextAHahaha")==0);
    tk_move(&tk);
    assert(tk.tk == TK_LEXP);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"a")==0);
    tk_move(&tk);
    assert(tk.tk == TK_DOT);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"b")==0);
    tk_move(&tk);
    assert(tk.tk == TK_REXP);
    tk_move(&tk);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"YouAreCorrect asd!")==0);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
    tk_destroy(&tk);
  }

  {
    struct tokenizer tk;
    struct string lexeme;
    const char* source = \
      "{{ a }}a b c d{{ a }}";
    token_id token[] = {
      TK_LEXP,
      TK_VARIABLE,
      TK_REXP,
      TK_TEXT,
      TK_LEXP,
      TK_VARIABLE,
      TK_REXP
    };
    tk_init(&tk,source);
    assert(tk.tk == TK_LEXP);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"a")==0);
    tk_move(&tk);
    assert(tk.tk == TK_REXP);
    tk_move(&tk);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"a b c d")==0);
    tk_move(&tk);
    assert(tk.tk == TK_LEXP);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"a")==0);
    tk_move(&tk);
    assert(tk.tk == TK_REXP);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
    tk_destroy(&tk);
  }

  {
    struct tokenizer tk;
    struct string lexeme;
    const char* source = \
      "{{ a }}\na b c d{{ a }}";
    token_id token[] = {
      TK_LEXP,
      TK_VARIABLE,
      TK_REXP,
      TK_TEXT,
      TK_LEXP,
      TK_VARIABLE,
      TK_REXP
    };
    tk_init(&tk,source);
    assert(tk.tk == TK_LEXP);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"a")==0);
    tk_move(&tk);
    assert(tk.tk == TK_REXP);
    tk_move(&tk);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"a b c d")==0);
    tk_move(&tk);
    assert(tk.tk == TK_LEXP);
    tk_move(&tk);
    assert(tk.tk == TK_VARIABLE);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"a")==0);
    tk_move(&tk);
    assert(tk.tk == TK_REXP);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
    tk_destroy(&tk);
  }

  {
    struct tokenizer tk;
    const char* source = \
      "{% raw %} Hi {% HelloWorld %} {% endraw %}" \
      "{% +-*/ // /// % true false 1234.56 \'helloworld\' %}";
    token_id token[] = {
      TK_TEXT,
      TK_LSTMT,
      TK_ADD,
      TK_SUB,
      TK_MUL,
      TK_DIV,
      TK_DIVTRUCT,
      TK_DIVTRUCT,
      TK_DIV,
      TK_MOD,
      TK_TRUE,
      TK_FALSE,
      TK_NUMBER,
      TK_STRING,
      TK_RSTMT
    };
    int i;

    tk_init(&tk,source);
    i = 0;
    while(tk.tk != TK_EOF) {
      assert(tk.tk == token[i]);
      tk_move(&tk);
      ++i;
    }
    tk_destroy(&tk);
  }
  {
    struct tokenizer tk;
    const char* source = \
      "{% {}()[].**and or not |,:None true True False false none !=" \
      " <= >= <> == % %}";
    token_id token[] = {
      TK_LSTMT,
      TK_LBRA,
      TK_RBRA,
      TK_LPAR,
      TK_RPAR,
      TK_LSQR,
      TK_RSQR,
      TK_DOT,
      TK_POW,
      TK_AND,
      TK_OR,
      TK_NOT,
      TK_PIPE,
      TK_COMMA,
      TK_COLON,
      TK_NONE,
      TK_TRUE,
      TK_TRUE,
      TK_FALSE,
      TK_FALSE,
      TK_NONE,
      TK_NE,
      TK_LE,
      TK_GE,
      TK_LT,
      TK_GT,
      TK_EQ,
      TK_MOD,
      TK_RSTMT
    };
    int i;

    i = 0;
    tk_init(&tk,source);
    while( tk.tk != TK_EOF ) {
      assert(tk.tk == token[i]);
      tk_move(&tk);
      ++i;
    }
    tk_destroy(&tk);
  }
  {
    struct tokenizer tk;
    struct string lexeme;
    const char* source = \
      "{% 1.2345 123 'Hello\\\'World' True true false False None none %}";

    tk_init(&tk,source);
    assert(tk.tk == TK_LSTMT);
    tk_move(&tk);

    assert(tk.tk == TK_NUMBER);
    assert(tk.num_lexeme == 1.2345);
    tk_move(&tk);

    assert(tk.tk == TK_NUMBER);
    assert(tk.num_lexeme == 123);
    tk_move(&tk);

    assert(tk.tk == TK_STRING);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"Hello'World")==0);
    tk_move(&tk);

    assert(tk.tk == TK_TRUE);
    tk_move(&tk);

    assert(tk.tk == TK_TRUE);
    tk_move(&tk);

    assert(tk.tk == TK_FALSE);
    tk_move(&tk);

    assert(tk.tk == TK_FALSE);
    tk_move(&tk);

    assert(tk.tk == TK_NONE);
    tk_move(&tk);

    assert(tk.tk == TK_NONE);
    tk_move(&tk);

    assert(tk.tk == TK_RSTMT);
    tk_move(&tk);

    assert(tk.tk == TK_EOF);

    tk_destroy(&tk);
  }
  {
    const char* source = \
      "{% 1.2345 123 \'Hello World\' True true FFFFF %}";
    char buf[CODE_SNIPPET_SIZE];
    tk_get_code_snippet(source,4,buf,256);
    printf("%s\n",buf);
    tk_get_code_snippet(source,22,buf,256);
    printf("%s\n",buf);
  }
  {
    struct tokenizer tk;
    struct string lexeme;
    const char* source = \
      "{% raw %} {% Hello World %} {% for } foreachshit {%} %} }%{% {% endraw %}";
    tk_init(&tk,source);
    assert(tk.tk == TK_TEXT);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,
          " {% Hello World %} {% for } foreachshit {%} %} }%{% ")==0);
    tk_destroy(&tk);
  }
}

/* Test the whitespace control for AJJ :
 *
 * The script instruction has 3 type , ie :
 * 1) {% %} statement
 * 2) {{ }} expression
 * 3) {# #} comment
 *
 * These 3 types are not the normal type of text. In AJJ,
 * for simplicity, the whitespace control are applied automatically.
 *
 * These 3 types of instruction are have *SAME* whitespace remove
 * rules
 *
 * ===========================================================
 *
 * The rules are as follow:
 * 1. Any leading whitespace that resides at the same line of the
 * statement will be removed , if no text resides at the same line.
 *
 * 2. Any trailing whitespace that resides at the same line of the
 * statment will be removed , if no text resides at the same line.
 * Additionally , if the trailing whitespace line follows by another
 * statement, then the last line break will be removed. Eg:
 * This is a text \n
 *     {% statement %}
 *
 * Since the line includes statement's leading whitespace is effectively
 * the trailing whitespace for the text. So the last line break which
 * is the line break that after "text" will be removed. NOTES: only this
 * line break will be removed, the extra space before this line break
 * will not be removed.
 *
 * Some example:
 *
 * 1) Text resides at the same line of statement
 *
 * Text {% some_statement %}  ( Suppose statement will not output
 * any text ) . For this case, since text resides at the same line,
 * no whitespace will be removed, therefore it outputs Text , pay
 * attention to that extra space following "Text".
 * Same as {% some_statement %} Text , which the trailing text resides
 * at the same line. So the extra space before "Text" won't be removed.
 *
 * 2) Text not resides at the same line of statement.
 *
 * Text\n
 *  {% some_statement %}
 *
 * In this case, since Text is not resides at the same line of the
 * statement, so the leading whitespace of {% some_statement %} will
 * be removed.
 *
 * Same here : {% some_statement %}   \n
 *             Text
 *
 * In this case, the trailing whitespace of some_statement will be
 * removed.
 *
 * ============================================================
 */


static void lex_test_ws_case( const char* L , const char* R ,
    const char* Text , const char* Exp ,
    token_id LT , token_id RT ) {
  struct tokenizer tk;
  struct string lexeme;
  char source[1024];
  sprintf(source,"%s %s%s%s %s",L,R,Text,L,R);
  tk_init(&tk,source);
  assert(tk.tk == LT);
  tk_move(&tk);
  assert(tk.tk == RT);
  tk_move(&tk);
  assert(tk.tk == TK_TEXT);
  lexeme = strbuf_tostring(&(tk.lexeme));
  assert(string_cmpc(&lexeme,Exp)==0);
  tk_move(&tk);
  assert(tk.tk == LT);
  tk_move(&tk);
  assert(tk.tk == RT);
  tk_move(&tk);
  assert(tk.tk == TK_EOF);
  tk_destroy(&tk);
}

static void lex_test_ws() {
  lex_test_ws_case("{%","%}"," A Test "," A Test ",
      TK_LSTMT,TK_RSTMT);
  lex_test_ws_case("{%","%}","  \n A Test \n "," A Test ",
      TK_LSTMT,TK_RSTMT);
  lex_test_ws_case("{%","%}"," \n A Test \n Here "," A Test \n Here ",
      TK_LSTMT,TK_RSTMT);
  lex_test_ws_case("{%","%}"," Here \n A Test \n "," Here \n A Test ",
      TK_LSTMT,TK_RSTMT);


  lex_test_ws_case("{{","}}"," A Test "," A Test ",
      TK_LEXP,TK_REXP);
  lex_test_ws_case("{{","}}"," \n A Test \n "," A Test ",
      TK_LEXP,TK_REXP);
  lex_test_ws_case("{{","}}"," \n A Test \n Here "," A Test \n Here ",
      TK_LEXP,TK_REXP);
  lex_test_ws_case("{{","}}"," Here \n A Test \n "," Here \n A Test ",
      TK_LEXP,TK_REXP);


  { /* text on different line */
    struct tokenizer tk;
    struct string lexeme;
    const char* source =  "This Text {# comment #} Another Text";
    tk_init(&tk,source);
    assert(tk.tk == TK_TEXT);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"This Text  Another Text")==0);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
  }

  { /* text on different line */
    struct tokenizer tk;
    struct string lexeme;
    const char* source =  "This Text \n  {# comment #}   \n Another Text";
    tk_init(&tk,source);
    assert(tk.tk == TK_TEXT);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"This Text  Another Text")==0);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
  }

  { /* text on different line */
    struct tokenizer tk;
    struct string lexeme;
    const char* source =  "This Text {# comment #}   \n Another Text";
    tk_init(&tk,source);
    assert(tk.tk == TK_TEXT);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"This Text  Another Text")==0);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
  }

  { /* text on different line */
    struct tokenizer tk;
    struct string lexeme;
    const char* source =  "This Text \n   {# comment #} Another Text";
    tk_init(&tk,source);
    assert(tk.tk == TK_TEXT);
    lexeme = strbuf_tostring(&(tk.lexeme));
    assert(string_cmpc(&lexeme,"This Text  Another Text")==0);
    tk_move(&tk);
    assert(tk.tk == TK_EOF);
  }
}

void lex_test_main() {
  lex_test_basic();
  lex_test_ws();
}

/* =============================================
 * Parser
 * ===========================================*/

#define PARSER_TEST_CHECK(X) \
  do { \
    if(pos) { \
      if(!(X)) { \
        fprintf(stderr,"%s",ajj_last_error(a)); \
        abort(); \
      } \
    } else { \
      if(!(X)) { \
        fprintf(stderr,"%s",ajj_last_error(a)); \
        abort(); \
      } \
    } \
  } while(0)

static
void parser_test( const char* src , int pos , int dump ) {
  struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
  struct ajj_object* obj = parse(a,"HelloWorld",src,0,0);
  const struct program* prg;

  (void)dump;

  PARSER_TEST_CHECK(obj);
  prg = ajj_object_jinja_main(obj);
  PARSER_TEST_CHECK(prg);
}

/* ALL these tests requires human interaction to look into the code
 * generated and verify they are correct or not. We may record the
 * correct code and assert on them in future for regression test */

static
void parser_basic() {
  const char* src = "UUVVXX{{ a.b }}XX";
  parser_test(src,1,0);
}

static
void parser_for() {
  {
    /* FOR LOOP */
    const char* src = "UUXX{% for key,value in my_dictionary if key == None %}\n" \
                       "<dt>{{ keyvalue | filter1 }}</dt>\n" \
                       "<dd>{{ keyvalue | filter2 }}</dt>\n" \
                       "<dd>{{ key }} {{ value }}</dt>\n"\
                      "{% else %}\n" \
                      "{{ key | filter3 | filter4 }}{{ val }}\n"\
                      "{% endfor %}";
    parser_test(src,1,0);
  }

  {
    /* FOR LOOP */
    const char* src = "UVWXYZ{% for a,_ in my_dict if a != True %}\n" \
                       "<dt>{{ KeyValue | filter1 }}</dt>\n" \
                       "<dt>{{ a | lower_case }}</dt>\n" \
                       "{% endfor %}";
    parser_test(src,1,0);
  }

  {
    /* FOR LOOP */
    const char* src = "UVWXYZ{% for _,a in my_dict if a != True %}\n" \
                       "<dt>{{ KeyValue | filter1 }}</dt>\n" \
                       "<dt>{{ a | lower_case }}</dt>\n" \
                       "{% endfor %}";
    parser_test(src,1,0);
  }

  {
    /* FOR LOOP */
    const char* src = "UVWXYZ{% for _,_ in my_dict if a != True %}\n" \
                       "<dt>{{ KeyValue | filter1 }}</dt>\n" \
                       "<dt>{{ a | lower_case }}</dt>\n" \
                       "{% endfor %}";
    parser_test(src,1,0);
  }



  {
    /* FOR LOOP WITH CONTROLS */
    const char* src = "{% for a in my_dict %}\n" \
                      "{% if b == 3 %}{% continue %}{% endif %}\n" \
                      "{% endfor %}\n";
    parser_test(src,1,0);
  }

  {
    /* FOR LOOP WITH CONTROLS */
    const char* src = "{% for a in my_dict %}\n" \
                      "{% if b == 3 %}{% break %}{% endif %}\n" \
                      "{% if b == 2 %}{% continue %}{% endif %}\n" \
                      "{% endfor %}\n";
    parser_test(src,1,0);
  }

  {
    /* FOR LOOP WITH CONTROLS */
    const char* src = "{% for a,b in my_dict %}\n" \
                      "{% if b == 3 %}{% break %}{% endif %}\n" \
                      "{{ UU }}\n" \
                      "{% if b == 2 %}{% continue %}{% endif %}\n" \
                      "{{ VV }}\n" \
                      "{% endfor %}\n";
    parser_test(src,1,0);
  }

  {
    /* FOR LOOP WITH CONTROLS */
    const char* src = "{% for _,_ in my_dict %}\n" \
                      "{% if b == 3 %}{% break %}{% endif %}\n" \
                      "{{ UU }}\n" \
                      "{% if b == 2 %}{% continue %}{% endif %}\n" \
                      "{{ VV }}\n" \
                      "{% endfor %}\n";
    parser_test(src,1,0);
  }


  {
    /* FOR LOOP WITH CONTROLS */
    const char* src = "{% for _,_ in my_dict %}\n" \
                      "{% if b == 3 %}{% break %}{% endif %}\n" \
                      "{% if b == 2 %}{% continue %}{% endif %}\n" \
                      "{% if b == 4 %}{% break %}{% endif %}\n" \
                      "{% if b == 5 %}{% continue %}{% endif %}\n" \
                      "{% if b == 6 %}{% break %}{% endif %}\n" \
                      "{% if b == 7 %}{% continue %}{% endif %}\n" \
                      "{% endfor %}\n";
    parser_test(src,1,0);
  }

}

static
void parser_branch() {
  {
    /* BRANCH */
    const char* src = "UUVVXX\n" \
      "{% if users %}\n" \
      "<ul>HelloWorld</ul>\n" \
      "{% for use in users %}\n" \
      "{{use}}\n" \
      "{% endfor %}\n" \
      "</ul>\n" \
      "{% endif %}\n";
    parser_test(src,1,0);
  }
  {
    /* BRANCH */
    const char* src = "UUVVXX\n" \
      "{% if users %}\n" \
      "</ul>\n" \
      "{% elif users == 3 % upv + 2.2 * 4 %}\n" \
      "{{UUU}}\n" \
      "{% endif %}\n";
    parser_test(src,1,0);
  }
  {
    /* BRANCH */
    const char* src = "UUVVXX\n" \
      "{% if users %}\n" \
      "</ul>\n" \
      "{% else %}\n" \
      "{{UUU}}\n" \
      "{% endif %}\n";
    parser_test(src,1,0);
  }
  {
    /* BRANCH */
    const char* src = \
      "UUVVXX\n" \
      "{% if users == 3+4*7%5//uv %}\n" \
      "{{ users }}\n" \
      "{% elif users % 3 == 0 %}\n" \
      "{{ users }} {{ users }}\n" \
      "{% endif %}\n";
    parser_test(src,1,0);
  }
}

static
void parser_expr() {

  { /* simple expression 1 */
    const char* src = " {% do ( users+3+4+foo(1,2,3) | user_end ) + 78-2 %} ";
    parser_test(src,1,0);
  }



  { /* simple expression 2 */
    const char* src = " {% do users.append( some_string + \'Hello\' ) // another.one ** 3 %}";
    parser_test(src,1,0);
  }



  { /* simple expression 3 */
    const char* src = "{% do users.append(some_thing+\'UUV\') >= -users.name %}\n";
    parser_test(src,1,0);
  }

  { /* logic */
    const char* src = "{% do append >= only and user >= shoot or user == None or not True %}";
    parser_test(src,1,0);
  }



  { /* logic */
    const char* src = "{% if append >= only and user >= more or not(user < 100) or None != False %}\n" \
                      "{% endif %}";
    parser_test(src,1,0);
  }

  {/* logic */
    const char* src = "{% if append > a %}\n" \
                      "{{ append }} {{ uuvv }}\n" \
                      "{{ hello  }}\n" \
                      "{% elif append == a %}\n" \
                      "{{ append }}\n" \
                      "{% else %}\n" \
                      "{{ yo+are+my+sunshine }}\n" \
                      "{% endif %}";
    parser_test(src,1,0);
  }


  { /* tenary */
    const char* src = "{% do 1+3*2//5 if me is you and i in some_map else 3+5 %}";
    parser_test(src,1,0);
  }

  { /* tenary */
    const char* src = "{% do (1+3*2//5 if me is you is he is not tom and i in some_map else 3+5) if None is True else None %}";
    parser_test(src,1,0);
  }

  { /* list */
    const char* src = "{% do [1,2,'three',True,False,None,1*MyName,[]] %}";
    parser_test(src,1,0);
  }

  { /* dict */
    const char* src = "{% do { 'You' : 'What' ," \
                      "        'Me'  : []," \
                      "        'Go'  : { 'Yet':1 , 'Another' : MyName+3333/2 }," \
                      "        HUHU  : {} } %}";
    parser_test(src,1,0);
  }

  { /* tuple */
    const char* src = "{% do ([],{},(),(())) %}";
    parser_test(src,1,0);
  }

  { /* tuple */
    const char* src = "{% do (1+1) + (1+1,) %}";
    parser_test(src,1,0);
  }

  { /* component */
    const char* src = "{% do a.p.u[\'v\'][myname+1+'string'].push(U) %}";
    parser_test(src,1,0);
  }
}

static
void parser_macro() {

  { /* MACRO */
    const char* src = "{% macro input(name,value=' ',type='text',size=20) %}\n" \
                      "type=\"{{type}}\" name=\"{{name}}\" value=\"{{value|e}}\" \
                      size=\"{{size}}\"\n" \
                      "{% endmacro %}\n" \
                      "{{ input('UserName') }}\n" \
                      "{{ input('You','Val','TT',30) }}\n";
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* obj = parse(a,"Hello World",src,0,0);
    const struct program* prg;
    struct string input = CONST_STRING("input");
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    prg = ajj_object_get_jinja_macro(obj,&input);
    assert(prg);
  }



  { /* MACRO */
    const char* src = "{% macro input() %}\n" \
                      "type={{U}} {{V}} {{W}} {{X}} {{Y}} {{Z}}\n" \
                      "{% endmacro %}\n" \
                      "{{ input() }}\n" \
                      "{{ input(1)}}\n";
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* obj = parse(a,"Hello World",src,0,0);
    const struct program* prg;
    struct string input = CONST_STRING("input");
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    prg = ajj_object_get_jinja_macro(obj,&input);
    assert(prg);

  }

  { /* MACRO */
    const char* src = "{% macro input(my_dict) %}\n" \
                      "{% for a in my_dict if a == None %}\n" \
                      "{{ a }} UVW {{ a }}\n" \
                      "{% endfor %}\n" \
                      "{% endmacro %}\n" \
                      "{{ input(1)}}\n";
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* obj = parse(a,"Hello World",src,0,0);
    const struct program* prg;
    struct string input = CONST_STRING("input");
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    prg = ajj_object_get_jinja_macro(obj,&input);
    assert(prg);

  }
}

static
void parser_block() {

  {
    const char* src = "================\n" \
                      "{% block head %}\n" \
                      "<><><><><><><><><>\n" \
                      "{% endblock %}\n"\
                      "================\n";
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* obj = parse(a,"Hello World",src,0,0);
    const struct program* prg;
    const struct string head = CONST_STRING("head");
    assert(obj);
    prg =ajj_object_jinja_main(obj);
    assert(prg);
    prg =ajj_object_get_jinja_block(obj,&head);

  }

  {
    const char* src = "==================\n" \
                      "{% block head %}\n" \
                      "{% for a in my_dict if a == my_upvalue * 2 %}\n" \
                      "{% do opt_print(a) %}\n" \
                      "{% endfor %}\n" \
                      "{% endblock %}\n" \
                      "{{ UUVV }} Hello World\n";
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* obj = parse(a,"Hello World",src,0,0);
    const struct program* prg;
    const struct string head = CONST_STRING("head");
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
}

static
void parser_do() {
  {
    const char* src = "<><>><><>{{}}{% do my_list.append(1) %}\n";
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* obj = parse(a,"Hello World",src,0,0);
    const struct program* prg;
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);

  }
}

static
void parser_filter() {

  {
    const char* src="A{% filter upper %} MyLargeBlock {% endfilter %}N\n";
    parser_test(src,1,0);
  }


  {
    const char* src="A{% filter upper(1,None,True,'Hello') %} MyLargeBlock {% endfilter %}N\n";
    parser_test(src,1,0);
  }
}

static
void parser_set() {

  {
    const char* src = "A{% set hulala= append.append.append(1,2,3,'V') %}UU\n";
    parser_test(src,1,0);
  }

  {
    const char* src = "A{% set hulala %}\n" \
                      "UUVVWWXXYYZZ\n" \
                      "{% endset %}\n";
    parser_test(src,1,0);
  }
}

static
void parser_with() {
  {
    const char* src = "A{% with %}\n" \
                      "{% set foo = 42 %}\n" \
                      "{{ foo }}\n" \
                      "{% endwith %}";
    parser_test(src,1,0);
  }
}

static
void parser_call() {

  {
    const char* src  = "{% call(user) dump_users(list_of_user) %}\n" \
                       "<dl> {{ user }} </dl>\n" \
                       "{% endcall %}\n";
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* obj = parse(a,"Hello World",src,0,0);
    const struct program* prg;
    struct string call_name = CONST_STRING("@c0");
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
    prg = ajj_object_get_jinja_macro(obj,&call_name);

  }

  {
    const char* src  = "{% call dump_users() %}\n" \
                       "<dl> {{ user }} </dl>\n" \
                       "{% endcall %}\n";
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* obj = parse(a,"Hello World",src,0,0);
    const struct program* prg;
    struct string call_name = CONST_STRING("@c0");
    assert(obj);
    prg = ajj_object_jinja_main(obj);
    assert(prg);
  }
}

static
void parser_include() {

  { /* basic include */
    const char* src = "{% include 'file.html' %}";
    parser_test(src,1,0);
  }
  { /* upvalue */
    const char* src = "{% include 'file.html' upvalue %}\n" \
                      "{% set key=my_key+2 %}\n" \
                      "{% set key2=my_key+3 optional %}\n" \
                      "{% set key3=my_key+4 override %}\n" \
                      "{% endinclude %}\n";
    parser_test(src,1,0);
  }
  { /* upvalue */
    const char* src = "{% include 'file.html' upvalue %}\n" \
                      "{% endinclude %}\n";
    parser_test(src,1,0);
  }
  { /* json */
    const char* src = "{% include 'file.html' json 'json.file'+json_path %}\n" \
                      "{% set key1=my_key2 %} \n" \
                      "{% set key2=my_key3 override %}\n" \
                      "{% set key3=my_key4 optional %}\n" \
                      "{% endinclude %}";
    parser_test(src,1,0);
  }

  { /* json */
    const char* src = "{% include 'file.html' json 'json.file'+json_path %}\n" \
                      "{% endinclude %}";
    parser_test(src,1,0);
  }
}

static
void parser_import() {
  const char* src = "{% import 'file.name'+1 as uuvv %}";
  parser_test(src,1,0);
}

static
void parser_extends() {

  { /* basic extends */
    const char* src = "{% extends 'some-file.html' %}";
    parser_test(src,1,0);
  }

  {
    const char* src = "{% block you %} {{UUVV}} Haha {% endblock %}\n" \
                      "{% extends 'some-file.html' %}\n" \
                      "{% block me %} {{ UUBB }} asad {% endblock %}\n";
    parser_test(src,1,0);
  }
}

static
void parser_move() {
  { /* same scope move , should fail */
    const char* src = "{% set U = 1 %}\n" \
                      "{% set V = 2 %}\n" \
                      "{% move U=V %}\n";
    parser_test(src,1,0);
  }
  { /* different scope */
    const char* src = "{% set U = 1 %}\n" \
                      "{% with V = 2 %} \n" \
                      "{% move U=V %}\n" \
                      "{% endwith %}";
    parser_test(src,1,0);
  }
  { /* different scope */
    const char* src = "{% set U = 1 %}\n" \
                      "{% with V = 2 %} \n" \
                      "{% move V = U %}\n" \
                      "{% endwith %}";
    parser_test(src,1,0);
  }
}

static
void parser_constexpr() {

  {
    const char* src = "{% macro input1(arg1=[],arg2={},arg3='UV',arg4=44,arg5=True,arg6=False,arg7=None) %}" \
                      "{% endmacro %}";
    parser_test(src,1,0);
  }

  {
    const char* src = "{% macro input1(arg1=[1,-2,3],arg2={'U':'V','Q':123,'P':()},arg3='UV',arg4=44,arg5=True,arg6=False,arg7=None) %}" \
                      "{% endmacro %}";
    parser_test(src,1,0);
  }
}

void parser_test_main() {
  parser_basic();
  parser_set();
  parser_for();
  parser_do();
  parser_branch();
  parser_expr();
  parser_call();
  parser_filter();
  parser_macro();
  parser_block();
  parser_with();
  parser_include();
  parser_import();
  parser_extends();
  parser_move();
  parser_constexpr();
}

/* =============================================
 * VM
 * ===========================================*/

static void vm_test( const char* src ) {
  struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
  struct ajj_object* jinja;
  struct ajj_io* output = ajj_io_create_file(a,stdout);
  jinja = parse(a,"vm-test-jinja",src,0,0);
  if(!jinja) {
    fprintf(stderr,"%s",a->err);
    abort();
  }
  if(vm_run_jinja(a,jinja,output,NULL)) {
    fprintf(stderr,"%s",a->err);
    abort();
  }
  ajj_io_destroy(a,output);
  ajj_destroy(a); /* all memory should gone */
}

/* ==================================
 * TEST EXPRESSION
 * ================================*/

#define VM_TEST(A,B) \
  do { \
    vm_test("{% do assert_expr( A == B , 'A == B')%}"); \
  } while(0)

static
void vm_expr() {
  /* Arithmatic */
  VM_TEST(1+2*3,7);
  VM_TEST(1+2**3,9);
  VM_TEST(1+4,5);
  VM_TEST(1*5,5);
  VM_TEST(100*100,10000);
  VM_TEST(5/100,0.02);
  VM_TEST(5%6,1);
  vm_test("{% do assert_expr(5//5==1,'5//2==1') %}");
  VM_TEST(1*2**3 && 4/5 or 1,True);
  VM_TEST(1+2*3/4 >= 5 and 90,False);
  VM_TEST(3/4 if True == False else 4**2,4**2);
  {
    const char* src = "{% set idx = 0 %}" \
                       "{% for index,val in xrange(1000) %}" \
                       "{% set i = idx + 1 %}" \
                       "{% do assert_expr(index == i-1) %}" \
                       "{% move idx = i %}" \
                       "{% endfor %}";
    vm_test(src);
  }
  {
    const char* src = "{% set arr = [1,2,3,4,5,100,1000 ] %}" \
                       "{% for i,val in arr %}" \
                       "{% do assert_expr(val == arr[i]) %}" \
                       "{% endfor %}";
    vm_test(src);
  }
  {
    const char* src = "{% set a = [] %}" \
                       "{% for i,_ in xrange(10000) %}" \
                       "{% do a.append(i) %}" \
                       "{% endfor %}\n"\
                       "{% for i in xrange(10000) %}" \
                       "{% do assert_expr(a[i] == i) %}" \
                       "{% endfor %}";
    vm_test(src);
  }
  {
    const char* src = "{% set a = {} %}" \
                       "{% for i,_ in xrange(10) %}" \
                       "{% do a.set( 'HelloWorld'+i , i ) %}" \
                       "{% endfor %}" \
                       "{% set idx = 0 %}" \
                       "{% for i,v in a %}" \
                       "{% set num = idx + 1 %}" \
                       "{% move idx = num %}" \
                       "{% do assert_expr( i == 'HelloWorld'+(num-1) ) %}" \
                       "{% do assert_expr( v == num-1 ) %}" \
                       "{% endfor %}";
    vm_test(src);
  }
  {
    const char* src =  "{% set arr = [] %}" \
                        "{% for i in xrange(10) if i%2 ==0 %}" \
                        "{% do arr.append(i) %}" \
                        "{% endfor %}" \
                        "{% for i,v in arr %}" \
                        "{% do assert_expr(v == i*2) %}" \
                        "{% do assert_expr(arr[i] == v ) %}" \
                        "{% endfor %}";
    vm_test(src);
  }

  vm_test("{% set my_dict = {'abc':123} %}"\
      "{% do assert_expr(my_dict.abc == 123,'_') %}" \
      "{% do assert_expr(my_dict['abc']==123,'_') %}");

  vm_test("{% set my_dict = {'abc':[1,2,3] , 'u'*3:'f'*3 } %}" \
      "{% do assert_expr(my_dict.abc[1] == 2,'_') %}" \
      "{% do assert_expr(my_dict.abc[0] == 1,'_') %}" \
      "{% do assert_expr(my_dict.uuu == 'fff','_') %}" \
      "{% do assert_expr(my_dict['uuu'] == 'fff','_') %}" \
      "{% set dict2 = { 'new_dict' : my_dict , 'vv':[1,2,3,4] } %}"\
      "{% do assert_expr(dict2.new_dict.uuu == 'fff','_') %}" \
      "{% do assert_expr(dict2.new_dict.abc[2] == 3,'_') %}" \
      "{% do assert_expr(dict2.vv[3] == 4,'_') %}");

  vm_test("{% set my_list = [] %}" \
      "{% for i in xrange(1000) if i % 3 == 0 %}" \
      "{% do my_list.append(i) %}" \
      "{% endfor %}" \
      "{% set cnt = 0 %}" \
      "{% for i in xrange(1000) if i % 3 == 0 %}" \
      "{% set idx = cnt+1 %}" \
      "{% move cnt = idx %}" \
      "{% do assert_expr(my_list[idx-1]==i,'_'+i+':'+idx) %}" \
      "{% endfor %}");

  VM_TEST(2 is odd,False);
  VM_TEST(3 is odd,True);
  VM_TEST(4 is even,True);
  VM_TEST(NotDefine is not defined,True);
  /* tenary operations */
  VM_TEST(3 if True != False else 4,4);
  VM_TEST('HH'*3 if 'f'*3 == 'fff' else 'UUV','HH'*3);
  VM_TEST(5%2 == 0 if 3%2 == 0 else True,True);
  /* in , not in and # operators */
  vm_test("{% do assert_expr( #'HelloWorld' == 10 ) %}" \
      "{% do assert_expr( #[1,2,3] == 3 ) %}" \
      "{% do assert_expr( #{'U':'V','W':'X','Y':'Z'} == 3 ) %}" \
      "{% do assert_expr( #1 == 1 ) %}" \
      "{% do assert_expr( #None == 0 ) %}" \
      "{% do assert_expr( #False == 1 ) %}" \
      "{% do assert_expr( #True == 1 ) %}");
  vm_test("{% do assert_expr( 'a' in { 'a' :123 } == True ) %}" \
      "{% do assert_expr( 'a' in { 'B' : None } == False ) %}" \
      "{% do assert_expr( 1 in [1,2,3] ) %}" \
      "{% do assert_expr( 3 in [1,2,3,4] ) %}" \
      "{% do assert_expr( 998 not in [1,234] ) %}" \
      "{% do assert_expr( None in [ None, None , None ] ) %}" \
      "{% do assert_expr( True in [True , True ] ) %}" \
      "{% do assert_expr( False in [True, False] ) %}");
}

static
void vm_loop() {
  vm_test("{% set cat1 = [] %}" \
      "{% set cat2 = [] %}" \
      "{% set cat3 = [] %}" \
      "{% for i in xrange(1000) %}" \
      "{% if i % 3 == 0 %}" \
      "{% do cat1.append(i) %}" \
      "{% elif i % 3 == 1 %}" \
      "{% do cat2.append(i) %}" \
      "{% else %}" \
      "{% do cat3.append(i) %}" \
      "{% endif %}" \
      "{% endfor %}" \
      "{% for i,v in cat1 %}" \
      "{% do assert_expr(cat1[i]==i*3) %}" \
      "{% do assert_expr(cat1[i]==v) %}" \
      "{% endfor %}" \
      "{% for i,v in cat2 %}" \
      "{% do assert_expr(cat2[i]==i*3+1) %}" \
      "{% do assert_expr(cat2[i]==v) %}" \
      "{% endfor %}"
      "{% for i,v in cat3 %}" \
      "{% do assert_expr(cat3[i]==i*3+2) %}" \
      "{% do assert_expr(cat3[i] == v) %}" \
      "{% endfor %}");

  vm_test("{% set arr = [] %}" \
      "{% for i in xrange(10) if i % 3 == 0 %}" \
      "{% do arr.append(i) %}" \
      "{% endfor %}" \
      "{% for i,v in arr %}" \
      "{% do assert_expr(v == i*3) %}" \
      "{% do assert_expr(v == arr[i]) %}" \
      "{% endfor %}");

  vm_test("{% set arr = [] %}" \
      "{% for i in xrange(10) %}" \
      "{% if i % 2 != 0 %}" \
      "{% continue %}" \
      "{% endif %}" \
      "{% do arr.append(i) %}" \
      "{% endfor %}" \
      "{% for i,v in arr %}" \
      "{% do assert_expr(v == i*2) %}" \
      "{% do assert_expr(v == arr[i]) %}" \
      "{% endfor %}");

  vm_test("{% set obj = {} %}" \
      "{% for i in xrange(100) %}" \
      "{% do obj.set('HelloWorld'+i,i) %}" \
      "{% endfor %}" \
      "{% set temp = 0 %}"
      "{% for i,v in obj %}" \
      "{% set idx = temp + 1 %}"
      "{% do assert_expr( obj['HelloWorld'+(idx-1)] == (idx-1) ) %}" \
      "{% do assert_expr( obj['HelloWorld'+(idx-1)] == (idx-1) ) %}" \
      "{% move temp=idx %}" \
      "{% endfor %}");

  /* mixed control flow test */
  vm_test("{% set arr = [] %}" \
      "{% for x in xrange(6) %}" \
      "{% if x < 4 and x % 3 != 0 or x == 0 %}" \
      "{% continue %}" \
      "{% endif %}" \
      "{% if x >= 5 %}" \
      "{% break %}" \
      "{% endif %}" \
      "{% do arr.append(x) %}" \
      "{% endfor %}" \
      "{% do assert_expr( #arr == 2 ) %}" \
      "{% do assert_expr( arr[0] == 3 ) %}"\
      "{% do assert_expr( arr[1] == 4 ) %}");

  /* iterate through string */
  vm_test("{% set str = 'abcdef' %}" \
      "{% set char = [] %}" \
      "{% for c in str %}" \
      "{% do char.append(c) %}" \
      "{% endfor %}" \
      "{% do assert_expr(char[0] == 'a') %}" \
      "{% do assert_expr(char[1] == 'b') %}" \
      "{% do assert_expr(char[2] == 'c') %}" \
      "{% do assert_expr(char[3] == 'd') %}" \
      "{% do assert_expr(char[4] == 'e') %}" \
      "{% do assert_expr(char[5] == 'f') %}");

  /* sum of an array */
  vm_test("{% set output = 0 %}" \
      "{% for x in [1,2,3,4,5,6,7,8,9,10] %}" \
      "{% set V = output + x %}" \
      "{% move output = V %}"\
      "{% endfor %}" \
      "{% do assert_expr( output == 1+2+3+4+5+6+7+8+9+10 ) %}");

  /* stack consistence check */
  vm_test("{% set result = 0 %}" \
      "{% for i in xrange(100) %}" \
      "{% set sum = result + i %}" \
      "{% move result = sum %}" \
      "{% do assert_expr(sum==result) %}" \
      "{% endfor %}" \
      "{% do assert_expr(result==4950) %}");

  vm_test("{% set result = 0 %}" \
      "{% for i in xrange(100) if i % 5 == 0 %}" \
      "{% set num = result + i %}" \
      "{% if i >= 20 %}" \
      "{% set new_num = num + 10 %}" \
      "{% do assert_expr(new_num == num+10) %}" \
      "{% endif %}" \
      "{% move result=num %}"
      "{% do assert_expr(result == num) %}" \
      "{% endfor %}");

  vm_test("{% for i in xrange(100) if i % 5 == 0 %}" \
      "{# Set up a local variable #}" \
      "{% set val1 = i %}" \
      "{% do assert_expr(val1 == i) %}" \
      "{# Set up a jump branch #}" \
      "{% if i >= 20 %} {% break %} {% endif %}" \
      "{# Set up another local variable which is right after the jump #}" \
      "{% set val2 = i*i %}" \
      "{% do assert_expr(val2 == i*i) %}" \
      "{% endfor %}");

  vm_test("{% for i in xrange(100) %}" \
      "{% set val1 = i %}" \
      "{% do assert_expr(val1 == i) %}" \
      "{% if i % 5 != 0 %}" \
      "{% with another_val = 2 %}" \
      "{% continue %}" \
      "{% endwith %}" \
      "{% endif %}" \
      "{% set val2 = i*i %}" \
      "{% do assert_expr(i*i == val2) %}" \
      "{% if i >= 60 %} {% break %} {% endif %}" \
      "{% set val3 = i*i+i %}" \
      "{% do assert_expr(i*i+i == val3) %}"
      "{% do assert_expr(i*i == val2) %}"
      "{% endfor %}");

  /* Complicated nested loops */
  vm_test("{% for i in xrange(10) %}" \
      "{% set loop1_v1 = i %}" \
      "{% for j in xrange(10) %}" \
      "{% set loop2_v1 = i*j %}" \
      "{% for k in xrange(10) %}" \
      "{% set loop3_v3 = k*i*j %}" \
      "{% if loop3_v3 % 5 == 0 %}" \
      "{% break %}" \
      "{% endif %}" \
      "{% endfor %}" \
      "{% if loop2_v1 %3 == 1 %}" \
      "{% with A = 1 %}" \
      "{% with B = 2 %}" \
      "{% continue %}" \
      "{% endwith %}" \
      "{% endwith %}" \
      "{% endif %}" \
      "{% set U = [] %}" \
      "{% set V = { 'U'*3 : U } %}" \
      "{% endfor %}" \
      "{% endfor %}");

  /* Matrix sum */
  vm_test("{% set matrix = [ [1,2,3] , [4,5,6] , [7,8,9] ] %}" \
      "{% set result = 0 %}" \
      "{% for x in matrix %}" \
      "{% set sum = 0 %}" \
      "{% for y in x %}" \
      "{% set sum1 = sum + y %}" \
      "{% move sum = sum1 %}" \
      "{% endfor %}" \
      "{% set temp = sum + result %}" \
      "{% move result = temp %}" \
      "{% endfor %}" \
      "{# Now we get our matrix sum #}" \
      "{% do assert_expr(result==45) %}");

  /* Loop object maniuplation */
  vm_test("{% for i in xrange(10) %}" \
      "{% do assert_expr( loop.index == i + 1 ) %}" \
      "{% do assert_expr( loop.index0== i ) %}" \
      "{% do assert_expr( loop.revindex == 10-i ) %}" \
      "{% do assert_expr( loop.revindex0 == 9-i ) %}" \
      "{% do assert_expr( loop.length == 10 ) %}" \
      "{% if i == 9 %}" \
      "{% do assert_expr( loop.last ) %}" \
      "{% do assert_expr( loop.first == False ) %}" \
      "{% elif i == 0 %}" \
      "{% do assert_expr( loop.last == False ) %}" \
      "{% do assert_expr( loop.first ) %}" \
      "{% else %}" \
      "{% do assert_expr( not loop.last ) %}" \
      "{% do assert_expr( not loop.first) %}" \
      "{% endif %}" \
      "{% endfor %}");
}

static
void vm_branch() {
  vm_test("{% if True is True %}" \
      "{% do assert_expr(1+3*4==13) %}" \
      "{% else %}" \
      "{% do assert_expr(False) %}" \
      "{% endif %}");
  vm_test("{% set variable = 3 %}" \
      "{% if variable > 4 %}" \
      "{% do assert_expr(False) %}" \
      "{% elif variable == 3 %}" \
      "{% do assert_expr(True) %}" \
      "{% else %}" \
      "{% do assert_expr(False)%}" \
      "{% endif %}" );
  vm_test("{% set variable = 3 %}" \
      "{% if variable >= 3 %}" \
      "  {% if variable % 3 == 0 %}" \
      "     {% do assert_expr(True) %}" \
      "  {% else %}" \
      "  {% do assert_expr(False) %}" \
      "  {% endif %}" \
      "{% elif variable <= 1 %}" \
      "{% do assert_expr(False) %}" \
      "{% else %}" \
      "{% do assert_expr(False) %}" \
      "{% endif %}");
}

/* MOVE operations */
static
void vm_move() {
  /* Move to outer scope with primitive , should nothing changed */
  vm_test("{% set Outer = 0 %}" \
      "{% with %}" \
      "{% set Inner = 100 %}" \
      "{% do assert_expr(Outer==0 )  %}" \
      "{% move Outer=Inner %}" \
      "{% do assert_expr(Inner==100) %}" \
      "{% endwith %}" \
      "{% do assert_expr(Outer==100) %}");

  vm_test("{% set Outer = True %}" \
      "{% with %}" \
      "{% set Inner = False %}" \
      "{% do assert_expr( Outer == True  ) %}" \
      "{% move Outer = Inner %}" \
      "{% do assert_expr( Inner == False ) %}" \
      "{% endwith %}" \
      "{% do assert_expr( Outer == False ) %}");

  vm_test("{% set Outer = False %}" \
      "{% with %}" \
      "{% set Inner = True %}" \
      "{% do assert_expr( Outer == False) %}" \
      "{% move Outer=Inner %}" \
      "{% do assert_expr( Inner == True ) %}" \
      "{% endwith %}" \
      "{% do assert_expr( Outer == True ) %}");

  /* Move object out of the scope.
   * The correctness of this move semantic is also related
   * to the correctness of implementation for this object */
  vm_test("{% set Outer = [1,2,3,4] %}" \
      "{% with %}" \
      "{% with %}" \
      "{% with %}" \
      "{% with Inner = [100,101,102] %}" \
      "{% do assert_expr( Outer[0] == 1 ) %}" \
      "{% do assert_expr( Outer[1] == 2 ) %}" \
      "{% do assert_expr( Outer[2] == 3 ) %}" \
      "{% do assert_expr( Outer[3] == 4 ) %}" \
      "{% move Outer=Inner %}" \
      "{% do assert_expr( Inner[0] == 100 ) %}" \
      "{% do assert_expr( Inner[1] == 101 ) %}" \
      "{% do assert_expr( Inner[2] == 102 ) %}" \
      "{% endwith %}" \
      "{% endwith %}" \
      "{% endwith %}" \
      "{% endwith %}" \
      "{% do assert_expr( Outer[0] == 100 ) %}" \
      "{% do assert_expr( Outer[1] == 101 ) %}" \
      "{% do assert_expr( Outer[2] == 102 ) %}");

  vm_test("{% set Outer = { 'U'*3 : [ 1 , 2 ] } %}" \
      "{% with %}" \
      "{% with Inner = { 'V'*3 : [ 3, 4 ] } %}" \
      "{% do assert_expr( Outer.UUU[0] == 1 ) %}" \
      "{% do assert_expr( Outer['UUU'][1] == 2 ) %}" \
      "{% move Outer=Inner %}" \
      "{% do assert_expr( Inner.VVV[0] == 3 ) %}" \
      "{% do assert_expr( Inner['VVV'][1] == 4 ) %}" \
      "{% endwith %}" \
      "{% endwith %}" \
      "{% do assert_expr( Outer.VVV[0] == 3 ) %}" \
      "{% do assert_expr( Outer.VVV[1] == 4 ) %}");
}

/* WITH */
static
void vm_with() {
  vm_test("{% do assert_expr( local_variable == None ) %}" \
      "{% with local_variable = True %}" \
      "{% do assert_expr( local_variable ) %}" \
      "{% endwith %}" \
      "{% do assert_expr( local_variable is None ) %}");
}

/* ============================
 * Function/Macro
 * ==========================*/
static
void vm_macro() {
  /* arguments passing is correct or not */
  vm_test("{% macro func(arg1,arg2,arg3,arg4,arg5,arg6,arg7) %}" \
      "{% do assert_expr( arg1 == True ) %}" \
      "{% do assert_expr( arg2 == False) %}" \
      "{% do assert_expr( arg3 == 10 ) %}" \
      "{% do assert_expr( arg4 == None ) %}" \
      "{% do assert_expr( arg5 == [1,2,3,4] ) %}" \
      "{% do assert_expr( arg6 == {'uu':3 } ) %}" \
      "{% do assert_expr( arg7 == 'HelloWorld') %}" \
      "{# Test builtin value of macro/functions. #}" \
      "{% do assert_expr( __argnum__ == 7 ) %}" \
      "{% do assert_expr( __func__ == 'func' ) %}" \
      "{% do assert_expr( caller == '__main__' ) %}" \
      "{% do assert_expr( vargs == None ) %}" \
      "{% do assert_expr( self != None ) %}" \
      "{% endmacro %}" \
      "{% do func(True,False,10,None,[1,2,3,4],{'uu':3},'HelloWorld') %}");
  /* default arguments */
  vm_test("{% macro func(arg1,arg2=True,arg3=False,arg4='HelloWorld',arg5=[1,2,3,4],arg6={'UU':'Hello'}) %}" \
      "{% do assert_expr( arg1 == 10 ) %}" \
      "{% do assert_expr( arg2 == True ) %}" \
      "{% do assert_expr( arg3 == False ) %}" \
      "{% do assert_expr( arg4 == 'HelloWorld') %}" \
      "{% do assert_expr( arg5 == [1,2,3,4] ) %}" \
      "{% do assert_expr( arg6 == { 'UU' : 'Hello' } ) %}" \
      "{% do assert_expr( __argnum__ == 1 ) %}" \
      "{% do assert_expr( __func__ == 'func' )%}" \
      "{% do assert_expr( caller == '__main__' ) %}" \
      "{% do assert_expr( vargs == None ) %}" \
      "{% do assert_expr( self != None ) %}" \
      "{% endmacro %}" \
      "{% do func(10) %}");
  /* varags */
  vm_test("{% macro func(arg1) %}" \
      "{% do assert_expr(arg1 == 10) %}" \
      "{% do assert_expr(vargs != None) %}" \
      "{% do assert_expr(vargs[0] == True) %}" \
      "{% do assert_expr(vargs[1] == False) %}" \
      "{% do assert_expr(vargs[2] == 10) %}" \
      "{% do assert_expr(vargs[3] == 'HelloWorld') %}" \
      "{% do assert_expr(vargs[4] == [1,2,3,4] ) %}" \
      "{% do assert_expr(vargs[5] == {'UU':'Hello' } ) %}" \
      "{% do assert_expr(#vargs == 6) %}" \
      "{% do assert_expr(__argnum__ == 1+6) %}" \
      "{% endmacro %}" \
      "{% do func(10,True,False,10,'HelloWorld',[1,2,3,4],{'UU':'Hello'}) %}");
  /* return */
  vm_test("{% macro Sum(arg) %}" \
      "{% set result = 0 %}" \
      "{% for i in arg %}" \
      "{% set sum = i + result %}" \
      "{% move result = sum %}" \
      "{% endfor %}" \
      "{% return result %}" \
      "{% endmacro %}" \
      "{% do assert_expr(Sum([1,2,3,4,5]) == 1+2+3+4+5) %}" );
  /* return in different branch */
  vm_test("{% macro ComplicatedReturn(arg) %}" \
      "{% set result1 = [] %}" \
      "{% set result2 = [] %}" \
      "{% set result3 = [] %}" \
      "{% if #arg >= 6  %}" \
      "{% for i in arg %}" \
      "{% if i % 3 == 0 %}" \
      "{% do result1.append(i) %}" \
      "{% endif %}" \
      "{% endfor %}" \
      "{% return result1 %}" \
      "{% elif #arg <= 3 %}" \
      "{% set local_var = 0 %}" \
      "{% for i in arg %}" \
      "{% set sum = local_var + i %}" \
      "{% move local_var = sum %}" \
      "{% do result2.append(sum) %}" \
      "{% endfor %}" \
      "{% return result2 %}" \
      "{% else %}" \
      "{% return arg %}"\
      "{% endif %}" \
      "{% endmacro %}" \
      "{% set LocalVar1 = 1 %}" \
      "{% set LocalVar2 = True %}" \
      "{% set LocalVar3 = [1,2,3,4] %}" \
      "{% set LocalVar4 = {'uu':'vv','hh':'pp'} %}" \
      "{% do assert_expr(ComplicatedReturn([1,2,3,4]) == [1,2,3,4]) %}" \
      "{% do assert_expr(ComplicatedReturn([1,2,3]) == [1,3,6]) %}" \
      "{% do assert_expr(ComplicatedReturn([1,2,3]) == [1,3,6]) %}" \
      "{% do assert_expr(ComplicatedReturn([1,2,3,4,5,6]) == [3,6]) %}" \
      "{% do assert_expr(LocalVar1 == 1 ) %}" \
      "{% do assert_expr(LocalVar2 == True) %}" \
      "{% do assert_expr(LocalVar3 == [1,2,3,4] ) %}" \
      "{% do assert_expr(LocalVar4 == {'uu':'vv','hh':'pp'} ) %}"
      );
  /* recursive call */
  vm_test("{% macro fib(num) %}" \
      "{% if num == 0 %}" \
      "{% return 0 %}" \
      "{% elif num == 1 %}" \
      "{% return 1 %}" \
      "{% else %}" \
      "{% return fib(num-1) + fib(num-2) %}" \
      "{% endif %}" \
      "{% endmacro %}" \
      "{% do assert_expr(fib(20) == 3*5*11*41)  %}");
  /* combine the call with other complicated structure */
  /* filter odd number */
  vm_test("{% macro filter_odd(arr) %}" \
      "{% set result = [] %}" \
      "{% for i in arr if i % 2 == 0 %}" \
      "{% do result.append(i) %}" \
      "{% endfor %}" \
      "{% return result %}" \
      "{% endmacro %}" \
      "{% do assert_expr( ([1,2,3,4] | filter_odd) == [2,4] ) %}");
  /* fib with iteration */
  vm_test("{% macro fib(num) %}" \
      "{% if num == 0 %}" \
      "{% return 0 %}" \
      "{% elif num == 1 %}" \
      "{% return 1 %}" \
      "{% else %}" \
      "{% set a = 0 %}" \
      "{% set b = 1 %}" \
      "{% for i in xrange(num-1) %}" \
      "{% set sum = a + b %}" \
      "{% set l = b %}" \
      "{% move a= l %}" \
      "{% move b= sum %}" \
      "{% endfor %}" \
      "{% return b %}"\
      "{% endif %}" \
      "{% endmacro %}"\
      "{% do assert_expr(fib(20) == 3*5*11*41) %}");

  /* binary search */
  vm_test("{% macro bsearch(arr,low,high,val) %}" \
      "{% if low > high %}" \
      "{% return -1 %}" \
      "{% else %}" \
      "{% if low == high %}" \
      "{% if val == arr[low] %}" \
      "{% return low %}"
      "{% else %}" \
      "{% return -1 %}" \
      "{% endif %}" \
      "{% endif %}" \
      "{% endif %}" \
      "{% set mid = (low+high)/2 | floor %}" \
      "{% if arr[mid] == val %}" \
      "{% return mid %}" \
      "{% elif arr[mid] > val %}" \
      "{% if mid-1 < low %}" \
      "{% return -1 %}" \
      "{% else %}" \
      "{% return bsearch(arr,low,mid-1,val) %}" \
      "{% endif %}" \
      "{% else %}" \
      "{% if mid + 1 > high %} "\
      "{% return -1 %}" \
      "{% else %}" \
      "{% return bsearch(arr,mid+1,high,val) %}" \
      "{% endif %}" \
      "{% endif %}" \
      "{% endmacro %}" \
      "{% set arr = [1,2,3,4,5,6,7,8,9] %}" \
      "{% set capture = bsearch(arr,0,#arr-1,7) %}" \
      "{% do assert_expr(arr[capture] == 7) %}");

  /* slice */
  vm_test("{% macro slice(arr,low,up) %}" \
      "{% set result = [] %}" \
      "{% if low >= up %}" \
      "{% return result %}" \
      "{% endif %}" \
      "{% for i in xrange(up-low) %}" \
      "{% do result.append(arr[low+i]) %}" \
      "{% endfor %}" \
      "{% return result %}" \
      "{% endmacro %}" \
      "{% do assert_expr( ( [ 1,2,3,4,5 ] | slice(2,3) ) == [3] ) %}");
}

static
void vm_call() {
  vm_test("{% macro child(arg) %}" \
      "{% do assert_expr(arg == 10) %}" \
      "{% do assert_expr(caller() == 'HelloWorld') %}" \
      "{% endmacro %}" \
      "{% call child(10) %}" \
      "{% return 'HelloWorld' %}" \
      "{% endcall %}");

  vm_test("{% macro child(arg1,arg2=True,arg3=False,arg4=[1,2,3,4]) %}" \
      "{% do assert_expr(arg1 == 10) %}" \
      "{% do assert_expr(arg2 == True ) %}" \
      "{% do assert_expr(arg3 == False) %}" \
      "{% do assert_expr(arg4 == [1,2,3,4] ) %}" \
      "{% do assert_expr(caller('HelloWorld') == 'Correct') %}" \
      "{% endmacro %}" \
      "{% call(arg) child(10) %}" \
      "{% do assert_expr(arg == 'HelloWorld') %}" \
      "{% return 'Correct' %}" \
      "{% endcall %}");

  vm_test("{% macro child(arg) %}" \
      "{% if __caller_stub__ is not None %}" \
      "{% do assert_expr( caller() == 'HelloWorld' ) %}" \
      "{% endif %}" \
      "{% endmacro %}" \
      "{% call child(10) %}" \
      "{% return 'HelloWorld' %}" \
      "{% endcall %}" \
      "{% do child(10) %}");
}

static void do_import( const char* source , const char* lib ) {
  struct ajj* a ;
  struct ajj_object* jinja;
  struct ajj_object* m;
  struct ajj_io* output;
  struct jj_file* f1, *f2;
  a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
  jinja = parse(a,"External",lib,0,0);
  if(!jinja) {
    fprintf(stderr,"%s",a->err);
    abort();
  }
  m = parse(a,"Main",source,0,0);
  if(!m) {
    fprintf(stderr,"%s",a->err);
    abort();
  }

  f1 = ajj_find_template(a,"External");
  f2 = ajj_find_template(a,"Main");

  output = ajj_io_create_file(a,stdout);
  if(vm_run_jinja(a,m,output,NULL)) {
    fprintf(stderr,"%s",a->err);
    abort();
  }
  ajj_io_destroy(a,output);
  ajj_destroy(a);
}

static
void vm_import() {
  do_import("{% import 'External' as Lib %}" \
      "{% do assert_expr( Lib.HelloWorld() == 'HelloWorld' ) %}",
      "{% macro HelloWorld() %}" \
      "{% return 'HelloWorld' %}" \
      "{% endmacro %}");
}

static
struct ajj_object*
load_template( struct ajj* a , const char* key , const char* src ) {
  struct ajj_object* jinja;
  jinja = parse(a,key,src,0,0);
  if(!jinja) {
    fprintf(stderr,"%s",a->err);
    abort();
  }
  return jinja;
}

static
void vm_extends() {
  {
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    load_template(a,"Base",
        "{% block head %}" \
        "This line should never show up!\n" \
        "{% endblock %}");

    jinja = load_template(a,"Child",
        "{% extends 'Base' %}" \
        "{% block head %}" \
        "This line should show up!\n"
        "{% endblock %}");
    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  { /* Multi-level inheritance */
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);
    load_template(a,"Base",
        "{% block head %}" \
        "This line should never show up!\n" \
        "{% endblock %}");

    load_template(a,"Child",
        "{% block tail %}" \
        "This is a block will never showed up when this "\
        "template is inherited or it will be showed when "\
        "this template is used as a leaf node !" \
        "{% endblock %}" \
        "{% extends 'Base' %}" \
        "{% block head %}" \
        "This line should show up!\n"
        "{% endblock %}");

    jinja = load_template(a,"Leaf",
        "{% extends 'Child' %}" \
        "{% block tail %}" \
        "This is a tail overload ~\n"\
        "{% endblock %}" \
        "{% block head %}" \
        "This is from leaf~\n"\
        "{% endblock %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  { /* Multiple inheritance */
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Base1",
        "{% block b1_head %}" \
        "Hello from Base1::b1_head!" \
        "{% endblock %}");

    load_template(a,"Base2",
        "{% block b2_head %}" \
        "Hello from Base2::b2_head!" \
        "{% endblock %}");

    jinja = load_template(a,"Child",
        "{% extends 'Base1' %}" \
        "{% extends 'Base2' %}" \
        "{% block b1_head %}" \
        "Child for b1_head\n" \
        "{% endblock %}" \
        "{% block b2_head %}" \
        "Child for b2_head\n" \
        "{% endblock %}");
    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  { /* Calling blocks as normal function */
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Base",
        "{% block b1 %}" \
        "Won't show up through extends\n" \
        "{% endblock %}" \
        "{% block b2 %}" \
        "{% do self.b1() %}" \
        "{% endblock %}");

    jinja = load_template(a,"Child",
        "{% extends 'Base' %}" \
        "{% block b1 %}" \
        "Show me \n" \
        "{% endblock %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  { /* Super */
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Base",
        "{% block b1 %}" \
        "Won't show up through extends\n" \
        "{% endblock %}" \
        "{% block b2 %}" \
        "{% do self.b1() %}" \
        "{% endblock %}");

    jinja = load_template(a,"Child",
        "{% extends 'Base' %}" \
        "{% block b1 %}" \
        "{% do super() %}" \
        "Show me \n" \
        "{% endblock %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }
}

static
void vm_include() {
  {
    /* Simple version */
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Inc1","{{ 'Hello World' }}\n");
    jinja = load_template(a,"Main","{% include 'Inc1' %}From Main\n");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }
  {
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Inc1","{{ 'Hello World' }}\n" \
        "{% macro MyTestMacro1() %}" \
        "{{ 'Hello World\n' }}" \
        "{% return 'Hello World2\n' %}" \
        "{% endmacro %}" \
        "{{ MyTestMacro1() }}");

    jinja = load_template(a,"Main","{% include 'Inc1' %}");
    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }
}

static
void vm_json() {
  DIR *d;
  struct dirent* dir;
  struct ajj* a;
  a = ajj_create(&AJJ_DEFAULT_VFS,NULL);

  d = opendir("json-test/");
  if(d) {
    while((dir = readdir(d)) != NULL) {
      char filename[1024];
      struct ajj_object* json;
      int valid;

      if(dir->d_type == DT_REG) {
        if(strcmp(dir->d_name,"LICENSE") ==0)
          continue;
        sprintf(filename,"json-test/%s",dir->d_name);

        json = json_parse(a,&(a->gc_root),filename,"test");
        if(strstr(filename,"invalid") || strstr(filename,"fail"))
          valid = 0;
        else
          valid = 1;

        if(valid) {
          if(!json) {
            fprintf(stderr,"%s:%s\n",dir->d_name,a->err);
          }
        } else {
          if(json) {
            fprintf(stderr,"This is a invalid example but gets parsed:%s!\n",
                dir->d_name);
          }
        }
      }
    }
    closedir(d);
  }
}

/* Test include with json as its MODEL */
static
void vm_include_with_json() {
  {
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Inc1","{{ Hello_World }}");

    jinja = load_template(a,
        "Main",
        "{% include 'Inc1' json 'hello_world.json' %}" \
        "{% endinclude %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  {
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Inc1",
        "{% macro sum(a) %}"  \
          "{% set s = 0 %}" \
          "{% for x in a %}" \
            "{% set v = x + s %}" \
            "{% move s = v %}" \
          "{% endfor %}" \
          "{% return s %}" \
        "{% endmacro %}" \
        "{{ sum(Arg1) }}");

    jinja = load_template(a,
        "Main",
        "{% include 'Inc1' json 'hello_world.json' %}" \
        "{% endinclude %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  {
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Inc1","{% do assert_expr( Arg3 + Arg4 == 10 ) %}");

    jinja = load_template(a,
        "Main",
        "{% include 'Inc1' json 'hello_world.json' %}" \
        "{% endinclude %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }

  {
    struct ajj* a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
    struct ajj_object* jinja;
    struct ajj_io* output = ajj_io_create_file(a,stdout);

    load_template(a,"Inc1",
        "\n\n{% for X,V in Arg2 %}" \
          "Key:{{X}};Value:{{V}}\n\n" \
        "{% endfor %}");

    jinja = load_template(a,
        "Main",
        "{% include 'Inc1' json 'hello_world.json' %}" \
        "{% endinclude %}");

    if( vm_run_jinja(a,jinja,output,NULL) ) {
      fprintf(stderr,"%s",a->err);
      abort();
    }
    ajj_io_destroy(a,output);
    ajj_destroy(a);
  }
}

static
void vm_basic() {
  vm_test("{% macro Input(title,class='dialog') %}" \
      "CallerStub=\n{{ caller('MyBoss','Overwrite') }}\n" \
      "{% do caller('MyBoos','Overwrite','Watch Me','Whip','Nae Nae') %}" \
      "{% endmacro %}" \
      "{% call(args,def='123') Input('Hello World','Caller') %}" \
      "argnum={{__argnum__}}\n" \
      "VARGS={{vargs}}\n" \
      "{% endcall %}");
  vm_test("{% import 'import.html' as T %}" \
      "{% import 'import.html' as T2%}" \
      "{{ T.Test('Hello World') }}" \
      "{{ T2.Test('Hello World2') }}");
  vm_test("{{ to_jsonc('{ \"Y\":[] , \
    \"V\":{},  \
      \"Hello\" : \
      \"World\" , \
      \"None\":null,\
      \"True\":true,\
      \"False\":\
      false,\
      \"UUV\":[1,2,3,4,5] \
      }') }}\n");
  vm_test("{% for x in xrange(10)%} III={{x}}\n {% endfor %}" \
      "{% extends 'base.html' %}" \
      "{% block Test3 %} vargs={{vargs}} {% endblock %}");
  vm_test("{% include 'include.html' %}"
      "Hello World From Child!\n");
  vm_test("{% include rm_trail(shell('pwd')) + '/include.html' %}"
      "{% import 'include.html' as Lib %}"
      "{% set arr = [1,2,3,4,5,6,7,8,9] %}"
      "Array = {{arr}}\n"
      "Sum = {{ Lib.array_sum(arr) }}\n"
      "Odd = {{ Lib.odd_filter(arr)}}\n"
      );
  vm_test("{{ -3*2>7-1998 | abs | abs | abs | abs }}");
}

void vm_test_main() {
  vm_expr();
  vm_loop();
  vm_move();
  vm_with();
  vm_branch();
  vm_macro();
  vm_call();
  vm_import();
  vm_include();
  vm_extends();
  vm_json();
  vm_include_with_json();
  vm_basic();
}

#ifndef DO_COVERAGE
int main() {
#else
void unit_test_main() {
#endif
  bc_test_main();
  util_test_main();
  lex_test_main();
  parser_test_main();
  vm_test_main();
#ifndef DO_COVERAGE
  return 0;
#endif
}
