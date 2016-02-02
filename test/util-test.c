#include "../src/util.h"
#include <stdio.h>


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
void test_macro() {
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
      strbuf_push(&b1,'c');
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
      strbuf_push(&b1,'c');
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
      strbuf_push(&b1,'d');
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
      strbuf_push(&b1,'c');
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
      int* ptr;
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

int main() {
  test_macro();
  test_string();
  test_strbuf();
  test_map();
  test_slab();
  printf("util-test pass!\n");
  return 0;
}
