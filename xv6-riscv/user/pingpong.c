#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  int p2c[2]; // pipe: parent -> child
  int c2p[2]; // pipe: child -> parent
  char byte = 'P';

  pipe(p2c);
  pipe(c2p);

  int pid = fork();

  if (pid > 0) {
    // ----- Parent process -----
    close(p2c[0]); // parent doesn't read from p2c
    close(c2p[1]); // parent doesn't write to c2p

    // send 'P' to child
    write(p2c[1], &byte, 1);

    // wait for child to respond
    char received;
    read(c2p[0], &received, 1);

    int my_pid = getpid();
    printf("%d: received pong '%c' from child %d\n", my_pid, received, pid);

    close(p2c[1]);
    close(c2p[0]);
    wait((int *)0);
  } else if (pid == 0) {
    // ----- Child process -----
    close(p2c[1]); // child doesn't write to p2c
    close(c2p[0]); // child doesn't read from c2p

    char received;
    read(p2c[0], &received, 1);

    int my_pid = getpid();
    printf("%d: received ping '%c' from parent\n", my_pid, received);

    // send response back
    char reply = 'R';
    write(c2p[1], &reply, 1);

    close(p2c[0]);
    close(c2p[1]);
    exit(0);
  }

  exit(0);
}
