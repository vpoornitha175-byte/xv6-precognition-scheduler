
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

  if(x == 123456789)
    printf("x=%d\n", x);
}

int
main(int argc, char *argv[])
{
  int pid;

  printf("Precognition scheduler test starting\n");

  pid = fork();

  if(pid == 0){
    cpu_work(10000);
    printf("Child 1 finished\n");
    exit(0);
  }

  pid = fork();

  if(pid == 0){
    cpu_work(50000);
    printf("Child 2 finished\n");
    exit(0);
  }

  pid = fork();

  if(pid == 0){
    cpu_work(100000);
    printf("Child 3 finished\n");
    exit(0);
  }

  wait(0);
  wait(0);
  wait(0);

  printf("Precognition scheduler test finished\n");

  exit(0);
}
