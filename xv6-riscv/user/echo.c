#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;

  for(i = 1; i < argc; i++){
    printf("%s", argv[i]);
    if(i + 1 < argc){
      printf(" ");
    } else {
      printf("\n");
    }
  }
  exit(0);
};

