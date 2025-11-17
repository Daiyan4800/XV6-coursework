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
void run_command(char *buf, int nbuf, int *pcp) {
  char *arguments[32];
  int numargs = 0;
  int ws = 1;
  int redirection_left = 0;
  int redirection_right = 0;
  char *file_name_l = 0;
  char *file_name_r = 0;
  int pipe_cmd = 0;
  char *pipe_position = 0;
  int sequence_cmd = 0;
  char *sequence_position = 0;

  // Parse the command line
  for (int i = 0; i < nbuf && buf[i] != '\0'; i++) {
    if (buf[i] == '|' && !pipe_position) {
      pipe_cmd = 1;
      pipe_position = &buf[i];
      buf[i] = 0; // Null terminate for first command
    } else if (buf[i] == ';' && !sequence_position) {
      sequence_cmd = 1;
      sequence_position = &buf[i];
      buf[i] = 0; // Null terminate for current command
    }
  }

  // Parse arguments for current command (before pipe or sequence)
  char *cmd_buf = buf;
  for (int i = 0; cmd_buf[i] != '\0' && i < nbuf; i++) {
    if (cmd_buf[i] == ' ' || cmd_buf[i] == '\t' || cmd_buf[i] == '\n') {
      if (!ws) {
        cmd_buf[i] = 0;
        ws = 1;
      }
    } else if (cmd_buf[i] == '<') {
      redirection_left = 1;
      ws = 1;
    } else if (cmd_buf[i] == '>') {
      redirection_right = 1;
      ws = 1;
    } else {
      if (ws) {
        if (redirection_left && !file_name_l) {
          file_name_l = &cmd_buf[i];
        } else if (redirection_right && !file_name_r) {
          file_name_r = &cmd_buf[i];
        } else if (numargs < 31) {
          arguments[numargs++] = &cmd_buf[i];
        }
        ws = 0;
      }
    }
  }
  
  if (numargs > 0) {
    arguments[numargs] = 0;
  }

  // Handle sequence operator - execute current command then the next
  if (sequence_cmd && sequence_position) {
    if (fork() == 0) {
      run_command(cmd_buf, nbuf, pcp);
      exit(0);
    } else {
      wait(0);
      if (sequence_position[1] != '\0') {
        run_command(&sequence_position[1], nbuf - (&sequence_position[1] - buf), pcp);
      }
      return;
    }
  }

  // Handle pipe
  if (pipe_cmd && pipe_position) {
    int p[2];
    pipe(p);
    
    if (fork() == 0) {
      // Child process - left side of pipe
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      
      // Handle redirections for left command
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
        if (exec(arguments[0], arguments) < 0) {
          fprintf(2, "exec %s failed\n", arguments[0]);
          exit(1);
        }
      }
    } else {
      if (fork() == 0) {
        // Child process - right side of pipe
        close(0);
        dup(p[0]);
        close(p[0]);
        close(p[1]);
        
        // Recursively handle the right side of the pipe
        run_command(&pipe_position[1], nbuf - (&pipe_position[1] - buf), pcp);
        exit(0);
      } else {
        // Parent process
        close(p[0]);
        close(p[1]);
        wait(0);
        wait(0);
      }
    }
  } else {
    // No pipe - regular command execution
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
      if (exec(arguments[0], arguments) < 0) {
        fprintf(2, "exec %s failed\n", arguments[0]);
        exit(1);
      }
    }
  }
}

int main(void) {
  static char buf[100];
  int pcp[2];
  pipe(pcp);

  while(getcmd(buf, sizeof(buf)) >= 0) {
    if(fork() == 0) {
      run_command(buf, sizeof(buf), pcp);
      exit(0);
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