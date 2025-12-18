/*
 * Student: [Ieong Cheok Him]
 * ID: [3036273512]
 * Platform: [Docker]
 * 
 * Progress: I should have finished everything I think...
 * Gen AI usage: https://chat.deepseek.com/share/sztkbx9qpvr3mvmbbh
 */
#include <sys/resource.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
//constants
#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_PIPES 5

// Global variables
volatile sig_atomic_t sigint_received = 0;

//struct to store process info
typedef struct {
    char state;
    int cpuid;
    double utime;
    double stime;
    long vsize;
    long minflt;
    long majflt;
} proc_info_t;

// Function prototypes
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

int main() {
    char *input;  //string to store input
    char **args;  //string array to store parsed input
    int should_run = 1;
    int is_background = 0;
    
    // Set up signal handler for SIGINT
    signal(SIGINT, handle_sigint);
    
    while (should_run) {
        // Remove any zombie processes
        remove_zombies();
        
        // Reset SIGINT flag
        sigint_received = 0;
        
        // Display prompt and read input
        display_prompt();
        input = read_input();
        
        if (input == NULL) {
            continue; // Empty input or error, go to next loop
        }
        
        // Parse input
        args = parse_input(input);
        
        if (args[0] == NULL) {
            free(input);
            free(args);
            continue; // Empty command
        }
        
        // Check for built-in commands, watch and exit function
        if (is_builtin_command(args)) {
            should_run = execute_builtin(args); //should_run = 0 if exit, program ends
        } else {
            // Check for pipes in the command
            int pipe_count = 0;
            for (int i = 0; args[i] != NULL; i++) {
                if (strcmp(args[i], "|") == 0) {
                    pipe_count++;
                }
            }
            
            if (pipe_count > 0) {
                // Handle piped commands
                if (pipe_count > MAX_PIPES) {
                    fprintf(stderr, "Error: Maximum %d pipes supported\n", MAX_PIPES);
                } else {
                    // Parse piped commands
                    char ***commands = malloc((pipe_count + 2) * sizeof(char **));
                    int cmd_index = 0;
                    int arg_index = 0;
                    commands[cmd_index] = &args[arg_index];
                    
                    for (int i = 0; args[i] != NULL; i++) {
                        if (strcmp(args[i], "|") == 0) {
                            args[i] = NULL;
                            commands[++cmd_index] = &args[i + 1];
                        }
                    }
                    commands[cmd_index + 1] = NULL;
                    
                    execute_piped_commands(commands, cmd_index + 1);
                    free(commands);
                }
            } else {
                // Execute single command
                execute_command(args);
            }
        }
        
        // Clean up
        free(input);
        free(args);
    }
    
    return 0;
}

void display_prompt() {
    printf("## 3230yash >> ");
    fflush(stdout);
}

char *read_input() {
    // Allocate memory for the input string
    char *input = malloc(MAX_INPUT * sizeof(char));
    if (input == NULL) { //debug process
        perror("malloc failed");
        return NULL;
    }
    
    // Use fgets to read from stdin into the allocated memory
    if (fgets(input, MAX_INPUT, stdin) == NULL) {
        // Handle EOF (Ctrl+D) or error
        free(input);
        return NULL;
    }
    input[strcspn(input, "\n")] = '\0';
    return input; // Return pointer to the allocated memory containing the input
}

char **parse_input(char *input) {
    char **args = malloc(MAX_ARGS * sizeof(char *));
    char *token;
    int position = 0;
    
    token = strtok(input, " \t"); //separate out one token by spaces or tabs
    while (token != NULL && position < MAX_ARGS - 1) { //stops when all separated
        args[position] = token; //assign strings into pointers
        position++;
        token = strtok(NULL, " \t");
    }
    
    args[position] = NULL; //prevent unassigned bug
    return args;
}

int execute_command(char **args) {
    pid_t pid;
    int status;
    
    pid = fork();
    if (pid == 0) {
        // Child process
        execvp(args[0], args); //search and execute program from $PATH
        // If execvp returns, there was an error
        perror("3230yash");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("3230yash: fork failed");
        return -1;
    } else {
        // Parent ALWAYS waits for child
        waitpid(pid, &status, 0);
        if (WIFSIGNALED(status)) { //signal handling
            print_signal_info(status);
        }
    }
    return 0;
}

int execute_piped_commands(char ***commands, int num_commands) {
    if (num_commands < 2) {
        fprintf(stderr, "Error: Pipe requires at least 2 commands\n");
        return -1;
    }
    if (num_commands > MAX_PIPES + 1) {
        fprintf(stderr, "Error: Maximum %d pipes supported\n", MAX_PIPES);
        return -1;
    }

    int num_pipes = num_commands - 1;
    int pipefds[2 * num_pipes];
    pid_t pids[num_commands];

    // Create all pipes
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipefds + i * 2) == -1) {
            perror("3230yash: pipe creation failed");
            return -1;
        }
    }

    // Fork processes for each command
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();
        
        if (pids[i] == 0) {
            // Child process
            
            // Connect to previous command's output (if not first command)
            if (i > 0) {
                if (dup2(pipefds[(i-1)*2], STDIN_FILENO) == -1) {
                    perror("3230yash: dup2 stdin failed");
                    exit(EXIT_FAILURE);
                }
            }
            
            // Connect to next command's input (if not last command)
            if (i < num_commands - 1) {
                if (dup2(pipefds[i*2 + 1], STDOUT_FILENO) == -1) {
                    perror("3230yash: dup2 stdout failed");
                    exit(EXIT_FAILURE);
                }
            }
            
            // Close all pipe file descriptors in child
            for (int j = 0; j < 2 * num_pipes; j++) {
                close(pipefds[j]);
            }
            
            // Execute command
            execvp(commands[i][0], commands[i]);
            fprintf(stderr, "3230yash: command not found: %s\n", commands[i][0]);
            exit(EXIT_FAILURE);
            
        } else if (pids[i] < 0) {
            perror("3230yash: fork failed");
            // Clean up on fork failure
            for (int j = 0; j < 2 * num_pipes; j++) {
                close(pipefds[j]);
            }
            // Kill any already forked processes
            for (int j = 0; j < i; j++) {
                kill(pids[j], SIGTERM);
            }
            return -1;
        }
    }

    // Parent process - close all pipe ends
    for (int i = 0; i < 2 * num_pipes; i++) {
        close(pipefds[i]);
    }

    // Wait for all children to complete
    int exit_status = 0;
    for (int i = 0; i < num_commands; i++) {
        int status;
        waitpid(pids[i], &status, 0);
        
        if (WIFEXITED(status)) {
            // Return status of the last command (convention)
            if (i == num_commands - 1) {
                exit_status = WEXITSTATUS(status);
            }
        } else if (WIFSIGNALED(status)) {
            print_signal_info(status);
            if (i == num_commands - 1) {
                exit_status = 128 + WTERMSIG(status);
            }
        }
    }

    return exit_status;
}

void handle_sigint(int sig) {
    sigint_received = 1;
    printf("\n"); //next line
    display_prompt(); //ignore the SIGINT and display prompt again
}

int is_builtin_command(char **args) { //return 1 if the first argument is 'watch' or 'exit'
    if (args[0] == NULL) return 0;
    
    if (strcmp(args[0], "exit") == 0) return 1;
    if (strcmp(args[0], "watch") == 0) return 1;
    
    return 0;
}

int execute_builtin(char **args) {
    if (strcmp(args[0], "exit") == 0) {
        return builtin_exit(args);
    } else if (strcmp(args[0], "watch") == 0) {
        return builtin_watch(args);
    }
    
    return 1; // Continue running
}

int builtin_exit(char **args) {
    // Check for improper usage (extra arguments)
    if (args[1] != NULL) {
        printf("exit with other arguments!!!\n");
        return 1; // Continue running
    }
    
    printf("Terminated\n");
    return 0; // Stop running
}

int builtin_watch(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "watch: missing command\n");
        fprintf(stderr, "Usage: watch <command> [args...]\n");
        return 1;
    }

    // Check for pipes (not allowed in watch)
    for (int i = 1; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            fprintf(stderr, "3230yash: Cannot watch a pipe sequence\n");
            return 1;
        }
    }

    pid_t pid;
    int status;
    int pfd[2];
    
    if (pipe(pfd) == -1) {
        perror("3230yash: pipe failed");
        return 1;
    }

    #define MAX_SNAPSHOTS 100
    proc_info_t snapshots[MAX_SNAPSHOTS];
    int snapshot_count = 0;

    pid = fork();
    if (pid == 0) {
        // Child process
        close(pfd[1]);
        char start_signal;
        read(pfd[0], &start_signal, 1);
        close(pfd[0]);
        execvp(args[1], &args[1]);
        perror("3230yash");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("3230yash: fork failed");
        close(pfd[0]);
        close(pfd[1]);
        return 1;
    } else {
        // Parent process
        close(pfd[0]);
        
        printf("Command: ");
        for (int i = 1; args[i] != NULL; i++) {
            printf("%s ", args[i]);
        }
        printf("\n");
        
        // 1. Take INITIAL snapshot
        usleep(50000);  // Increased delay to 50ms
        if (snapshot_count < MAX_SNAPSHOTS) {
            if (get_process_info(pid, &snapshots[snapshot_count]) == 0) {
                snapshot_count++;
            }
        }
        
        // 2. Signal child to start
        write(pfd[1], "G", 1);
        close(pfd[1]);
        
        // 3. Monitor with shorter intervals for quick commands
        int monitoring = 1;
        int iterations = 0;
        int max_iterations = 20;  // Maximum 10 seconds (20 * 500ms)
        
        while (monitoring && iterations < max_iterations) {
            usleep(500000);  // 500ms
            
            // Take snapshot BEFORE checking status
            if (snapshot_count < MAX_SNAPSHOTS) {
                if (get_process_info(pid, &snapshots[snapshot_count]) == 0) {
                    snapshot_count++;
                }
            }
            
            // Check if child terminated
            if (waitpid(pid, &status, WNOHANG) == pid) {
                monitoring = 0;
            }
            
            iterations++;
        }
        
        // 4. Wait for final termination
        waitpid(pid, &status, 0);
        
        // 5. Take FINAL snapshot
        if (snapshot_count < MAX_SNAPSHOTS) {
            // Try multiple times as process might disappear quickly
            for (int attempt = 0; attempt < 3; attempt++) {
                if (get_process_info(pid, &snapshots[snapshot_count]) == 0) {
                    snapshot_count++;
                    break;
                }
                usleep(10000);
            }
        }
        
        // 6. PRINT ALL SNAPSHOTS
        printf("STATE  CPUID  UTIME   STIME    VSIZE      MINFLT   MAJFLT\n");
        for (int i = 0; i < snapshot_count; i++) {
            print_proc_info(&snapshots[i]);
        }
        
        printf("=== Process Monitoring Completed ===\n");
        
        if (WIFEXITED(status)) {
            printf("Process exited with status: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            print_signal_info(status);
        }
    }

    return 1;
}

// Get process information from /proc/[pid]/stat
int get_process_info(pid_t pid, proc_info_t *info) {
    char path[64];
    FILE *file;
    
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    file = fopen(path, "r");
    if (!file) {
        return -1;
    }
    
    char line[1024];
    if (fgets(line, sizeof(line), file)) {
        // Find the positions of parentheses
        char *first_paren = strchr(line, '(');
        char *last_paren = strrchr(line, ')');
        
        if (first_paren && last_paren) {
            // Extract the state character (it's right after the closing parenthesis)
            char state_char = last_paren[2]; // Skip ')' and space
            info->state = state_char;
        }
        
        // Replace command name with spaces for easier parsing
        if (first_paren && last_paren) {
            for (char *p = first_paren; p <= last_paren; p++) {
                *p = ' ';
            }
        }
        
        // Now parse the rest of the fields
        int unused_pid;
        int unused_ppid, unused_pgrp, unused_session, unused_tty, unused_tpgid;
        unsigned long flags;
        long minflt, cminflt, majflt, cmajflt, utime, stime;
        long vsize;
        int cpuid;
        
        int result = sscanf(line,
            "%d %*s %*c %d %d %d %d %d %lu %ld %ld %ld %ld %ld %ld %*d %*d %*d %*d %*d %*d %*d %ld %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %d",
            &unused_pid, &unused_ppid, &unused_pgrp, &unused_session, 
            &unused_tty, &unused_tpgid, &flags, &minflt, &cminflt, &majflt, &cmajflt,
            &utime, &stime, &vsize, &cpuid);
        
        fclose(file);
        
        if (result >= 14) {
            info->cpuid = cpuid;
            
            // Convert clock ticks to seconds
            long clock_ticks_per_sec = sysconf(_SC_CLK_TCK);
            info->utime = (double)utime / clock_ticks_per_sec;
            info->stime = (double)stime / clock_ticks_per_sec;
            
            info->vsize = vsize;
            info->minflt = minflt;
            info->majflt = majflt;
            return 0;
        }
    }
    
    fclose(file);
    return -1;
}

// Print process information in the required format
void print_proc_info(const proc_info_t *info) {
    // Handle zombie processes specially (they have vsize=0)
    if (info->vsize == 0) {
        printf("%-2c     %-4d   %-6.2f  %-6.2f   %-8ld   %-6ld   %-6ld\n",
               'Z',  // Force 'Z' for zombie
               info->cpuid,
               info->utime,
               info->stime,
               0L,   // vsize = 0 for zombie
               info->minflt,
               info->majflt);
    } else {
        printf("%-2c     %-4d   %-6.2f  %-6.2f   %-8ld   %-6ld   %-6ld\n",
               info->state,
               info->cpuid,
               info->utime,
               info->stime,
               info->vsize,
               info->minflt,
               info->majflt);
    }
}

// Print signal information when process is terminated by signal
void print_signal_info(int status) {
    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        printf("Process terminated by signal: %d (%s)\n", sig, strsignal(sig));
    }
}


void remove_zombies() {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Zombie processes removed
    }
}