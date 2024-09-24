#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// defining sizes for data structures allocated
#define INPUT_SIZE 256
#define MAX_BGPROCESS 5
#define HISTORY_SIZE 100
// flag if the background process is started
int bgProcess = 0;
// PIDs of background process running in the background. Can handle max of 5 processes only
// initialising all values to 0
pid_t run_bgproccess[MAX_BGPROCESS] = {0};

// struct for each cmd stored in history
struct ComParam {
    char cmd[INPUT_SIZE];
    time_t start_time;
    time_t end_time;
    double duration;
    pid_t proc_pid;
};

struct ComHist {
    struct ComParam record[HISTORY_SIZE];
    int histCount;
};

struct ComHist history;


// Function to display details of each cmd when the program is terminated using Ctrl + C
void disEnd() {
    printf("-----------------------\n");
    for (int i = 0; i < history.histCount; i++)  {
        struct ComParam record = history.record[i];
        // conversion of start and end time to string structures
        struct tm *start_time_inf = localtime(&record.start_time);
        char start_time_buff[80];
        strftime(start_time_buff, sizeof(start_time_buff), "%Y-%m-%d %H:%M:%S", start_time_inf);
        struct tm *end_time_info = localtime(&record.end_time);
        char end_time_buffer[80];
        strftime(end_time_buffer, sizeof(end_time_buffer), "%Y-%m-%d %H:%M:%S", end_time_info);
        printf("%s\nProcess PID: %d\n", record.cmd, record.proc_pid);
        printf("Start time: %s\nEnd Time: %s\nProcess Duration: %f\n", start_time_buff, end_time_buffer, record.duration);
        printf("---------------------------\n");
    }
}

// SIGINT (Ctrl + C) handler
static void my_handler(int signum) {
    printf("\nCtrl + C pressed\n");
    disEnd();
    exit(0);
}

// displaying cmd history
void disHist() {
    history.record[history.histCount].proc_pid = getpid();
    for (int i = 0; i < history.histCount + 1; i++){
        printf("%d  %s\n", i + 1, history.record[i].cmd);
    }
}

int pop(pid_t pid) {
    // removing the PID of the completed background process and resetting it to 0
    for (int i = 0; i < MAX_BGPROCESS; i++) {
        if (run_bgproccess[i] == pid) {
            run_bgproccess[i] = 0;
            return i;
        }
    }
    return -1;
}

// append background process as the latest value in the array
int append(pid_t pid) {
    int added;
    added = -1;
    // if no background process running then add it as the first value
    // Check if run_bgproccess is empty (filled of 0)
    int count = 0;
    for (int i = 0; i < MAX_BGPROCESS; i++) {
        if (run_bgproccess[i] == 0)
        {
            count++;
        }
    }
    if (count == MAX_BGPROCESS) {
        run_bgproccess[0] = pid;
        added = 0;
        return added;
    }
    // adding the process only after the last non-zero PID that is a running background process
    for (int i = MAX_BGPROCESS - 2; i >= 0; i--) {
        if (run_bgproccess[i] != 0)
        {
            run_bgproccess[i + 1] = pid;
            added = i + 1;
        }
    }
    return added;
}

// function to strip the leading and trailing spaces
char *strip(char *stri) {
    char stripped[strlen(stri) + 1];
    int flag;    
    int len = 0;
    if (stri[0] == ' ') {
        flag = 0;
    } else {
        flag = 1;}
    for (int i = 0; stri[i] != '\0'; i++) {
        if (stri[i] != ' ' && flag == 0){
            stripped[len++] = stri[i];
            flag = 1;
        } else if (flag == 1) {
            stripped[len++] = stri[i];
        } else if (stri[i] != ' ') {
            flag = 1;
        }
    }
    char *final_strip = (char *)malloc(INPUT_SIZE);    
    stripped[len] = '\0';
    if (final_strip == NULL) {
        perror("Error: Memory allocation failed\n");
    }
    memcpy(final_strip, stripped, INPUT_SIZE);
    return final_strip;
}

// splitting the cmd according to specified delimiter
char **tokenize(char *cmd, const char delim[2]) {
    char **args = (char **)malloc(INPUT_SIZE * sizeof(char *));
    if (args == NULL){
        perror("Error: Memory allocation failed\n");
    }
    int count = 0;
    char *token = strtok(cmd, delim);
    while (token != NULL) {
        args[count++] = strip(token);
        token = strtok(NULL, delim);
    }
    // checking for & as the last argument to check if the process should be in the background
    if (count > 0 && strcmp(args[count - 1], "&") == 0 && strcmp(delim, " ") == 0) {
        bgProcess = 1;
        free(args[count - 1]);
        args[count - 1] = NULL;
        count--;
    }
    return args;
}

// Handling Piping
int pipe_process(char **cmds, int pipes) {
    // Creating pipes according to the input
    int fd[pipes][2];
    for (int i = 0; i < pipes; i++){
        if (pipe(fd[i]) == -1){
            perror("Error: Piping failed\n");
        }
    }
    int pid;
    // Using the read and write descriptors of the pipe for each cmd to read from STDIN and write to STDOUT
    for (int i = 0; i < pipes + 1; i++) {
        char **args = tokenize(cmds[i], " ");
        pid = fork();
        if (pid < 0) {
            perror("Error: Fork failed\n");
        } else if (pid == 0) {
            // First cmd cannot read from the pipe
            if (i > 0) {
                // Closing all the descriptors which are not required
                for (int j = 0; j < pipes; j++) {
                    if (j != i - 1) {
                        close(fd[j][0]);
                    }
                    if (j != i) {
                        close(fd[j][1]);
                    }                    
                }
                if (dup2(fd[i - 1][0], STDIN_FILENO) == -1) {
                    printf("Pipe %d: Reading failed\n", i - 1);
                    exit(1);
                }
                // Closing the used read descriptor
                close(fd[i - 1][0]);
            }
            // Last cmd cannot write into the pipe. It can only write to STDOUT
            if (i < pipes){
                if (dup2(fd[i][1], STDOUT_FILENO) == -1){
                    printf("Pipe %d: Writing failed\n", i);
                    exit(1);
                }
                // Closing the used write descriptor
                close(fd[i][1]);
            }
            int check = execvp(args[0], args);
            if (check == -1){
                printf("%s: cmd not found\n", args[0]);
                exit(1);
            }
        } else {
            // Closing the duplicate file descriptors created due to forking
            if (i < pipes) {
                close(fd[i][1]);
            }            
            if (i > 0){
                close(fd[i - 1][0]);
            } 
        }
    }
    // waiting for all the child process to terminate
    for (int i = 0; i < pipes + 1; i++) {
        wait(NULL);
    }
    // returning pid of the latest child process
    return pid;
}

// launch process for pipe cmds
int launch_pipe(char *cmd) {
    int status;
    int count = 0;
    // counting number of pipes
    for (int i = 0; cmd[i] != '\0'; i++) {
        if (cmd[i] == '|'){
            count++;}
    }
    // splitting all user cmds according to the pipe
    char **cmds = tokenize(cmd, "|");
    status = pipe_process(cmds, count);
    if (status < 0) {
        history.record[history.histCount].proc_pid = 0;
    }
    else {        
        history.record[history.histCount].proc_pid = status;

    }
    // setting the time after the pipe process terminates
    history.record[history.histCount].end_time = time(NULL);
    history.record[history.histCount].duration = difftime(history.record[history.histCount].end_time, history.record[history.histCount].start_time);
    // freeing allocated memory space
    free(cmds);
    return status;
}

// Handling SIGCHLD: Passed when a child process terminates (background process in this case)
void handle_sigchld(int signum) {
    int status;
    pid_t pid;
    // returns the pid of the terminated child
    // WNOHANG: returns 0 if the status information of any process is not available i.e. the process has not terminated
    // In other words, the loop will only run after the background processes running has terminated or changed state
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Finding the child process in history according to the PID of the terminated process
        for (int i = 0; i < history.histCount; i++) {
            if (history.record[i].proc_pid == pid) {
                // finding from background process array
                int order = pop(pid);
                if (order == -1) {
                    // background process not found i.e. it was not added to the array
                    history.record[i].end_time = history.record[i].start_time;
                    history.record[i].duration = difftime(history.record[i].end_time,history.record[i].start_time);
                } else {
                    history.record[i].end_time = time(NULL);
                    history.record[i].duration = difftime(history.record[i].end_time,history.record[i].start_time);
                    // duplicating the cmd to tmp to avoid corruption of data
                    char *tmp = strdup(history.record[i].cmd);
                    if (tmp == NULL) {
                        perror("Error: strdup failed\n");
                        exit(EXIT_FAILURE);
                    }
                    tmp = strtok(tmp, "&");
                    printf("\n[%d]+ Done                    %s\n", order + 1, tmp);
                    break;

                }
            }
        }
    }
}

// Checking for quotation marks and backslash in the input
bool validate_cmd(char *cmd) {
    if (strchr(cmd, '\'') || strchr(cmd, '\"') || strchr(cmd, '\\')) {
        return true;
    }
    return false;
}
// running shell cmds
int create_process_and_run(char **args) {
    int status = fork();
    if (status < 0)
    {
        perror("Fork Failed");
    }
    else if (status == 0)
    {
        int check = execvp(args[0], args);
        if (check == -1) {
            printf("%s: cmd not found\n", args[0]);
            exit(1);
        }
    }
    else {
        // Checking for background process
        if (!(bgProcess)) {
            int child_status;
            // Wait for the child to complete
            wait(&child_status);
            if (WIFEXITED(child_status)) {
                int exit_code = WEXITSTATUS(child_status);
            } else{
                printf("Child process did not exit normally.\n");
            }
        } else {
            // parent doesn't wait for the background child processes to terminate
            int order = append(status);
            if (order == -1) {
                history.record[history.histCount].proc_pid = 0;
                printf("No more background processes can be added");
            } else {
                history.record[history.histCount].proc_pid = status;
                // start of background process
                printf("[%d] %d\n", order + 1, status);
            }
        }
    }
    return status;
}

// launch function
int launch(char **args) {
    int status;
    status = create_process_and_run(args);
    if (status < 0) {
        history.record[history.histCount].proc_pid = 0;
    } else {
        history.record[history.histCount].proc_pid = status;
    }
    return status;
}

// taking input from the terminal
char *read_user_input() {
    char *input = (char *)malloc(INPUT_SIZE);
    if (input == NULL) {
        perror("Error: Memory allocation failed\n");
        free(input);
        exit(EXIT_FAILURE);
    }
    size_t size = 0;
    int read = getline(&input, &size, stdin);
    if (read != -1) {
        return input;
    } else {
        perror("Error: Some error occured while reading lines\n");
        free(input);
    }
}

// main shell loop
void shell_loop(){
    // Setting the function for SIGINT (Ctrl + C)
    if (signal(SIGINT, my_handler) == SIG_ERR){
        perror("Error: SIGINT handling failed");
    }
    // Setting the function for SIGCHLD
    if (signal(SIGCHLD, handle_sigchld) == SIG_ERR) {
        perror("Error: SIGCHLD handling failed");
    }
    int status;
    do {
        // Creating the prompt text
        char *user = getenv("USER");
        if (user == NULL) {
            perror("Error: USER environment variable not declared");
            exit(1);
        }
        char host[INPUT_SIZE];
        int hostname = gethostname(host, sizeof(host));
        if (hostname == -1) {
            perror("Error: HOST not declared");
            exit(1);
        }
        printf("%s@%s~$ ", user, host);
        // taking input
        char *cmd = read_user_input();
        // handling the case if the input is blank or enter key
        if (strlen(cmd) == 0 || strcmp(cmd, "\n") == 0) {
            status = 1;
            continue;
        }
        // removing the newline character
        cmd = strtok(cmd, "\n");
        bool isInvalidcmd = validate_cmd(cmd);
        char *tmp = strdup(cmd);
        if (tmp == NULL) {
            perror("Error in strdup");
            exit(EXIT_FAILURE);
        }
        if (isInvalidcmd) {
            status = 1;
            strcpy(history.record[history.histCount].cmd, tmp);
            history.record[history.histCount].start_time = time(NULL);
            history.record[history.histCount].end_time = time(NULL);
            history.record[history.histCount].duration = difftime(history.record[history.histCount].end_time,history.record[history.histCount].start_time);
            history.histCount++;
            printf("Invalid cmd : Includes quotes/backslash  (These characters are not allowed)\n");
            continue;
        }
        // checking if the input is "history"
        if (strstr(cmd, "history")) {
            if (history.histCount > 0) {
                strcpy(history.record[history.histCount].cmd, tmp);
                history.record[history.histCount].start_time = time(NULL);
                disHist();
                history.record[history.histCount].end_time = time(NULL);
                history.record[history.histCount].duration = difftime(history.record[history.histCount].end_time,history.record[history.histCount].start_time);
                history.histCount++;
            } else {
                status = 1;
                printf("No cmd in the history\n");
                continue;
            }
        } else {
            // checking for pipes in the process
            if (strchr(cmd, '|')) {
                strcpy(history.record[history.histCount].cmd, tmp);
                history.record[history.histCount].start_time = time(NULL);
                status = launch_pipe(cmd);
                history.histCount++;
            } else {
                char **args = tokenize(cmd, " ");
                strcpy(history.record[history.histCount].cmd, tmp);
                history.record[history.histCount].start_time = time(NULL);
                status = launch(args);
                history.record[history.histCount].end_time = time(NULL);
                history.record[history.histCount].duration = difftime(
                    history.record[history.histCount].end_time,
                    history.record[history.histCount].start_time);
                history.histCount++;
            }
        }
        // resetting the background process variable
        bgProcess = 0;
    } while (status);
}

// Main function
int main() {
    // initializing count for elements in history
    history.histCount = 0;
    shell_loop();
    return 0;
}
