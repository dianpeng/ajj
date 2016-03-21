#include <ajj.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>


int main() {
  DIR* d;
  struct dirent* dir;
  struct ajj* a;
  struct ajj_io* output;
  FILE* devnull = fopen("/dev/null","w");
  int cnt = 0;
  assert(devnull);
  a = ajj_create();
  output = ajj_io_create_file(a,devnull);
  d = opendir("jinja-test-case/");
  if(d) {
    while((dir = readdir(d)) != NULL) {
      char fn[1024];
      int ret;
      if(dir->d_type == DT_REG) {
        sprintf(fn,"jinja-test-case/%s",dir->d_name);
        ret = ajj_render_file(a,output,fn);
        if(ret) {
          fprintf(stderr,"%s",ajj_last_error(a));
          abort();
        }
        ++cnt;
      }
    }
  }
  printf("FINISH:%d\n",cnt);
}
