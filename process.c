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

  printf("Calling Process `p_proc`:\n");
  printf("  PPID: %d\n", getppid());
  printf("  PID : %d\n", getpid());
  printf("---------------------------\n");
  
  // 1. Process creation (fork)
  printf("Calling fork.....\n");
  pid = fork();
  
  if (pid == -1) {
    perror("fork failed");
    printf("`p_proc`: return value from fork(): %d\n", pid);
    exit(EXIT_FAILURE);
  }
  
  if (pid == 0) {
    printf("Cloned Process `c_proc`:\n");
    printf("  PPID: %d\n", getppid());
    printf("  PID : %d\n", getpid());
    printf("Return value from fork() to `c_proc`: %d\n", pid);
    printf("---------------------------\n");

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
      printf("---------------------------\n");
      printf("Return value from fork(): to `p_proc` %d\n", pid);
    } else {
      printf("Child did not exit normally.\n");
    }
  }

  return 0;
}