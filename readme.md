# Processes In Linux

A process is an instance of a running program. Every time you execute a command or run a program, the Linux kernel creates a process to run it.

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
  - Every process is a decendent of the `init` process.

To view the tree like structure of the processes, use `pstree`

Why does process hierarchy exists? To understand this, we need to understand how processes are created.

## Process Creation

It is a 4 step process.

| Step | Description                                           | Function |
| ---- | ----------------------------------------------------- | -------- |
| 1    | Clone the current process                             | `fork()` |
| 2    | Replaces the process image with the new program       | `exec()` |
| 3    | Parent waits for the child to finish                  | `wait()` |
| 4    | Process finishes and returns the status to the parent | `exit()` |

Each process may require other helper processes.
  - For example, when I did `pstree`, I found that VS Code is not a standalone process. It has a lot of subprocesses in itself.
  - The same is with firefox and other processes.

Hirarchial structure is not just a design philosophy, but a very logical decision.
  - It is a proper way to manage which process spawned which subprocess.

Now the question is, how can this be done?
  - The child process must have the identifying attributes from the parent process. This is the requirement.
  - The most straightforward way to do this is to make a clone of parent process, keep the metadata attributes the same and change the process-specific attributes accordingly.
  - And this is what the above mentioned 4-step process is doing.

## Let's Go Practical

We are going to run an ELF binary made up of this C code:
```c
#include<stdio.h>
#include<unistd.h>

void main(){
  printf("Hello, World!");
  sleep(400);
}
```
and see how linux do all the magic.

To get the final ELF binary:
```bash
gcc main.c -o main_exe
```

Check:
```bash
file main_exe

main_exe: ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, for GNU/Linux 3.2.0, not stripped
```

### Step1 - Call The Binary

To call or execute the binary (`main_exe`), we need a shell (or terminal).

Now I have opened my shell, which is zsh.
```zsh
$ which zsh

/usr/bin/zsh
```

`zsh` itself is a "running" process. Lets find out.
```zsh
$ ps
  
PID TTY          TIME CMD
41027 pts/0    00:00:01 zsh
49852 pts/0    00:00:00 ps
```
This shows that zsh has a process ID of 41027.

Now I ran the binary.
```zsh
./main_exe
```

Lets do ps again. But the shell is occupied now. Lets run this binary in background.
```zsh
$ ./main_exe

[1] 52184
```

We can also verify the process IDs again.
```zsh
$ ps                

PID TTY          TIME CMD
41027 pts/0    00:00:02 zsh
52184 pts/0    00:00:00 main_exe
52325 pts/0    00:00:00 ps
```

Since ./main_exe is executed within zsh, zsh must be the parent of main_exe? Lets verify this.
```zsh
$ ps -T -o pid,ppid,cmd

PID       PPID      CMD
41027     40797     /usr/bin/zsh -i
59461     41027     ./main_exe
59559     41027     ps -T -o pid,ppid,cmd
```
  + -T shows processes for the current terminal session.
  + -o helps in custom formatting.
  + But this shows that main_exe is a child of zsh.

This proves that `fork()` was called upon the `zsh` process.
  - Now, `execve()` is called inside this child process

### Step2 - `execve()`

execve is a syscall, which replaces the current process image.

A fork is a new-clone of the parent process. But the child has a differnt task than the parent. Therefore, the process image has to be changed in order to reflect that.
  - This is what `execve()` is purposed for.

The kernel opens `main_exe` ELF file using virtual file system (VFS).
  - Now it reads the ELF Header (first 64 bytes) to confirm that it is an ELF file, and find the `e_type`, `e_entry` and Program Headers Table (PHT) for the later work.

The kernel loads the binary into memory by reading PHT. This is the PHT for our ELF:
```bash
$ readelf -l main_exe

Elf file type is DYN (Position-Independent Executable file)
Entry point 0x1060
There are 14 program headers, starting at offset 64

Program Headers:
  Type           Offset             VirtAddr           PhysAddr
                 FileSiz            MemSiz              Flags  Align
  PHDR           0x0000000000000040 0x0000000000000040 0x0000000000000040
                 0x0000000000000310 0x0000000000000310  R      0x8
  INTERP         0x0000000000000394 0x0000000000000394 0x0000000000000394
                 0x000000000000001c 0x000000000000001c  R      0x1
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
  LOAD           0x0000000000000000 0x0000000000000000 0x0000000000000000
                 0x0000000000000660 0x0000000000000660  R      0x1000
  LOAD           0x0000000000001000 0x0000000000001000 0x0000000000001000
                 0x0000000000000179 0x0000000000000179  R E    0x1000
  LOAD           0x0000000000002000 0x0000000000002000 0x0000000000002000
                 0x000000000000010c 0x000000000000010c  R      0x1000
  LOAD           0x0000000000002dd0 0x0000000000003dd0 0x0000000000003dd0
                 0x0000000000000250 0x0000000000000258  RW     0x1000
  DYNAMIC        0x0000000000002de0 0x0000000000003de0 0x0000000000003de0
                 0x00000000000001e0 0x00000000000001e0  RW     0x8
  NOTE           0x0000000000000350 0x0000000000000350 0x0000000000000350
                 0x0000000000000020 0x0000000000000020  R      0x8
  NOTE           0x0000000000000370 0x0000000000000370 0x0000000000000370
                 0x0000000000000024 0x0000000000000024  R      0x4
  NOTE           0x00000000000020ec 0x00000000000020ec 0x00000000000020ec
                 0x0000000000000020 0x0000000000000020  R      0x4
  GNU_PROPERTY   0x0000000000000350 0x0000000000000350 0x0000000000000350
                 0x0000000000000020 0x0000000000000020  R      0x8
  GNU_EH_FRAME   0x0000000000002014 0x0000000000002014 0x0000000000002014
                 0x000000000000002c 0x000000000000002c  R      0x4
  GNU_STACK      0x0000000000000000 0x0000000000000000 0x0000000000000000
                 0x0000000000000000 0x0000000000000000  RW     0x10
  GNU_RELRO      0x0000000000002dd0 0x0000000000003dd0 0x0000000000003dd0
                 0x0000000000000230 0x0000000000000230  R      0x1

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

PHT describes how the operating system should load the ELF binary into memory. It essentially maps parts of the binary file into memory regions with specific permissions and purposes.

A simple decode of this cryptic table is:
  - Type: PHDR
    ```
    Offset:  0x40
    VirtAddr:0x40
    Size:    0x310
    Flags:   R
    ```
    This describes where in memory the program headers themselves are located.

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

### Step 3 - 