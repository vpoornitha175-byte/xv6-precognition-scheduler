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
  int start;
  int end;

  printf("\n=== PRECOGNITION FAIRNESS TEST ===\n");

  start = uptime();

  pid = fork();

  if(pid == 0){
    printf("Process 1: Small workload started\n");

    cpu_work(10000);

    printf("Process 1: Small workload completed at tick %d\n",
           uptime());

    printf("Scheduling count: %d\n", getprecogcount());

    exit(0);
  }

  pid = fork();

  if(pid == 0){
    printf("Process 2: Medium workload started\n");

    cpu_work(50000);

    printf("Process 2: Medium workload completed at tick %d\n",
           uptime());

    printf("Scheduling count: %d\n", getprecogcount());

    exit(0);
  }

  pid = fork();

  if(pid == 0){
    printf("Process 3: Large workload started\n");

    cpu_work(100000);

    printf("Process 3: Large workload completed at tick %d\n",
           uptime());

    printf("Scheduling count: %d\n", getprecogcount());

    exit(0);
  }

  wait(0);
  wait(0);
  wait(0);

  end = uptime();

  printf("\nAll processes completed\n");
  printf("Total elapsed ticks: %d\n", end - start);

  printf("=== FAIRNESS TEST FINISHED ===\n");

  exit(0);
}
