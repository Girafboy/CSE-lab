/*
 * selftest /classfs/dir1 /classfs/dir2
 *
 * Test correctness of locking and cache coherence by creating
 * and deleting files in the same underlying directory
 * via two different ccfs servers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

char dn[5][512];
extern int errno;

char big[20001];
char huge[65536];

void
create1(const char *d, const char *f, const char *in)
{
  int fd;
  char n[512];

  /*
   * The FreeBSD NFS client only invalidates its caches
   * cache if the mtime changes by a whole second.
   */
  sleep(1);

  sprintf(n, "%s/%s", d, f);
  fd = creat(n, 0666);
  if(fd < 0){
    fprintf(stderr, "selftest: create(%s): %s\n",
            n, strerror(errno));
    exit(1);
  }
  if(write(fd, in, strlen(in)) != strlen(in)){
    fprintf(stderr, "selftest: write(%s): %s\n",
            n, strerror(errno));
    exit(1);
  }
  if(close(fd) != 0){
    fprintf(stderr, "selftest: close(%s): %s\n",
            n, strerror(errno));
    exit(1);
  }
}

void
check1(const char *d, const char *f, const char *in)
{
  int fd, cc;
  char n[512], buf[21000];

  sprintf(n, "%s/%s", d, f);
  fd = open(n, 0);
  if(fd < 0){
    fprintf(stderr, "selftest: open(%s): %s\n",
            n, strerror(errno));
    exit(1);
  }
  errno = 0;
  cc = read(fd, buf, sizeof(buf) - 1);
  if(cc != strlen(in)){
    fprintf(stderr, "selftest: read(%s) returned too little %d%s%s\n",
            n,
            cc,
            errno ? ": " : "",
            errno ? strerror(errno) : "");
    exit(1);
  }
  close(fd);
  buf[cc] = '\0';
  if(strncmp(buf, in, strlen(n)) != 0){
    fprintf(stderr, "selftest: read(%s) got \"%s\", not \"%s\"\n",
            n, buf, in);
    exit(1);
  }
}

void
unlink1(const char *d, const char *f)
{
  char n[512];

  sleep(1);

  sprintf(n, "%s/%s", d, f);
  if(unlink(n) != 0){
    fprintf(stderr, "selftest: unlink(%s): %s\n",
            n, strerror(errno));
    exit(1);
  }
}

void
checknot(const char *d, const char *f)
{
  int fd;
  char n[512];

  sprintf(n, "%s/%s", d, f);
  fd = open(n, 0);
  if(fd >= 0){
    fprintf(stderr, "selftest: open(%s) succeeded for deleted file\n", n);
    exit(1);
  }
}

void
append1(const char *d, const char *f, const char *in)
{
  int fd;
  char n[512];

  sleep(1);

  sprintf(n, "%s/%s", d, f);
  fd = open(n, O_WRONLY|O_APPEND);
  if(fd < 0){
    fprintf(stderr, "selftest: append open(%s): %s\n",
            n, strerror(errno));
    exit(1);
  }
  if(write(fd, in, strlen(in)) != strlen(in)){
    fprintf(stderr, "selftest: append write(%s): %s\n",
            n, strerror(errno));
    exit(1);
  }
  if(close(fd) != 0){
    fprintf(stderr, "selftest: append close(%s): %s\n",
            n, strerror(errno));
    exit(1);
  }
}

// write n characters starting at offset start,
// one at a time.
void
write1(const char *d, const char *f, int start, int n, char c)
{
  int fd;
  char name[512];

  sleep(1);

  sprintf(name, "%s/%s", d, f);
  fd = open(name, O_WRONLY|O_CREAT, 0666);
  if (fd < 0 && errno == EEXIST)
    fd = open(name, O_WRONLY, 0666);
  if(fd < 0){
    fprintf(stderr, "selftest: open(%s): %s\n",
            name, strerror(errno));
    exit(1);
  }
  if(lseek(fd, start, 0) != (off_t) start){
    fprintf(stderr, "selftest: lseek(%s, %d): %s\n",
            name, start, strerror(errno));
    exit(1);
  }
  for(int i = 0; i < n; i++){
    if(write(fd, &c, 1) != 1){
      fprintf(stderr, "selftest: write(%s): %s\n",
              name, strerror(errno));
      exit(1);
    }
    if(fsync(fd) != 0){
      fprintf(stderr, "selftest: fsync(%s): %s\n",
              name, strerror(errno));
      exit(1);
    }
  }
  if(close(fd) != 0){
    fprintf(stderr, "selftest: close(%s): %s\n",
            name, strerror(errno));
    exit(1);
  }
}

// check that the n bytes at offset start are all c.
void
checkread(const char *d, const char *f, int start, int n, char c)
{
  int fd;
  char name[512];

  sleep(1);

  sprintf(name, "%s/%s", d, f);
  fd = open(name, 0);
  if(fd < 0){
    fprintf(stderr, "selftest: open(%s): %s\n",
            name, strerror(errno));
    exit(1);
  }
  if(lseek(fd, start, 0) != (off_t) start){
    fprintf(stderr, "selftest: lseek(%s, %d): %s\n",
            name, start, strerror(errno));
    exit(1);
  }
  for(int i = 0; i < n; i++){
    char xc;
    if(read(fd, &xc, 1) != 1){
      fprintf(stderr, "selftest: read(%s): %s\n",
              name, strerror(errno));
      exit(1);
    }
    if(xc != c){
      fprintf(stderr, "selftest: checkread off %d %02x != %02x\n",
              start + i, xc, c);
      exit(1);
    }
  }
  close(fd);
}


void
createn(const char *d, const char *prefix, int nf, bool possible_dup)
{
  int fd, i;
  char n[512];

  /*
   * The FreeBSD NFS client only invalidates its caches
   * cache if the mtime changes by a whole second.
   */
  sleep(1);

  for(i = 0; i < nf; i++){
    sprintf(n, "%s/%s-%d", d, prefix, i);
    fd = creat(n, 0666);
    if (fd < 0 && possible_dup && errno == EEXIST)
      continue;
    if(fd < 0){
      fprintf(stderr, "selftest: create(%s): %s\n",
              n, strerror(errno));
      exit(1);
    }
    if(write(fd, &i, sizeof(i)) != sizeof(i)){
      fprintf(stderr, "selftest: write(%s): %s\n",
              n, strerror(errno));
      exit(1);
    }
    if(close(fd) != 0){
      fprintf(stderr, "selftest: close(%s): %s\n",
              n, strerror(errno));
      exit(1);
    }
  }
}

void
checkn(const char *d, const char *prefix, int nf)
{
  int fd, i, cc, j;
  char n[512];

  for(i = 0; i < nf; i++){
    sprintf(n, "%s/%s-%d", d, prefix, i);
    fd = open(n, 0);
    if(fd < 0){
      fprintf(stderr, "selftest: open(%s): %s\n",
              n, strerror(errno));
      exit(1);
    }
    j = -1;
    cc = read(fd, &j, sizeof(j));
    if(cc != sizeof(j)){
      fprintf(stderr, "selftest: read(%s) returned too little %d%s%s\n",
              n,
              cc,
              errno ? ": " : "",
              errno ? strerror(errno) : "");
      exit(1);
    }
    if(j != i){
      fprintf(stderr, "selftest: checkn %s contained %d not %d\n",
              n, j, i);
      exit(1);
    }
    close(fd);
  }
}

void
unlinkn(const char *d, const char *prefix, int nf)
{
  char n[512];
  int i;

  sleep(1);

  for(i = 0; i < nf; i++){
    sprintf(n, "%s/%s-%d", d, prefix, i);
    if(unlink(n) != 0){
      fprintf(stderr, "selftest: unlink(%s): %s\n",
              n, strerror(errno));
      exit(1);
    }
  }
}

int
compar(const void *xa, const void *xb)
{
  char *a = *(char**)xa;
  char *b = *(char**)xb;
  return strcmp(a, b);
}

void
dircheck(const char *d, int nf)
{
  DIR *dp;
  struct dirent *e;
  char *names[1000];
  int nnames = 0, i;

  dp = opendir(d);
  if(dp == 0){
    fprintf(stderr, "selftest: opendir(%s): %s\n", d, strerror(errno));
    exit(1);
  }
  while((e = readdir(dp))){
    if(e->d_name[0] != '.'){
      if(nnames >= sizeof(names)/sizeof(names[0])){
        fprintf(stderr, "warning: too many files in %s\n", d);
      }
      names[nnames] = (char *) malloc(strlen(e->d_name) + 1);
      strcpy(names[nnames], e->d_name);
      nnames++;
    }
  }
  closedir(dp);

  if(nf != nnames){
    fprintf(stderr, "selftest: wanted %d dir entries, got %d\n", nf, nnames);
    exit(1);
  }

  /* check for duplicate entries */
  qsort(names, nnames, sizeof(names[0]), compar);
  for(i = 0; i < nnames-1; i++){
    if(strcmp(names[i], names[i+1]) == 0){
      fprintf(stderr, "selftest: duplicate directory entry for %s\n", names[i]);
      exit(1);
    }
  }

  for(i = 0; i < nnames; i++)
    free(names[i]);
}

void
reap (int pid)
{
  int wpid, status;
  wpid = waitpid (pid, &status, 0);
  if (wpid < 0) {
    perror("waitpid");
    exit(1);
  }
  if (wpid != pid) {
    fprintf(stderr, "unexpected pid reaped: %d\n", wpid);
    exit(1);
  }
  if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "child exited unhappily\n");
    exit(1);
  }
}

int
main(int argc, char *argv[])
{
  int pid, i;
  const char *name[5] = {"aa", "bb", "cc", "dd", "ee"};

  if(argc != 6){
    fprintf(stderr, "Usage: selftest dir1 dir2\n");
    exit(1);
  }
  sprintf(dn[0], "%s/d%d", argv[1], getpid());
  if(mkdir(dn[0], 0777) != 0){
    fprintf(stderr, "test-lab2-part2-a: failed: mkdir(%s): %s\n",
            dn[0], strerror(errno));
    exit(1);
  }
  for(int i=1; i<5; i++){
    sprintf(dn[i], "%s/d%d", argv[i+1], getpid());
    if(access(dn[i], 0) != 0){
      fprintf(stderr, "test-lab2-part2-a: failed: access(%s) after mkdir %s: %s\n",
              dn[i], dn[0], strerror(errno));
      exit(1);
    }
  }
  setbuf(stdout, 0);

  for(i = 0; i < sizeof(big)-1; i++)
    big[i] = 'x';
  for(i = 0; i < sizeof(huge)-1; i++)
    huge[i] = '0';

  printf("More yfsclient Concurrent creates: ");
  for(int i=0; i<4; i++){
    pid = fork();
    if(pid < 0){
      perror("selftest: fork");
      exit(1);
    }
    if(pid == 0){
      createn(dn[i+1], name[i+1], 2, false);
      printf("I'm son %d createn\n", i);
      exit(0);
    }
    createn(dn[i], name[i], 2, false);
    printf("I'm parent %d createn\n", i);
    sleep(2);
    reap(pid);
    printf("I'm parent %d reap\n", i);
    dircheck(dn[0], 4 + i*2);
    printf("I'm parent %d dircheck\n", i); 
    checkn(dn[i], name[i+1], 2);
    checkn(dn[i+1], name[i], 2);
    printf("I'm parent %d checkn\n", i);
    unlinkn(dn[i], name[i+1], 2);
    printf("I'm parent %d unlink\n", i);
  }
  printf("OK\n");

  exit(0);
  return(0);
}
