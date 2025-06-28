# Processes In Linux

A process is an instance of a running program. Every time you execute a command or run a program, the Linux kernel creates a process to run it.

Every process gets a virtual address space, which is mostly made up of the program image.

## Key Properties Of A Process

| Property                 | Description                                                   |
| ------------------------ | ------------------------------------------------------------- |
| PID (Process ID)         | Unique identifier for every process                           |
| PPID (Parent Process ID) | Unique identifier of the process that created the PID process |
| UID (User ID)            | Who owns the process                                          |
| State                    | Running, sleeping, zombie etc....                             |
| Memory Info              | RAM consumption                                               |
| Executable code and data | What it’s running and working with                            |

## Process Hierarchy

Processes follow a tree like structure in linux.
  - The first process is `init` or `systemd`, depending on the system, which is started by the kernel. It's PID is 1, and PPID is 0.
  - Every process is created by another process (its parent process).
  - Every process is a descendant of the `init` process.
  - Every process is independent in nature.

To view the tree like structure of the processes, use `pstree`

Why does process hierarchy exists?
  - To understand this, we need to understand how processes are created.

## Process Creation

It is a 4 step process.

| Step | Description | Syscall |
| ---- | ------------------------- | -------- |
| 1 | Clone the current process | `fork()` |
| 2 | Replaces the process image with the new program in the cloned process | `exec()` |
| 3 | Parent waits for the child to finish | `wait()` |
| 4 | Process finishes and returns the status to the parent | `exit()` |

Each process may require other helper processes.
  - For example, when I did `pstree`, I found that VS Code is not a standalone process. It has a lot of subprocesses in itself.
  - The same is with firefox and other processes.

Hierarchical structure is not just a design philosophy, but a very logical decision.
  - It is a proper way to manage which process spawned which subprocess.

Now the question is, how can this be done?
  - The child process must have the identifying attributes of the parent process.
  - The most straightforward way to do this is to make a clone of the parent process, keep the metadata attributes the same and change the process-specific attributes accordingly.
  - And this is what the above mentioned 4-step process is doing.

# Lets Observe The Behavior 

We are going to run an ELF binary made from this C code and see how linux does all the magic.
```c
#include <stdio.h>
#include <unistd.h>    // sleep()

int main(void){
  printf("Hello, World!");
  sleep(400);
}
```

To get the final ELF binary:
```bash
gcc main.c -o main_elf
```

## Step1 - Call The Binary

To call or execute the binary (`main_elf`), we need a shell (or terminal).

I have opened my shell, which is zsh.
```zsh
$ which zsh

/usr/bin/zsh
```

`zsh` itself is a "running" process.
```zsh
$ ps
  
PID       TTY       TIME         CMD
41027     pts/0     00:00:01     zsh
49852     pts/0     00:00:00     ps
```

Run the binary.
```zsh
./main_elf
```

Lets do ps again. But the shell is occupied now. Lets run the binary in background.
```zsh
$ ./main_elf &

[1] 52184
```

Final State:
```zsh
$ ps                

PID       TTY       TIME         CMD
41027     pts/0     00:00:02     zsh
52184     pts/0     00:00:00     main_elf
52325     pts/0     00:00:00     ps
```

Since main_elf is executed within zsh, zsh must be the parent of main_elf? Lets verify this.
```zsh
$ ps -T -o pid,ppid,cmd

PID     PPID    CMD
41027   40797   /usr/bin/zsh -i
59461   41027   ./main_elf
59559   41027   ps -T -o pid,ppid,cmd
```
  + -T shows processes for the current terminal session.
  + -o helps in custom formatting.

This proves that `fork()` was called upon the `zsh` process.

Until now, we can say that a base template for the child process is created.
  - Now we have to find the evidence for the process image replacement.

## Step2 - Correct (Replace) The Process Image In The Child Process (Fork)

A fork is a near-clone of the parent process. But the child process is a different program than the parent. Therefore, the process image has to be changed.

What is `execve`?
  - `execve` is a syscall which executes the binary passed in the pathname argument by replacing the process image of the current process.
  - This design exists because `execve` is meant to be paired with `fork` in order to fit Linux's hierarchical process structure.

What is a process image?
  - A process image is the complete in-memory layout of a program after it has been loaded into memory by the OS, and before or during execution.
  - It is the answer to the question, "What the process looks like in the RAM?"
  - It includes code, data, stack, heap, environment, memory-mapped regions, loaded libraries etc....
  - A process image is the memory representation of a program at runtime.
  - It is created by the kernel during `execve()`, based on ELF layout.

This is the whole process that `execve` syscalls carries out.

The kernel opens `main_elf` file using virtual file system (VFS).
  - Now it reads the ELF Header (first 64 bytes) to confirm that it is an ELF file, and find the `e_type`, `e_entry` and Program Headers Table (PHT) for the later work.

What is VFS and Why the kernel is using it?
  - It's an abstraction layer inside the Linux kernel that provides a uniform interface to access all kinds of file systems — regardless of their actual formats or physical devices.
  - There exist multiple file systems, like ext4, btrfs, zfs, hfs, ntfs, fat32 and so on.... If there is no VFS, the kernel has to learn to speak all the different file systems.
  - VFS knows how to talk to different file systems and provide the kernel with a consistent interface.

The kernel loads the binary into the memory by reading the PHT.
  - It maps each segment defined in the PHT into memory regions using `mmap()` syscall and sets the permissions (R, W, X) accordingly.
  - This is the PHT for our ELF: 
    ```bash
    $ readelf -l main_elf

    Elf file type is DYN (Position-Independent Executable file)
    Entry point 0x1060
    There are 14 program headers, starting at offset 64

    Program Headers:
      Type           Offset             VirtAddr           PhysAddr           FileSiz            MemSiz              Flags  Align
      PHDR           0x0000000000000040 0x0000000000000040 0x0000000000000040 0x0000000000000310 0x0000000000000310  R      0x8
      INTERP         0x0000000000000394 0x0000000000000394 0x0000000000000394 0x000000000000001c 0x000000000000001c  R      0x1
                    [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
      LOAD           0x0000000000000000 0x0000000000000000 0x0000000000000000 0x0000000000000660 0x0000000000000660  R      0x1000
      LOAD           0x0000000000001000 0x0000000000001000 0x0000000000001000 0x0000000000000179 0x0000000000000179  R E    0x1000
      LOAD           0x0000000000002000 0x0000000000002000 0x0000000000002000 0x000000000000010c 0x000000000000010c  R      0x1000
      LOAD           0x0000000000002dd0 0x0000000000003dd0 0x0000000000003dd0 0x0000000000000250 0x0000000000000258  RW     0x1000
      DYNAMIC        0x0000000000002de0 0x0000000000003de0 0x0000000000003de0 0x00000000000001e0 0x00000000000001e0  RW     0x8
      NOTE           0x0000000000000350 0x0000000000000350 0x0000000000000350 0x0000000000000020 0x0000000000000020  R      0x8
      NOTE           0x0000000000000370 0x0000000000000370 0x0000000000000370 0x0000000000000024 0x0000000000000024  R      0x4
      NOTE           0x00000000000020ec 0x00000000000020ec 0x00000000000020ec 0x0000000000000020 0x0000000000000020  R      0x4
      GNU_PROPERTY   0x0000000000000350 0x0000000000000350 0x0000000000000350 0x0000000000000020 0x0000000000000020  R      0x8
      GNU_EH_FRAME   0x0000000000002014 0x0000000000002014 0x0000000000002014 0x000000000000002c 0x000000000000002c  R      0x4
      GNU_STACK      0x0000000000000000 0x0000000000000000 0x0000000000000000 0x0000000000000000 0x0000000000000000  RW     0x10
      GNU_RELRO      0x0000000000002dd0 0x0000000000003dd0 0x0000000000003dd0 0x0000000000000230 0x0000000000000230  R      0x1

    Section to Segment mapping:
      Segment Sections...
      00     
      01     .interp 
      02     .note.gnu.property .note.gnu.build-id .interp .gnu.hash .dynsym .dynstr .gnu.version .gnu.version_r .rela.dyn .rela.plt 
      03     .init .plt .plt.got .text .fini 
      04     .rodata .eh_frame_hdr .eh_frame .note.ABI-tag 
      05     .init_array .fini_array .dynamic .got .got.plt .data .bss 
      06     .dynamic 
      07     .note.gnu.property 
      08     .note.gnu.build-id 
      09     .note.ABI-tag 
      10     .note.gnu.property 
      11     .eh_frame_hdr 
      12     
      13     .init_array .fini_array .dynamic .got
    ```

PHT describes how the operating system should load the ELF binary into the memory. It maps parts of the binary file into memory regions with specific permissions and purposes.

A simple decode of this cryptic table is:
  - Type: PHDR
    ```
    Offset:  0x40
    VirtAddr:0x40
    Size:    0x310
    Flags:   R
    ```
    This describes where in the memory the program headers themselves are located.

    The loader reads this to get all other segment info.

  - Type: INTERP
    ```
    Offset: 0x394
    VirtAddr: 0x394
    Size: 0x1c
    Flags: R
    [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
    ```
    Specifies the dynamic linker to load and run this PIE executable.

    This linker will resolve symbols and apply relocations before main() runs.

  - Type: LOAD Segments, The actual loadable code/data in the binary
    ```bash
    # LOAD 1
    Offset: 0x0
    VirtAddr: 0x0
    FileSiz: 0x660
    Flags: R
    ```
    The R flag shows that this is a read only section, contains the initial part of ELF.

    ```bash
    # LOAD 2
    Offset: 0x1000
    VirtAddr: 0x1000
    Size:    0x179
    Flags:   R E
    ```
    This is a read + executable section. This implies that it contains the actual code segment

    ```bash
    # LOAD 3
    Offset: 0x2000
    VirtAddr: 0x2000
    Size:    0x10c
    Flags:   R
    ```
    Another readonly segment, which may contain constants and other ro-data.

    ```bash
    # LOAD 4
    Offset: 0x2dd0
    VirtAddr: 0x3dd0
    Size: File=0x250, Mem=0x258
    Flags: RW
    ```
    This section is both readable and writeable. This is where .data, .bss, etc.... are stored.

  - Type: DYNAMIC
    ```bash
    Offset: 0x2de0
    VirtAddr: 0x3de0
    Size: 0x1e0
    Flags: RW
    ```
    Contains relocations, library names, symbol tables, etc...., which are used by the dynamic linker to perform symbol resolution and relocation at runtime.
  
  - Type: Note Segments and GNU_PROPERTY, for metadata handling. Nothing explosive.
  - Type: GNU_EH_FRAME, exception handling frame, used by debuggers and during crashes.
  - Type: GNU_STACK, specifies stack permissions.
  - Type GNU_RELRO, a region that is read-only after relocation.

The kernel finds the `LOAD` segments and maps them into memory.

Now our program's memory is loaded, but we're not executing yet.

### Handle Dynamic Linking

Since the PHT has INTERP header, this tells the kernel to run this interpreter `interpreter: /lib64/ld-linux-x86-64.so.2` before jumping to the entry point of the binary.

The kernel now loads the dynamic linker (`ld-linux-x86-64.so.2`) into memory via its own PHT.

The dynamic linker is the first code that will run in this process.

Now the kernel set up the stack, `rip` to ld-linux's entry point and returns the control to userspace.

And the child process is finally alive.

ld-linux now:
  1. Parses the `.dynamic` section (readelf -S main_elf)
  2. Finds the required shared libraries, like `libc.so.6`
  3. Loads them into memory using `mmap()`.
  4. Applies relocations to the code.
  5. Finally, jumps to our ELF binary's real entry point (not `main`, but `_start`).

### Entrypoint Mgmt && Program Execution

The dynamic linker jumps to _start in your binary (provided by crt1.o).

_start sets up the runtime.

Then it calls `__libc_start_main()`, a libc function that: initializes more stuff and finally calls the `main()`

Now the program runs.
  - `printf()` is a call into `libc`.
  - `sleep()` sleeps the process for 400 seconds using `nanosleep` syscall.

## Step3 - End Of The Program

After main() ends, control goes back to `__libc_start_main()`, which handles the final cleanup and calls `exit()`.

The kernel:
  + Cleans up process resources.
  + Returns exit code to parent (your shell).

# Lets Manage An Actual Process Using C

The source code of the program can be found here, [process.c](./process.c)

```c
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
```

Lets understand this program.

`pid_t` is just a type definition defined by POSIX to hold process IDs. It allows the kernel and user-space programs to use a consistent, portable data type for storing and passing process IDs.
  - The `pid` variable just holds an integer value returned by `fork()`.

fork() function is used to clone the calling process. It returns
  - `0` if the process is cloned successfully and we are in the context of the newly created child process.
  - `-1` if an error occurred
  - and the process ID of the new process to the old process.
  - What concerns us is 0, or -1. These are the two values that `pid` can actually hold.

The functions `getpid()` and `getppid()` are used to obtain the child process ID and the parent process ID, respectively.
  - These functions are relative to the process that has invoked them.
  - This is why `getpid()` before forking the process returned the process ID of the current process, which became the parent process after forking.

The `exec()` family of functions replaces the current process image with a new process image.
  - The `char *const argv[]` argument is an array of pointers to null-terminated strings that represent the argument list available to the new program.
  - The first argument, by convention, should point to the filename associated with the file being executed. The array of pointers must be terminated by a `null` pointer.

  - `execvp()` is wrapper build upon `execve` syscall. Internally, it is just:
    ```c
    int execvp(const char *file, char *const argv[]);
    ```
  - The first argument is a pointer to the binary which is to be executed and the second argument is an array to the arguments provided to the binary.
  - Why not `execvp(args)` directly?
    - Remember, `sys.argv[0]` is reserved to the filename in python. `$0` is reserved for the script name in bash. The same principle is followed here.

The `exec()` family of functions returns only when an error has occurred, which is -1.
  - Therefore, we are running the binary through `execvp` and matching if the return value is -1, to indicate failure or success.

The `waitpid()` function is like `async()` function in JavaScript, which waits for a longer process to finish and then adjusts the results appropriately, without stopping the current thread.
  - `waitpid()` also waits for the child process to finish.
  - After that some cleanup happens and we are done.

Now, lets understand the flow of execution, that's the most important thing here.
  - After forking the current process, now there exists two process with exact memory layout and execution context.
  - Both the processes continue executing the same code from the point where fork() returned. The only difference is the return value of fork(), which tells them if they're the parent or the child.
  - Since the first process has a `pid > 0`, it will go in either of the if-blocks.
    - But the forked process has a `pid = 0`, therefore it will go in the second if-block.
    - And since the first process has not gone into any of the if-blocks, it will go into the else-block.
  - That's how it works.
  - The first process then waits in the else part for the child to finish and then does the cleanup.

And that's how we finish establishing a baseline understanding in linux processes.

# When To Use What?

`execve` syscall has multiple wrappers as library functions. The questions is, which one to use when?

`execve(path, argv, envp)` is the raw signature of execve syscall.
```c
char *args[] = {"/path/to/binary", .., .., NULL};
char *envp[] = {"variable=value", "var2=val2", NULL}
execve(args[0], args, envp)
```

`execv(path, argv)`: inherits caller's environment.

`execl(path, arg0, arg1, ..., NULL)`: Same as `execv`, except that the arguments are directly passed as varargs, rather than an array.

`execvp(file, argv)`: searches `$PATH` variable for the binary. Good to run programs like a shell would do, like `ls` or `./exe`

`execlp(file, arg0, ...., argN, NULL)`: combines `execl + $PATH`

`execle(path, arg0, ..., NULL, envp)`: varargs + custom env

## Prefix Guide

v: vector: refers to an array/vector of arguments
l: list: refers to a list of arguments, passed as varargs
p: path: tells the function to search $PATH for the executable
e: environment: lets you explicitly pass the environment