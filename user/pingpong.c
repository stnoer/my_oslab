#include "kernel/types.h"
#include "user.h"

int main(int argc, char *argv[]) {
  int c2f[2], f2c[2];
  pipe(c2f);  // child -> parent
  pipe(f2c);  // parent ->child
  int fpid = getpid(), cpid;
  char ping[100];
  char pong[100];

  if ((cpid = fork()) == 0) {
    close(f2c[1]);
    read(f2c[0], ping, sizeof(ping) - 1);
    printf("%d: received %s from pid %d\n", getpid(), ping, fpid);
    close(f2c[0]);

    close(c2f[0]);
    write(c2f[1], "pong", 4);
    close(c2f[1]);
  } else {
    close(f2c[0]);
    write(f2c[1], "ping", 4);
    close(f2c[1]);

    close(c2f[1]);
    read(c2f[0], pong, sizeof(pong) - 1);
    printf("%d: received %s from pid %d\n", fpid, pong, cpid);
    close(c2f[1]);
  }

  exit(0);
}