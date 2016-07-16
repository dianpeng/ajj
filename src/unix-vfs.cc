/* INCLUDE ME WHEN YOU ARE IN LINUX SYSTEM */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

static void*
unix_vfs_load( struct ajj* a , const char* path ,
    size_t* len , time_t* ts, void* udata ) {
  struct stat st;
  int fd;
  char* buf;
  ssize_t ret;

  (void)udata;
  if(stat(path,&st)) {
    ajj_error(a,"Cannot state file:%s with errno:%s",
        path,strerror(errno));
    return NULL;
  }
  if(ts) {
    *ts = st.st_mtime;
  }

  /* open file */
  fd = open(path,O_RDONLY);
  if(fd<0) {
    ajj_error(a,"Cannot open file:%s with errno:%s",
        path,strerror(errno));
    return NULL;
  }

  buf = malloc(st.st_size+1);

  /* read the data into buffer */
  ret = read(fd,buf,st.st_size);
  if(ret >0) {
    buf[ret] = 0;
    goto done;
  } else {
    ajj_error(a,"Cannot read file:%s with errno:%s",
        path,strerror(errno));
    goto fail;
  }

done:
  close(fd);
  assert(len);
  *len = st.st_size;
  return buf;

fail:
  close(fd);
  free(buf);
  return NULL;
}

static int
unix_vfs_timestamp( struct ajj* a , const char* path ,
    time_t* ts , void* udata ) {
  struct stat st;
  (void)udata;
  if(stat(path,&st)) {
    ajj_error(a,"Cannot state file:%s with errno:%s",
        path,strerror(errno));
    return 1;
  }
  *ts = st.st_mtime;
  return 0;
}

static int
unix_vfs_timestamp_is_current( struct ajj* a , const char* path ,
    time_t ts , void* udata ) {
  time_t new_ts;
  if(unix_vfs_timestamp(a,path,&new_ts,udata)) return -1;
  return new_ts == ts;
}

struct ajj_vfs AJJ_DEFAULT_VFS = {
  unix_vfs_load,
  unix_vfs_timestamp,
  unix_vfs_timestamp_is_current
};

