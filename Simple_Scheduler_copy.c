#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

// Define constants for priority levels
#define MIN_PRIORITY 1
#define MAX_PRIORITY 100

long get_time() {
    struct timeval time, *address_time = &time;
    if (gettimeofday(address_time, NULL) != 0) {
        printf("Error in getting the time.\n");
        exit(1);
    }
    long epoch_time = time.tv_sec * 1000;
    return epoch_time + time.tv_usec / 1000;
}

char history[100][100];
int pid_history[100], child_pid;
long time_history[100][2], start_time;
bool flag_for_Input = true;
int count_history = 0, queue_head = 0, queue_tail = 0, NCPU, TSLICE;

typedef struct {
    int pid;
    char command[100];
    int priority; // Priority of the job
    long start_time;
    long end_time;
} Job;

Job jobs[100]; // Array to store job information

int add_to_history(char *command, int pid, int priority, long start_time_ms, long end_time_ms, int count_history) {
    strcpy(history[count_history], command);
    pid_history[count_history] = pid;
    jobs[count_history].pid = pid;
    jobs[count_history].priority = priority;
    jobs[count_history].start_time = start_time_ms;
    jobs[count_history].end_time = end_time_ms;
    return ++count_history;
}

void display_history() {
    printf("-------------------------------\n");
    printf("\n Command History: \n");
    printf("-------------------------------\n");

    for (int i = 0; i < count_history; i++) {
        printf("Command: %s\n", history[i]);
        printf("PID: %d\n", pid_history[i]);
        printf("Priority: %d\n", jobs[i].priority);
        printf("Start_Time: %ld\n", jobs[i].start_time);
        printf("End_Time: %ld\n", jobs[i].end_time);
        printf("-------------------------------\n");
    }
}

void signal_handler(int signum) {
    if (signum == SIGINT) {
        printf("\n---------------------------------\n");
        display_history();
        exit(0);
    }
}

void setup_signal_handler() {
    struct sigaction sh;
    sh.sa_handler = signal_handler;
    if (sigaction(SIGINT, &sh, NULL) != 0) {
        printf("Signal handling failed.\n");
        exit(1);
    }
    sigaction(SIGINT, &sh, NULL);
}

bool newline_checker(char *line, int len) {
    bool flag1 = false, flag2 = false;
    if (line[len - 1] == '\n') {
        flag1 = true;
    }
    if (line[len - 1] == '\r') {
        flag2 = true;
    }
    return flag1 || flag2;
}

void executeCommand(char **argv, int queue[], int priority) {
    int pid = fork();
    child_pid = pid;

    if (pid < 0) {
        printf("Forking child failed.\n");
        exit(1);
    } else if (pid == 0) { // Child process
        signal(SIGCONT, SIG_DFL);
        execvp(argv[0], argv);
        printf("Command failed.\n");
        exit(1);
    } else {
        queue[queue_head++] = pid;
        int ret;
        int pid = wait(&ret);

        if (WIFEXITED(ret)) {
            if (WEXITSTATUS(ret) == -1) {
                printf("Exit = -1\n");
            }
        } else {
            printf("\nAbnormal termination with pid :%d\n", pid);
        }

        return;
    }
}

void schedule(int signum, int queue[], int priorities[], int queue_size) {
    // Sort the jobs in the queue based on priority (higher priority first)
    for (int i = 0; i < queue_size - 1; i++) {
        for (int j = i + 1; j < queue_size; j++) {
            if (priorities[i] < priorities[j]) {
                int temp_pid = queue[i];
                int temp_priority = priorities[i];
                queue[i] = queue[j];
                priorities[i] = priorities[j];
                queue[j] = temp_pid;
                priorities[j] = temp_priority;
            }
        }
    }

    // Signal the first NCPU processes in the ready queue to start execution
    for (int i = 0; i < NCPU && i < queue_size; i++) {
        int pid = queue[i];
        kill(pid, SIGCONT);
    }

    // Pause the running processes after TSLICE milliseconds
    usleep(TSLICE * 1000);

    // Check for completed processes and remove them from the queue
    int i = queue_head;
    while (i < queue_size) {
        int pid = queue[i];
        int status;
        int result = waitpid(pid, &status, WNOHANG);
        if (result == -1) {
            // Error handling
        } else if (result == 0) {
            // The process is still running
            i++;
        } else {
            // The process has terminated, remove it from the queue
            queue_head++;
        }
    }

    // Requeue the paused processes to the rear of the ready queue
    for (int i = queue_tail; i < NCPU && i < queue_size; i++) {
        int pid = queue[i];
        kill(pid, SIGSTOP);
        queue[queue_head++] = pid;
    }
}

char **break_spaces(char *str) {
    char **command;
    char *sep = " \n";
    command = (char **)malloc(sizeof(char *) * 100);
    int len = 0;
    if (command == NULL) {
        printf("Memory allocation failed\n");
        exit(1);
    }

    int i = 0;
    char *token = strtok(str, sep);
    while (token != NULL) {
        len = strlen(token);
        command[i] = (char *)malloc(len + 1);
        if (command[i] == NULL) {
            printf("Memory allocation failed\n");
            exit(1);
        }

        strcpy(command[i], token);
        token = strtok(NULL, sep);
        i++;
    }
    command[i] = NULL;
    return command;
}

char *Input() {
    char *input_str = (char *)malloc(100);
    if (input_str == NULL) {
        printf("Memory allocation failed\n");
        exit(1);
    }
    flag_for_Input = false;
    fgets(input_str, 100, stdin);

    if (strlen(input_str) != 0 && input_str[0] != '\n' && input_str[0] != ' ') {
        flag_for_Input = true;
    }
    return input_str;
}

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        printf("All arguments not entered\n");
        exit(1);
    }

    NCPU = atoi(argv[1]);
    TSLICE = atoi(argv[2]);
    int queue[NCPU];
    int priorities[NCPU]; // Priority for each job in the queue
    setup_signal_handler();
    char *str, *str_for_history = (char *)malloc(100);
    if (str_for_history == NULL) {
        printf("Error allocating memory\n");
        exit(1);
    }

    char c[100];
    printf("\n\nSHELL STARTED\n\n----------------------------\n\n");

    while (1) {
        getcwd(c, sizeof(c));
        printf("Shell> %s>>> ", c);
        str = Input(); // Get user input
        if (flag_for_Input == true) {
            char **command_1 = break_spaces(str);
            int priority = MIN_PRIORITY; // Default priority
            if (command_1[2]) {
                // The user specified a priority as a command-line argument
                priority = atoi(command_1[2]);
            }

            strcpy(str_for_history, str);
            start_time = get_time();
            executeCommand(command_1, queue, priority);
            count_history = add_to_history(str_for_history, child_pid, priority, start_time, get_time(), count_history);
        }
    }

    free(str_for_history);

    return 0;
}
