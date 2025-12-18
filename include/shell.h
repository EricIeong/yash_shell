#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_PIPES 5

// Structure for process info
typedef struct {
    char state;
    int cpuid;
    double utime;
    double stime;
    long vsize;
    long minflt;
    long majflt;
} proc_info_t;

// Function declarations
void display_prompt();
char *read_input();
char **parse_input(char *input);
int execute_command(char **args);
int execute_piped_commands(char ***commands, int num_commands);
void handle_sigint(int sig);
int is_builtin_command(char **args);
int execute_builtin(char **args);
int builtin_exit(char **args);
int builtin_watch(char **args);
int get_process_info(pid_t pid, proc_info_t *info);
void print_proc_info(const proc_info_t *info);
void remove_zombies();
void print_signal_info(int status);

#endif