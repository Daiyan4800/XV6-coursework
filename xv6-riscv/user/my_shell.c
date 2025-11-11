#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/* Print the prompt ">>> " and read a line of characters
   from stdin. */
int getcmd(char *buf, int nbuf) {
  fprintf(2, ">>> ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

/*
  A recursive function which parses the command
  at *buf and executes it.
*/
__attribute__((noreturn))
void run_command(char *buf, int nbuf, int *pcp) {
  char *arguments[10];
  int numargs = 0;
  int ws = 1;
  int we = 0;
  int redirection_left = 0;
  int redirection_right = 0;
  char *file_name_l = 0;
  char *file_name_r = 0;
  int p[2];
  int pipe_cmd = 0;
  int sequence_cmd = 0;

  int i = 0;
  for (; i < nbuf && buf[i] != '\0'; i++) {
    // Parse current character and set flags
    if (buf[i] == '|') {
      pipe_cmd = 1;
      if (numargs > 0) {
        arguments[numargs] = 0;
        numargs++;
      }
      ws = 1;
      we = 0;
    } else if (buf[i] == '<') {
      redirection_left = 1;
      ws = 1;
      we = 0;
    } else if (buf[i] == '>') {
      redirection_right = 1;
      ws = 1;
      we = 0;
    } else if (buf[i] == ';') {
      sequence_cmd = 1;
      if (numargs > 0) {
        arguments[numargs] = 0;
        numargs++;
      }
      buf[i] = 0;
      break;
    } else if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n') {
      if (!we && numargs > 0) {
        buf[i] = 0;
        we = 1;
        ws = 1;
      }
    } else {
      if (ws) {
        if (redirection_left && !file_name_l) {
          file_name_l = &buf[i];
        } else if (redirection_right && !file_name_r) {
          file_name_r = &buf[i];
        } else if (numargs < 9) {
          arguments[numargs++] = &buf[i];
        }
        ws = 0;
        we = 0;
      }
    }
  }

  if (numargs > 0 && arguments[numargs-1] != 0) {
    arguments[numargs] = 0;
  }

  if (sequence_cmd) {
    sequence_cmd = 0;
    if (fork() == 0) {
      run_command(buf, nbuf, pcp);
    } else {
      wait(0);
      if (i+1 < nbuf && buf[i+1] != '\0') {
        run_command(&buf[i+1], nbuf - i - 1, pcp);
      }
    }
    exit(0);
  }

  if (redirection_left && file_name_l) {
    close(0);
    if(open(file_name_l, O_RDONLY) < 0) {
      fprintf(2, "cannot open %s\n", file_name_l);
      exit(1);
    }
  }
  
  if (redirection_right && file_name_r) {
    close(1);
    if(open(file_name_r, O_WRONLY|O_CREATE) < 0) {
      fprintf(2, "cannot open %s\n", file_name_r);
      exit(1);
    }
  }

  if (strcmp(arguments[0], "cd") == 0) {
    if (numargs < 2) {
      fprintf(2, "cd: missing argument\n");
    } else {
      write(pcp[1], arguments[1], strlen(arguments[1]) + 1);
    }
    exit(2);
  } else {
    if (pipe_cmd) {
      pipe(p);
      if (fork() == 0) {
        close(1);
        dup(p[1]);
        close(p[0]);
        close(p[1]);
        if (exec(arguments[0], arguments) < 0) {
          fprintf(2, "exec %s failed\n", arguments[0]);
          exit(1);
        }
      } else {
        if (fork() == 0) {
          close(0);
          dup(p[0]);
          close(p[0]);
          close(p[1]);
          char *next_args[10];
          int next_numargs = 0;
          int found_pipe = 0;
          
          for (int j = 0; j < numargs; j++) {
            if (found_pipe && next_numargs < 9) {
              next_args[next_numargs++] = arguments[j];
            }
            if (arguments[j] == 0) {
              found_pipe = 1;
            }
          }
          next_args[next_numargs] = 0;
          
          if (next_numargs > 0) {
            run_command(buf + (next_args[0] - buf), nbuf - (next_args[0] - buf), pcp);
          }
          exit(0);
        } else {
          close(p[0]);
          close(p[1]);
          wait(0);
          wait(0);
        }
      }
    } else {
      if (exec(arguments[0], arguments) < 0) {
        fprintf(2, "exec %s failed\n", arguments[0]);
        exit(1);
      }
    }
  }
  exit(0);
}

int main(void) {
  static char buf[100];
  int pcp[2];
  pipe(pcp);

  while(getcmd(buf, sizeof(buf)) >= 0) {
    if(fork() == 0) {
      run_command(buf, 100, pcp);
    } else {
      int child_status;
      wait(&child_status);
      if (child_status == 2 * 256) { // CD command executed in child
        char cd_path[100];
        read(pcp[0], cd_path, sizeof(cd_path));
        if(chdir(cd_path) < 0) {
          fprintf(2, "cannot cd %s\n", cd_path);
        }
      }
    }
  }
  exit(0);
}