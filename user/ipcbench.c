#include "kernel/types.h"
#include "user/user.h"

#define DEFAULT_ROUNDS 500
#define MAX_ROUNDS 10000

static int
read_value(int fd, int *value)
{
  char *p = (char *)value;
  int left = sizeof(*value);
  while (left > 0) {
    int n = read(fd, p, left);
    if (n <= 0)
      return -1;
    p += n;
    left -= n;
  }
  return 0;
}

static int
write_value(int fd, int value)
{
  char *p = (char *)&value;
  int left = sizeof(value);
  while (left > 0) {
    int n = write(fd, p, left);
    if (n <= 0)
      return -1;
    p += n;
    left -= n;
  }
  return 0;
}

int
main(int argc, char *argv[])
{
  int tochild[2], fromchild[2];
  int rounds = argc > 1 ? atoi(argv[1]) : DEFAULT_ROUNDS;
  int start, elapsed;
  int failed = 0;

  if (argc > 2 || rounds <= 0 || rounds > MAX_ROUNDS) {
    printf("usage: ipcbench [round-trips: 1-%d]\n", MAX_ROUNDS);
    exit(1);
  }
  if (pipe(tochild) < 0 || pipe(fromchild) < 0) {
    printf("ipcbench: pipe failed\n");
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    printf("ipcbench: fork failed\n");
    exit(1);
  }
  if (pid == 0) {
    close(tochild[1]);
    close(fromchild[0]);
    for (int i = 0; i < rounds; i++) {
      int value;
      if (read_value(tochild[0], &value) < 0 ||
          write_value(fromchild[1], value + 1) < 0)
        exit(1);
    }
    close(tochild[0]);
    close(fromchild[1]);
    exit(0);
  }

  close(tochild[0]);
  close(fromchild[1]);
  start = uptime();
  for (int i = 0; i < rounds; i++) {
    int reply;
    if (write_value(tochild[1], i) < 0 ||
        read_value(fromchild[0], &reply) < 0 || reply != i + 1) {
      failed = 1;
      break;
    }
  }
  elapsed = uptime() - start;
  close(tochild[1]);
  close(fromchild[0]);
  wait(0);

  if (failed) {
    printf("ipcbench: pipe round-trip failed\n");
    exit(1);
  }
  printf("Pipe ping-pong: %d round trips, %d ticks\n", rounds, elapsed);
  printf("Average latency: %d.%d ms/round trip\n",
         elapsed * 100 / rounds, (elapsed * 10000 / rounds) % 100);
  if (elapsed > 0)
    printf("Throughput: %d round trips/sec\n", rounds * 10 / elapsed);
  else
    printf("Throughput: <1 tick elapsed (increase round count)\n");
  exit(0);
}
