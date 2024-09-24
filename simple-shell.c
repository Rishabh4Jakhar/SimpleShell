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
pid_t running_bg_process[MAX_BGPROCESS] = {0};

// struct for each command stored in history
struct ComParam {
    char command[INPUT_SIZE];
    time_t start_time;
    time_t end_time;
    double duration;
    pid_t proc_pid;
};

struct ComHist
{
    struct ComParam record[HISTORY_SIZE];
    int historyCount;
};

struct ComHist history;



// main shell loop
void shell_loop()
{
    // Setting the function for SIGINT (Ctrl + C)
    if (signal(SIGINT, my_handler) == SIG_ERR)
    {
        perror("SIGINT handling failed");
    }
    // Setting the function for SIGCHLD
    if (signal(SIGCHLD, handle_sigchld) == SIG_ERR)
    {
        perror("SIGCHLD handling failed");
    }
    int status;
    do
    {
        // Creating the prompt text
        char *user = getenv("USER");
        if (user == NULL)
        {
            perror("USER environment variable not declared");
            exit(1);
        }
        char host[INPUT_SIZE];
        int hostname = gethostname(host, sizeof(host));
        if (hostname == -1)
        {
            perror("HOST not declared");
            exit(1);
        }
        printf("%s@%s~$ ", user, host);

        // taking input
        char *command = read_user_input();
        // handling the case if the input is blank or enter key
        if (strlen(command) == 0 || strcmp(command, "\n") == 0)
        {
            status = 1;
            continue;
        }
        // removing the newline character
        command = strtok(command, "\n");
        bool isInvalidCommand = validate_command(command);
        char *tmp = strdup(command);
        if (tmp == NULL)
        {
            perror("Error in strdup");
            exit(EXIT_FAILURE);
        }
        if (isInvalidCommand)
        {
            status = 1;
            strcpy(history.record[history.historyCount].command, tmp);
            history.record[history.historyCount].start_time = time(NULL);
            history.record[history.historyCount].end_time = time(NULL);
            history.record[history.historyCount].duration = difftime(
                history.record[history.historyCount].end_time,
                history.record[history.historyCount].start_time);
            history.historyCount++;
            printf("Invalid Command : includes quotes/backslash\n");
            continue;
        }
        // checking if the input is "history"
        if (strstr(command, "history"))
        {
            if (history.historyCount > 0)
            {
                strcpy(history.record[history.historyCount].command, tmp);
                history.record[history.historyCount].start_time = time(NULL);
                displayHistory();
                history.record[history.historyCount].end_time = time(NULL);
                history.record[history.historyCount].duration = difftime(
                    history.record[history.historyCount].end_time,
                    history.record[history.historyCount].start_time);
                history.historyCount++;
            }
            else
            {
                status = 1;
                printf("No command in the history\n");
                continue;
            }
        }
        else
        {
            // checking for pipes in the process
            if (strchr(command, '|'))
            {
                strcpy(history.record[history.historyCount].command, tmp);
                history.record[history.historyCount].start_time = time(NULL);
                status = launch_pipe(command);
                history.historyCount++;
            }
            else
            {
                char **args = tokenize(command, " ");
                strcpy(history.record[history.historyCount].command, tmp);
                history.record[history.historyCount].start_time = time(NULL);
                status = launch(args);
                history.record[history.historyCount].end_time = time(NULL);
                history.record[history.historyCount].duration = difftime(
                    history.record[history.historyCount].end_time,
                    history.record[history.historyCount].start_time);
                history.historyCount++;
            }
        }
        // resetting the background process variable
        bgProcess = 0;
    } while (status);
}

// Main function
int main()
{
    // initializing count for elements in history
    history.historyCount = 0;
    shell_loop();
    return 0;
}
