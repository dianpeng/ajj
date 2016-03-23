#ifndef __TEST_CHECK_H__
#define __TEST_CHECK_H__

#ifdef NDEBUG
#include <stdlib.h>
#include <stdio.h>
#undef assert
#define assert(X) \
  do { \
    if(!(X)) { \
      fprintf(stderr,"%s","Assertion:"#X); \
      abort(); \
    } \
  } while(0)
#else
#include <assert.h>
#endif /* NDEBUG */

#endif /* __TEST_CHECK_H__ */
