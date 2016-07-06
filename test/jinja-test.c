#include <ajj.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef DO_COVERAGE
int main() {
#else
int jinja_test_main() {
#endif
  DIR* d;
  struct dirent* dir;
  struct ajj* a;
  struct ajj_io* output;
  FILE* devnull = fopen("/dev/null","w");
  int cnt = 0;
  assert(devnull);
  a = ajj_create(&AJJ_DEFAULT_VFS,NULL);
  output = ajj_io_create_file(a,stdout);
  d = opendir("jinja-test-case/");
  if(d) {
    while((dir = readdir(d)) != NULL) {
      char fn[1024];
      int ret;
      if(dir->d_type == DT_REG) {
        sprintf(fn,"jinja-test-case/%s",dir->d_name);
        ret = ajj_render_file(a,output,fn,NULL);
        if(ret) {
          fprintf(stderr,"%s",ajj_last_error(a));
          abort();
        }
        ++cnt;
      }
    }
  }
  closedir(d);
  printf("FINISH:%d\n",cnt);
  fclose(devnull);
  ajj_io_destroy(a,output);
  ajj_destroy(a);
  return 0;
}
