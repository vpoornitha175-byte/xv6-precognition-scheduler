#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
cpu_work(int amount)
{
  volatile int i;
  volatile int j;
  volatile int x = 0;

  for(i = 0; i < amount; i++){
    for(j = 0; j < 1000; j++){
      x = x + i + j;
    }
  }

  // Prevent the compiler from completely removing the work.
  if(x == 123456789)
    printf("x=%d\n", x);
}

int
main(int argc, char *argv[])
{
  int pid;

  printf("Precognition scheduler test starting\n");

  // --------------------------------------------------
  // Child 1: Small CPU workload
  // --------------------------------------------------
  pid = fork();

  if(pid == 0){
    printf("Child 1: small CPU workload\n");

    cpu_work(1000);

    printf("Child 1 finished\n");
    exit(0);
  }

  // --------------------------------------------------
  // Child 2: Medium CPU workload
  // --------------------------------------------------
  pid = fork();

  if(pid == 0){
    printf("Child 2: medium CPU workload\n");

    cpu_work(5000);

    printf("Child 2 finished\n");
    exit(0);
  }

  // --------------------------------------------------
  // Child 3: Large CPU workload
  // --------------------------------------------------
  pid = fork();

  if(pid == 0){
    printf("Child 3: large CPU workload\n");

    cpu_work(10000);

    printf("Child 3 finished\n");
    exit(0);
  }

  // Parent waits for all children.
  wait(0);
  wait(0);
  wait(0);

  printf("Precognition scheduler test finished\n");

  exit(0);
}