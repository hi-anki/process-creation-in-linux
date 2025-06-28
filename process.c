// Standard I/O: printf() and perror()
#include <stdio.h>

// General Utils: exit()
#include <stdlib.h>

// POSIX OS API: fork(), execvp(), getpid(), getppid()
#include <unistd.h>

// Defines data types used in system calls: pid_t, the data type for process IDs
#include <sys/types.h>

// Provides macros and functions for waiting on child processes: waitpid(), WIFEXITED(), WEXITSTATUS()
#include <sys/wait.h>

int main() {
  pid_t pid;

  printf("Parent PID: %d\n", getpid());

  // 1. Process creation (fork)
  pid = fork();

  if (pid < 0) {
    perror("fork failed");
    exit(EXIT_FAILURE);
  }

  if (pid == 0) {
    // Child process
    printf("Child PID: %d, PPID: %d\n", getpid(), getppid());

    // 2. Image replacement using exec
    char *args[] = {"./main_elf", NULL};
    if (execvp(args[0], args) == -1) {
      perror("exec failed");
      exit(EXIT_FAILURE);
    }
  }

  else {
    // Parent process
    int status;
    waitpid(pid, &status, 0);  // Wait for child to finish

    if (WIFEXITED(status)) {
      printf("Child exited with status %d\n", WEXITSTATUS(status));
    } else {
      printf("Child did not exit normally.\n");
    }
  }

  return 0;
}