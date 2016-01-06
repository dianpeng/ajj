#include "util.h"
#include <stdio.h>
#include "memmem.c" /* for memmem */

/* char string table */
static const unsigned char CSTR_TABLE[][2] = {
  { 0,0 },
  { 1,0 },
  { 2,0 },
  { 3,0 },
  { 4,0 },
  { 5,0 },
  { 6,0 },
  { 7,0 },
  { 8,0 },
  { 9,0 },
  { 10,0 },
  { 11,0 },
  { 12,0 },
  { 13,0 },
  { 14,0 },
  { 15,0 },
  { 16,0 },
  { 17,0 },
  { 18,0 },
  { 19,0 },
  { 20,0 },
  { 21,0 },
  { 22,0 },
  { 23,0 },
  { 24,0 },
  { 25,0 },
  { 26,0 },
  { 27,0 },
  { 28,0 },
  { 29,0 },
  { 30,0 },
  { 31,0 },
  { 32,0 },
  { 33,0 },
  { 34,0 },
  { 35,0 },
  { 36,0 },
  { 37,0 },
  { 38,0 },
  { 39,0 },
  { 40,0 },
  { 41,0 },
  { 42,0 },
  { 43,0 },
  { 44,0 },
  { 45,0 },
  { 46,0 },
  { 47,0 },
  { 48,0 },
  { 49,0 },
  { 50,0 },
  { 51,0 },
  { 52,0 },
  { 53,0 },
  { 54,0 },
  { 55,0 },
  { 56,0 },
  { 57,0 },
  { 58,0 },
  { 59,0 },
  { 60,0 },
  { 61,0 },
  { 62,0 },
  { 63,0 },
  { 64,0 },
  { 65,0 },
  { 66,0 },
  { 67,0 },
  { 68,0 },
  { 69,0 },
  { 70,0 },
  { 71,0 },
  { 72,0 },
  { 73,0 },
  { 74,0 },
  { 75,0 },
  { 76,0 },
  { 77,0 },
  { 78,0 },
  { 79,0 },
  { 80,0 },
  { 81,0 },
  { 82,0 },
  { 83,0 },
  { 84,0 },
  { 85,0 },
  { 86,0 },
  { 87,0 },
  { 88,0 },
  { 89,0 },
  { 90,0 },
  { 91,0 },
  { 92,0 },
  { 93,0 },
  { 94,0 },
  { 95,0 },
  { 96,0 },
  { 97,0 },
  { 98,0 },
  { 99,0 },
  { 100,0 },
  { 101,0 },
  { 102,0 },
  { 103,0 },
  { 104,0 },
  { 105,0 },
  { 106,0 },
  { 107,0 },
  { 108,0 },
  { 109,0 },
  { 110,0 },
  { 111,0 },
  { 112,0 },
  { 113,0 },
  { 114,0 },
  { 115,0 },
  { 116,0 },
  { 117,0 },
  { 118,0 },
  { 119,0 },
  { 120,0 },
  { 121,0 },
  { 122,0 },
  { 123,0 },
  { 124,0 },
  { 125,0 },
  { 126,0 },
  { 127,0 },
  { 128,0 },
  { 129,0 },
  { 130,0 },
  { 131,0 },
  { 132,0 },
  { 133,0 },
  { 134,0 },
  { 135,0 },
  { 136,0 },
  { 137,0 },
  { 138,0 },
  { 139,0 },
  { 140,0 },
  { 141,0 },
  { 142,0 },
  { 143,0 },
  { 144,0 },
  { 145,0 },
  { 146,0 },
  { 147,0 },
  { 148,0 },
  { 149,0 },
  { 150,0 },
  { 151,0 },
  { 152,0 },
  { 153,0 },
  { 154,0 },
  { 155,0 },
  { 156,0 },
  { 157,0 },
  { 158,0 },
  { 159,0 },
  { 160,0 },
  { 161,0 },
  { 162,0 },
  { 163,0 },
  { 164,0 },
  { 165,0 },
  { 166,0 },
  { 167,0 },
  { 168,0 },
  { 169,0 },
  { 170,0 },
  { 171,0 },
  { 172,0 },
  { 173,0 },
  { 174,0 },
  { 175,0 },
  { 176,0 },
  { 177,0 },
  { 178,0 },
  { 179,0 },
  { 180,0 },
  { 181,0 },
  { 182,0 },
  { 183,0 },
  { 184,0 },
  { 185,0 },
  { 186,0 },
  { 187,0 },
  { 188,0 },
  { 189,0 },
  { 190,0 },
  { 191,0 },
  { 192,0 },
  { 193,0 },
  { 194,0 },
  { 195,0 },
  { 196,0 },
  { 197,0 },
  { 198,0 },
  { 199,0 },
  { 200,0 },
  { 201,0 },
  { 202,0 },
  { 203,0 },
  { 204,0 },
  { 205,0 },
  { 206,0 },
  { 207,0 },
  { 208,0 },
  { 209,0 },
  { 210,0 },
  { 211,0 },
  { 212,0 },
  { 213,0 },
  { 214,0 },
  { 215,0 },
  { 216,0 },
  { 217,0 },
  { 218,0 },
  { 219,0 },
  { 220,0 },
  { 221,0 },
  { 222,0 },
  { 223,0 },
  { 224,0 },
  { 225,0 },
  { 226,0 },
  { 227,0 },
  { 228,0 },
  { 229,0 },
  { 230,0 },
  { 231,0 },
  { 232,0 },
  { 233,0 },
  { 234,0 },
  { 235,0 },
  { 236,0 },
  { 237,0 },
  { 238,0 },
  { 239,0 },
  { 240,0 },
  { 241,0 },
  { 242,0 },
  { 243,0 },
  { 244,0 },
  { 245,0 },
  { 246,0 },
  { 247,0 },
  { 248,0 },
  { 249,0 },
  { 250,0 },
  { 251,0 },
  { 252,0 },
  { 253,0 },
  { 254,0 },
  { 255,0 }
};

struct string NULL_STRING = { NULL , 0 };
struct string EMPTY_STRING = CONST_STRING("");
struct string TRUE_STRING = CONST_STRING("true");
struct string FALSE_STRING= CONST_STRING("false");
struct string NONE_STRING = CONST_STRING("none");


struct string string_dup( const struct string* str ) {
  struct string ret;
  if( string_null(str) ) return NULL_STRING;
  ret.str = malloc(str->len+1);
  ret.len = str->len;
  memcpy((void*)ret.str,str->str,str->len);
  return ret;
}

struct string string_dupc( const char* str ) {
  struct string ret;
  ret.str = strdup(str);
  ret.len = strlen(str);
  return ret;
}

struct string string_dupu( const char* str , size_t l ) {
  struct string tmp;
  tmp.str = str;
  tmp.len = l;
  return string_dup(&tmp);
}

/* Do not call string_destroy on constant string */
struct string string_const( const char* str , size_t len ) {
  struct string ret;
  ret.str = str;
  ret.len = len;
  return ret;
}

int string_cmp( const struct string* l , const struct string* r ) {
  assert( !string_null(l) );
  assert( !string_null(r) );

  /* This string comparison is not ditionary order comparison,
   * please notes. It is just a certain sort of partial order
   * that exists in the universe but doesn't have to be the order
   * by the dictionary number. The reason is because we use memcmp
   * instead of runestrcmp which require us to convert the input
   * sequence to the rune sequence and then do the order. In most
   * cases user may not really care about the order. If they do,we
   * have builtin function to do the actual comparison. */
  if(l->len > r->len)
    return 1;
  else if (l->len < r->len)
    return -1;
  else
    return memcmp(l->str,r->str,l->len);
}

int string_cmpcl(const struct string* l , const char* str , size_t len ) {
  struct string r;
  assert( !string_null(l) );
  assert( str != NULL );
  r.str = str;
  r.len = len;
  return string_cmp(l,&r);
}

const char*
string_str(const struct string* l , const struct string* r) {
  return memmem(l->str,l->len,r->str,r->len);
}

void string_destroy( struct string* str ) {
  if(string_empty(str))
    return;
  free((void*)str->str);
  *str = NULL_STRING;
}

void strbuf_reserve( struct strbuf* buf , size_t cap ) {
  char* nbuf = malloc(cap);
  if( buf->str ) {
    memcpy(nbuf,buf->str,buf->len);
    buf->str[buf->len] = 0;
    free(buf->str);
  }
  buf->str = nbuf;
  buf->cap = cap;
}

void strbuf_init( struct strbuf* buf ) {
  buf->len = 0;
  buf->cap = 0;
  buf->str = mem_grow(NULL,sizeof(char),0,&(buf->cap));
}

void strbuf_init_cap( struct strbuf* buf , size_t cap ) {
  buf->str = NULL;
  buf->len = 0;
  assert(cap>0);
  strbuf_reserve(buf,cap);
}

void strbuf_push( struct strbuf* buf , char c ) {
  if( buf->cap == 0 || buf->cap == buf->len+1 ) {
    buf->str = mem_grow(buf->str,sizeof(char),0,&(buf->cap));
  }
  buf->str[buf->len] = c;
  ++(buf->len);
  buf->str[buf->len]=0; /* always end with a null terminator */
}

void strbuf_push_rune( struct strbuf* buf , Rune c ) {
  int l;
  if( buf->cap == 0 || buf->cap < buf->len +1+ UTFmax ) {
    buf->str = mem_grow(buf->str,sizeof(char),UTFmax,&(buf->cap));
  }
  l = runetochar(buf->str+buf->len,&c);
  buf->len += l;
  buf->str[buf->len] = 0;
}

void strbuf_append( struct strbuf* buf , const char* str , size_t len ) {
  if( buf->cap == 0 || buf->cap <= buf->len + len + 1 ) {
    buf->str = mem_grow(buf->str,sizeof(0),len,&(buf->cap));
  }
  memcpy(buf->str+buf->len,str,len);
  buf->len += len;
  buf->str[buf->len] = 0; /* always end with a null terminator */
}

int strbuf_append_file( struct strbuf* buf , FILE* f ) {
  /* We cannot use ftell/fseek to figure out the length of the file
   * since it might be a pipe which doesn't have meaningful fseek */
  do {
    size_t sp = buf->cap - buf->len -1;
    int ret;
    if(sp == 0) {
      buf->str = mem_grow(buf->str,sizeof(char),1,&(buf->cap));
      sp = buf->cap - buf->len;
      assert(sp>0);
    }
    ret = fread(buf->str+buf->len,sizeof(char),sp,f);
    if( ret < 0 ) {
      return -1;
    } else if( ret < sp ) {
      buf->len += ret;
      buf->str[buf->len] = 0; /* null terminated */
      return 0;
    } else {
      buf->len += ret;
    }
  } while(1);
}

void strbuf_destroy( struct strbuf* buf ) {
  free(buf->str);
  buf->cap = buf->len = 0;
}

char strbuf_index( const struct strbuf* buf , int idx ){
  assert( idx < (int)(buf->len) );
  return buf->str[idx];
}

void strbuf_resize(struct strbuf* buf , size_t size) {
  if( size + 1 >= buf->cap ) {
    strbuf_reserve(buf,size+1);
    buf->len = size;
    buf->str[size] = 0;
  } else {
    buf->len = size;
    buf->str[buf->len] = 0;
  }
}

void strbuf_reset( struct strbuf* buf ) {
  buf->len = 0;
  if(buf->str) buf->str[0] = 0;
}

void* strbuf_detach( struct strbuf* buf, size_t* len , size_t* cap ) {
  void* m = buf->str;
  if(len) *len = buf->len;
  if(cap) *cap = buf->cap;
  buf->str = NULL;
  buf->cap = buf->len = 0;
  return m;
}

void strbuf_move( struct strbuf* buf , struct string* output ) {
  /* If the occupied the buffer is larger than 1/2 of string buffer
   * and its length is smaller than 1KB( small text ). We just return
   * the buffer directly. Otherwise, we do a deep copy */

  if( buf->len > STRBUF_MOVE_THRESHOLD ) {
    if( buf->len == buf->cap ) {
      output->str = buf->str;
      output->len = buf->len;
      buf->cap = buf->len = 0;
      buf->str = NULL;
    } else {
      output->str = strdup(buf->str);
      output->len = buf->len;
      buf->len = 0;
    }
  } else {
    if( buf->len >= buf->cap/2 ) {
      output->str = buf->str;
      output->len = buf->len;
      buf->cap = buf->len = 0;
      buf->str = NULL;
    } else {
      output->str = strdup(buf->str);
      output->len = buf->len;
      buf->len = 0;
    }
  }
}

struct string strbuf_tostring( struct strbuf* buf ) {
  struct string ret;
  ret.str = buf->str;
  ret.len = buf->len;
  return ret;
}

int strbuf_printf( struct strbuf* buf, const char* format, ... ) {
  va_list vl;
  va_start(vl,format);
  return strbuf_vprintf(buf,format,vl);
}

int strbuf_vprintf( struct strbuf* buf, const char* format, va_list vl ) {
  if( buf->cap == buf->len+1 ) {
    /* resize the memory */
    buf->str = mem_grow(buf->str,sizeof(char),0,&(buf->cap));
  }
  do {
    int ret = vsnprintf(
        buf->str + buf->len,
        buf->cap - buf->len,
        format,
        vl);
    if( ret == (int)(buf->cap-buf->len) ) {
      /* resize the memory again */
      buf->str = mem_grow(buf->str,sizeof(char),0,&(buf->cap));
    } else {
      if(ret >0) buf->len += ret;
      return ret;
    }
  } while(1);
}

void* mem_grow( void* ptr , size_t obj_sz,
    size_t append ,
    size_t* old_cap ) {
  size_t cap = *old_cap;
  /* We an use realloc safe even if the input ptr is not
   * filled up with data. We just copy garbage bytes and
   * it is no harm */
  if(cap == 0) {
    cap = INITIAL_MEMORY_SIZE > append ? INITIAL_MEMORY_SIZE : append;
  } else {
    if( cap >= BOUNDED_MEMORY_SIZE ) {
      /* now we fallback to linear growth instead of exponential
       * growth which avoids large memory allocation and waste */
      cap += BOUNDED_MEMORY_SIZE > append ? BOUNDED_MEMORY_SIZE : append;
    } else {
      cap += (append > cap) ? append+cap : cap;
    }
  }
  *old_cap = cap;
  return realloc(ptr,obj_sz*cap);
}

struct string
string_concate( const struct string* l , const struct string* r ) {
  struct strbuf sbuf;
  strbuf_init_cap(&sbuf,l->len+r->len+1);
  strbuf_append(&sbuf,l->str,l->len);
  strbuf_append(&sbuf,r->str,r->len);
  return strbuf_tostring(&sbuf);
}

struct string
string_multiply( const struct string*  l , int times ) {
  struct strbuf sbuf;
  strbuf_init_cap(&sbuf,l->len*times + 1);
  while(times-- >0) {
    strbuf_append(&sbuf,l->str,l->len);
  }
  return strbuf_tostring(&sbuf);
}

/* Yet another open addressing hash table */
static
unsigned int map_hash( const struct string* key ) {
  /* This hash function implementation is taken from LUA */
  size_t i;
  const size_t sz = key->len;
  unsigned int h = STRING_HASH_SEED;
  for( i = 0 ; i < sz ; ++i ) {
    h = (h ^((h<<5)+(h>>2))) + (unsigned int)(key->str[i]);
  }
  return h;
}

/* Insert a key into the hash table and return a slot entry to the caller.
 * This entry may be already in used ( return an existed one ) or a new one */
static
struct map_entry* map_insert_entry_c( struct map* d ,
    const char* key , unsigned int fullhash , int insert ) {
  unsigned int idx = fullhash & (d->cap-1);
  struct map_entry* e;
  e = d->entry + idx;

  if( !e->used ) {
    return insert ? e : NULL;
  } else {
    struct map_entry* ret = e->del ? e : NULL;
    struct map_entry* ne = e;
    /* Looking froward through the collision chain */
    do {
      if( ne->del ) {
        if( ret == NULL )
          ret = ne;
      } else {
        if( ne->hash == fullhash && (string_cmpc(&(ne->key),key)==0)) {
          /* We found an existed one here */
          return ne;
        }
      }
      if( !ne->more )
        break;
      ne = d->entry + ne->next;
    } while(1);

    /* We don't do insertion, so after searching in the chain,
     * if we cannot find any one, then just return NULL */
    if( !insert ) return NULL;

    if( ret == NULL ) {
      /* linear probing here */
      unsigned int h = fullhash;
      while(1) {
        ret = d->entry + (++h &(d->cap-1));
        if( !(ret->used) || ret->del )
          break;
      }
      assert( !ne->more );
      ne->next = ret - d->entry;
      ne->more = 1;
    }
    return ret;
  }
}

#define MAP_VALUE(D,E) (((char*)((D)->value)) + ((E)-((D)->entry))*(D)->obj_sz)

static inline
void map_value_store( struct map* d, struct map_entry* e , const void* val ) {
  void* pos = MAP_VALUE(d,e);
  memcpy(pos,val,d->obj_sz);
}

static inline
void map_value_load( struct map* d , struct map_entry* e , void* val ) {
  void* pos = MAP_VALUE(d,e);
  memcpy(val,pos,d->obj_sz);
}

static
struct map_entry* map_insert_entry(struct map* d,
    const struct string* key,
    unsigned int fullhash, int insert ) {
  return map_insert_entry_c(d,
      key->str,
      fullhash,
      insert);
}

/* rehashing */
static
void map_rehash( struct map* d ) {
  size_t new_cap = d->cap * 2; /* make sure power of 2 */
  void* new_buf = calloc(new_cap, sizeof(struct map_entry) + d->obj_sz);
  struct map temp_d;
  int i;

  temp_d.entry = new_buf;
  temp_d.cap = new_cap;
  temp_d.len = 0;
  temp_d.obj_sz = d->obj_sz;
  temp_d.value = (char*)(new_buf) + sizeof(struct map_entry)*new_cap;

  for( i = 0 ; i < d->cap ; ++i ) {
    struct map_entry* o = d->entry + i;
    struct map_entry* e;
    if( o->del || !(o->used) )
      continue;
    e = map_insert_entry(&temp_d,&(o->key),o->hash,1);
    e->key = o->key;
    e->hash = o->hash;
    map_value_store(&temp_d,e,MAP_VALUE(d,o));
    if(e->del) e->del = 0;
    if(!(e->used)) {
      e->used = 1;
      e->more = 0;
    }
  }

  /* free old memory if we have to */
  free(d->entry);

  temp_d.len = d->len;
  *d = temp_d;
}

int map_insert( struct map* d, const struct string* key , int own ,
    const void* val ) {
  int fh;
  struct map_entry* e;

  if( d->len == DICT_MAX_SIZE )
    return -1;

  if( d->cap == d->len )
    map_rehash(d);

  fh = map_hash(key);
  e = map_insert_entry(d,key,fh,1);
  e->key = own ? *key : string_dup(key);
  if( e->del ) e->del = 0;
  if( !(e->used) ) {
    e->used = 1;
    e->more = 0;
  }
  ++d->len;
  e->hash = fh;
  map_value_store(d,e,val);
  return 0;
}

int map_insert_c( struct map* d, const char* key ,
    const void* val ) {
  int fh;
  struct map_entry* e;
  struct string k;

  if( d->len == DICT_MAX_SIZE )
    return -1;

  if( d->cap == d->len )
    map_rehash(d);
  k.str = key;
  k.len = strlen(key);

  fh = map_hash(&k);
  e = map_insert_entry_c(d,key,fh,1);
  e->key = string_dup(&k);
  if( e->del ) e->del = 0;
  if( !(e->used) ) {
    e->used = 1;
    e->more = 0;
  }
  ++d->len;
  e->hash = fh;
  map_value_store(d,e,val);
  return 0;
}

int map_remove( struct map* d , const struct string* key , void* output ) {
  struct map_entry* e = map_insert_entry(d,key,map_hash(key),0);
  if( e == NULL )
    return -1;
  else {
    assert(e->used);
    assert(!e->del);
    assert(string_eq(&(e->key),key));
    /* destroy the key */
    string_destroy(&(e->key));
    if( output )
      map_value_load(d,e,output);
    e->del = 1;
    --d->len;
    return 0;
  }
}

int map_remove_c( struct map* d , const char* key , void* output ) {
  struct string k;
  struct map_entry* e;
  k.str = key;
  k.len = strlen(key);
  e = map_insert_entry_c(d,key,map_hash(&k),0);
  if( e == NULL )
    return -1;
  else {
    assert(e->used);
    assert(!e->del);
    assert(string_eqc(&(e->key),key));
    /* destroy string */
    string_destroy(&(e->key));
    if( output )
      map_value_load(d,e,output);
    e->del = 1;
    --d->len;
    return 0;
  }
}

void* map_find( struct map* d , const struct string* key ) {
  struct map_entry* e = map_insert_entry(d,key,map_hash(key),0);
  if( e ) {
    return MAP_VALUE(d,e);
  } else {
    return NULL;
  }
}

void* map_find_c( struct map* d , const char* key ) {
  struct string k;
  struct map_entry* e;
  k.str = key;
  k.len = strlen(key);
  e = map_insert_entry_c(d,key,map_hash(&k),0);
  if( e ) {
    return MAP_VALUE(d,e);
  } else {
    return NULL;
  }
}

void map_create( struct map* d , size_t obj_sz , size_t cap ) {
  assert( cap >= 2 && !((cap&(cap-1))) );
  assert( obj_sz > 0 );
  d->obj_sz = obj_sz;
  d->cap = cap;
  d->len = 0;
  d->entry = calloc(cap,sizeof(struct map_entry)+obj_sz);
  d->value = ((char*)(d->entry)) + cap*sizeof(struct map_entry);
}

void map_destroy( struct map* d ) {
  int i;
  /* We need to traversal through the mapionary to release all the key
   * since they all are on the heap */
  for( i = 0 ; i < d->cap ; ++i ) {
    struct map_entry* e = d->entry + i;
    if( e->used && !e->del ) {
      string_destroy(&(e->key));
    }
  }
  free(d->entry);
  d->entry = d->value = NULL;
  d->cap = d->len = 0;
}

void map_clear( struct map* d ) {
  int i;
  for( i = 0 ; i < d->cap ; ++i ) {
    struct map_entry* e = d->entry + i;
    if( e->used ) {
      if( !e->del ) {
        string_destroy(&(e->key));
      }
      memset(e,0,sizeof(*e));
    }
  }
  d->len = 0;
}

int map_iter_start( const struct map* d ) {
  int ret = 0;
  for( ; ret < d->cap ; ++ret ) {
    struct map_entry* e = d->entry + ret;
    if( e->used && !e->del )
      return ret;
  }
  return ret;
}

int map_iter_move( const struct map* d , int itr ) {
  for( ++itr ; itr < d->cap ; ++itr ) {
    struct map_entry* e = d->entry + itr;
    if( e->used && !e->del )
      return itr;
  }
  return itr;
}

struct map_pair map_iter_deref( struct map* d, int itr ) {
  struct map_entry* e = d->entry + itr;
  struct map_pair ret;
  assert( e->used && !e->del );
  ret.key = &(e->key);
  ret.val = ((char*)(d->value)) + (d->obj_sz*(e-(d->entry)));
  return ret;
}

/* =====================
 * Slab implementation
 * =====================*/

static
void slab_reserve( struct slab* sl ) {
  const size_t cap = sl->cur_cap * 2;
  void* mem = malloc(sizeof(struct chunk) + sl->cur_cap*sl->obj_sz*2);
  void* h;
  size_t i;
  ((struct chunk*)mem)->next = sl->ck;
  sl->ck = mem;
  h = mem = (char*)(mem) + sizeof(struct chunk);

  for( i = 0 ; i < cap-1 ; ++i ) {
    ((struct freelist*)(mem))->next =
      (struct freelist*)(((char*)mem) + sl->obj_sz);
    mem = (char*)mem + sl->obj_sz;
  }
  ((struct freelist*)(mem))->next = NULL;
  sl->fl = h;
  sl->cur_cap = cap;
}

void slab_init( struct slab* sl , size_t cap , size_t obj_sz ) {
  cap = cap < 32 ? 16 : cap/2;
  sl->obj_sz = obj_sz < sizeof(void*) ?
    sizeof(void*) : obj_sz;
  sl->fl = NULL;
  sl->ck = NULL;
  sl->cur_cap = cap;
  slab_reserve( sl );
}

void* slab_malloc( struct slab* sl ) {
  void* ret;
  if( sl->fl == NULL ) {
    slab_reserve(sl);
  }
  ret = sl->fl;
  sl->fl = sl->fl->next;
  return ret;
}

void slab_free( struct slab* sl , void* ptr ) {
  ((struct freelist*)ptr)->next = sl->fl;
  sl->fl = ptr;
}


void slab_destroy( struct slab* sl ) {
  struct chunk* c = sl->ck;
  struct chunk* n;
  while( c ) {
    n = c->next;
    free(c);
    c = n;
  }
  sl->ck = NULL;
  sl->fl = NULL;
  sl->cur_cap = sl->obj_sz = 0;
}

/* ===============================
 * Other
 * =============================*/
int is_int( double val ) {
  double i;
  modf(val,&i);
  if( i < INT_MAX && i > INT_MIN )
    return i == val;
  else
    return 0;
}

char* dtoc( double val , size_t* len ) {
  char buf[256];
  if( is_int(val) ) {
    int r;
    r = sprintf(buf,"%d",(int)(val)); /* very slow :( */
    assert( r> 0 && r < 256 );
    *len = (size_t)r;
    return strdup(buf);
  } else {
    int r;
    r = sprintf(buf,"%f",val);
    assert( r> 0 && r < 256 );
    *len = (size_t)r;
    return strdup(buf);
  }
}

const char* const_cstr( char c ) {
  unsigned char uc = (unsigned char)c;
  return (const char*)CSTR_TABLE[uc];
}
