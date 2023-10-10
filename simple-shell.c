#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include <stdbool.h>
#include <signal.h> 
#include <sys/time.h>
#include <time.h>

long get_time(){
    struct timeval time, *address_time = &time;
    if (gettimeofday(address_time, NULL) != 0) {
        printf("Error in getting the time.\n");
        exit(1);
    }
    long epoch_time = time.tv_sec * 1000;
    return epoch_time + time.tv_usec / 1000;
}

char history[100][100];
int pid_history[100],  child_pid;
long time_history[100][2],start_time;
bool flag_for_Input = true;
int count_history = 0, queue_head= 0 , queue_tail = 0, NCPU , TSLICE ;
pid_t queue[100];

int add_to_history(char *command, int pid, long start_time_ms, long end_time_ms, int count_history) {
    strcpy(history[count_history], command);
    pid_history[count_history] = pid;
    time_history[count_history][0] = start_time_ms;
    time_history[count_history][1] = end_time_ms;
    return ++count_history;
}

void display_history() {
    printf("-------------------------------\n");
    printf("\n Command History: \n");
    printf("-------------------------------\n");

    for (int i = 0; i < count_history; i++) {
        printf("Command: %s\n", history[i]);
        printf("PID: %d\n", pid_history[i]);
        printf("Start_Time: %ld\n", time_history[i][0]);
        printf("End_Time: %ld\n", time_history[i][1]);
        printf("-------------------------------\n");
    }
      

        
}

void signal_handler(int signum) { // check
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

bool newline_checker( char* line  , int len){
    bool flag1  = false , flag2 = false;
    if ( line[len - 1] == '\n' )
    {
        flag1 = true;
    }
    if (line[len - 1] == '\r')
    {
        flag2 = true;
    }
    return flag1 || flag2;
}

void executeCommand(char** argv) {  // check
    int pid = fork();
    child_pid = pid;

    if (pid < 0) {
        printf("Forking child failed.\n");
        exit(1);
    }

    else if (pid == 0) { //child process

        execvp(argv[0], argv); 
        printf("Command failed.\n");
        exit(1);
    }

    else { 
        
        int ret;
        int pid = wait(&ret);

        if (WIFEXITED(ret)) {
            if (WEXITSTATUS(ret) == -1)
            {
                printf("Exit = -1\n");
            }
        } else {
            printf("\nAbnormal termination with pid :%d\n" , pid);
        }
        
        return;
    }
}


char** break_spaces(char *str) {  
    char **command;
    char *sep = " \n";
    command = (char**)malloc(sizeof(char*) * 100);
    int len = 0;
    if (command == NULL) {
        printf("Memory allocation failed\n");
        exit(1); 
    }

    int i = 0;
    char *token = strtok(str,sep ); 
    while (token != NULL) {
        len = strlen(token);
        command[i] = (char*)malloc( len + 1);
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

char* Input(){   // to take input from user , returns the string entered
    char *input_str = (char*)malloc(100);
    if (input_str == NULL) {
        printf("Memory allocation failed\n");
        exit(1); 
    }
    flag_for_Input = false;
    fgets(input_str ,100, stdin);
    
    if (strlen(input_str) != 0 && input_str[0] != '\n' && input_str[0] != ' ')
    {   
        flag_for_Input = true;
    }
    return input_str;
}

void queue_command(char** argv) {
    pid_t pid = fork();

    if (pid < 0) {
        printf("Forking child failed.\n");
        exit(1);
    } else if (pid == 0) {
        wait(NULL);

        execvp(argv[0], argv);
        printf("Command failed.\n");
        exit(1);
    } else { // parent process
        queue[queue_tail++] = pid;
        kill(pid, SIGSTOP);
        printf("Child Paused___queue_tail = %d\n", queue_tail);
        return;
    }
}


int main(int argc, char const *argv[]) {
    setup_signal_handler(); 
    char *str, *str_for_history = (char *)malloc(100);
    if (str_for_history == NULL) {
        printf("Error allocating memory\n");
        exit(1);
    }

    char c[100]; // to print the current directory
    printf("\n\nSHELL STARTED\n\n----------------------------\n\n");

    while (1) {
        getcwd(c, sizeof(c));
        printf("Shell> %s>>> ", c);
        str = Input(); // Get user input
        if (flag_for_Input == true) {
            strcpy(str_for_history, str);
            start_time = get_time();
            char **command_1 = break_spaces(str);
            if (strcmp("submit" , command_1[0]) == 0)
            {   
                int arrSize = sizeof(command_1) / sizeof(command_1[0]);

                // Create a new array to store the updated contents
                char** newArr;

                // Copy elements from the original array to the new array, skipping the first element
                for (int i = 1; i < arrSize; i++) {
                    strcpy(newArr[i - 1], command_1[i]);
                }
                queue_command(newArr);
            }
            else
            {
                executeCommand(command_1);
            }         
            count_history =  add_to_history(str_for_history, child_pid, start_time, get_time() , count_history);
        }
    }
    free(str_for_history);
    return 0;
}