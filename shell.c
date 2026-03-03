#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

#define BUFFER_SIZE 1024
#define PROMPT_SIZE 4096
#define MAX_ARGS 64
#define MAX_HIST 50

char history_path[BUFFER_SIZE];



void print_prompt(char *prompt) {
    char hostname[BUFFER_SIZE], cwd[BUFFER_SIZE], username[BUFFER_SIZE];
    gethostname(hostname, sizeof(hostname)); // gets the hostname from the user. 
    getcwd(cwd, sizeof(cwd)); // gets the current working directory.
    strcpy(username, getenv("USER")); // grabs the username of the  user.
    if (username == NULL) {
        perror("getenv");
        strcpy(username, "default"); // in case username cannot be found use "default" as default./
    }
    // safely builds the prompt string (e.g., "\nLALO@nicho:/home/dir> ") into the 'prompt' variable.
    // it prevents buffer overflows by strictly limiting the size to BUFFER_SIZE.
    snprintf(prompt, PROMPT_SIZE, "%s@%s:%s> ", username, hostname, cwd); 
}


char *get_command(void) {

    char prompt[BUFFER_SIZE]; // buffer to  hold shell prompt text.
     

    print_prompt(prompt); // gets hostname, username and path from user.
    
    char *command = readline(prompt); // prints the prompt, waits for user input, handles arrow keys/backspace. dynamic allocates memory for final string.


    if (command == NULL) {  // user entered ctrla+D
        return NULL;
    }

    // Add to readline's internal memory so the up-arrow has previous commands to display
    if (strlen(command) > 0) {
        add_history(command); // adds the typed command to readline's internal history list. this makes the command accessible when the user presses up arrow.
    }
    
    return command;
}

char **parse(char *command) { // parses user's command.
    int i = 0;
    char **args = malloc(MAX_ARGS * sizeof(char *)); // allocate memory 
    if (!args) {
        printf("Error: Memory allocation failed.\n");
        exit(1);
    }
    char *token = strtok(command, " \t\n"); // tokenize the string, command, on spaces, tabs and newlines.
    while(token != NULL) {
        if(i >= MAX_ARGS - 1){ // check if there are too many arguments in the command.
            printf("Too many arguments.\n");
            break;
        }
        args[i++] = token; // store each token in the arguments array.
        token = strtok(NULL, " \t\n"); 
    }
    args[i] = NULL;
    return args; // return the arguments
}

// Handles Output Redirection: command > file
void redir_out(char *args[], int index) {
    char *filename = args[index + 1];
    if (filename == NULL) {
        fprintf(stderr, "Redirection error: no output file specified\n");
        exit(1);
    }

    // Open file: Write-Only, Create if missing, Truncate (empty) if exists
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open output file");
        exit(1);
    }

    dup2(fd, STDOUT_FILENO); // Point STDOUT to our file
    close(fd);               // Close the extra descriptor

    args[index] = NULL;      // Remove ">" so execvp doesn't see it
}

// Handles Input Redirection: command < file
void redir_in(char *args[], int index) {
    char *filename = args[index + 1];
    if (filename == NULL) {
        fprintf(stderr, "Redirection error: no input file specified\n");
        exit(1);
    }

    // Open file: Read-Only
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open input file");
        exit(1);
    }

    dup2(fd, STDIN_FILENO); // Point STDIN to our file
    close(fd);              // Close the extra descriptor

    args[index] = NULL;     // Remove "<" so execvp doesn't see it
}

void redir_append(char *args[], int index) {
    char *filename = args[index + 1];
    if(filename == NULL) {
        perror("redirection error. No output file.\n");
        exit(1);
    }

    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("open append file.");
        exit(1);

    }

    dup2(fd, STDOUT_FILENO);
    close(fd);

    args[index] = NULL;
}

void prepare_and_run(char *args[]) {
    
    for(int i = 0; args[i] != NULL; i++) {

        if (strcmp(args[i], ">") == 0) {
            redir_out(args, i);
            break;
        }
        else if (strcmp(args[i], "<") == 0) {
            redir_in(args, i);
            break;
        }
        else if(strcmp(args[i], ">>") == 0) {
            redir_append(args, i);
            break;
        }
    }

    execvp(args[0], args); // execute the command given by user.
    perror("execvp"); // if execvp returns, then there was an error in child process. iF it reaches here = error.
    exit(1);
}
void execute_commands(char *args[]) {

    pid_t pid = fork();

    if(pid == -1) { // check for forking err.
        printf("Fork error.\n");
        exit(1);
    }

    if(pid == 0) { // child process.
        prepare_and_run(args);
    }
    else { // parent process waits for the child process to finish.
        int status; 
        if(waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
        }
    }

}

void execute_piped_commands(char ***commands, int num_cmds) {
    
    int pipefd[2]; // holdds read [0] and write [1] fds for current pipe.
    int prev_fd = -1; // holds read end of prev pipe.
    pid_t pid; // holds pid from fork.

    for (int i = 0; i < num_cmds; i++) { // loop through each command in the pipeline
        if (i < num_cmds - 1) { // if it's not the last command, we need a pipe to send data to the next command.
            if (pipe(pipefd) < 0) {
                perror("pipe");
                return;
            }

        }
    

        pid = fork(); // create child process.

        if (pid < 0) {
            perror("fork");
            return;
        }

        if (pid == 0) { // child process
            if (prev_fd != -1) { // check if there is a previous pipe (not the first one.)
                dup2(prev_fd, STDIN_FILENO); // redirects standard in to read from previous pipe,.
                close(prev_fd); // close previous pipe fd since it;s now duped to STDIN.
            }

            if (i < num_cmds - 1) { // if there are still more commands to come
                dup2(pipefd[1], STDOUT_FILENO); // redirect stdout to write to curr pipe.
                close(pipefd[0]); // close read end of current pipe.
                close(pipefd[1]); // close write end.
            }
        
            prepare_and_run(commands[i]);
        }

        if(prev_fd != -1) {
            close(prev_fd);
        }

        if (i < num_cmds - 1) {
            prev_fd = pipefd[0];
            close(pipefd[1]);
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        wait(NULL); // loop to wait for all child process in pipeline to finish executing.
    }
}

int parse_pipes(char **args, char ***commands) {
    int num_cmds = 0;
    commands[num_cmds ++] = args;

    for (int i =0 ; args[i] != NULL; i++) {
        if(strcmp(args[i], "|") == 0) {
            args[i] = NULL;
            commands[num_cmds++] = &args[i + 1];
        }
    } 
    return num_cmds;
}

void setup_history() {
    char *home = getenv("HOME"); // get home path. 
    //printf("HOME: %s", home);
    if (home != NULL) {
        snprintf(history_path, sizeof(history_path), "%s/.shell_history.txt", home);
    }
    else {
        strcpy(history_path, ".shell_history.txt");
    }
    read_history(history_path);
}

void run_shell(void) {
    
    setup_history();
    printf("This program lets you enter any unix command. Enter command or press 'q' to quit.\n");
    
    while (1) {
       
        char *command = get_command(); // get the command from the user
        if (command == NULL) {
            printf("\nExiting shell.\n");
            write_history(history_path);
            break;
        }
        if(strlen(command) == 0) {
            free(command);
            continue;
        }
        
        char **args = parse(command); // parse into arguments for the execvp command.
        if(args[0] == NULL) {
            free(command);
            free(args);
            continue;
        }
        
        

        if (strcmp(args[0], "q") == 0 || strcmp(args[0], "Q") == 0) { // built in quit/exit.
            printf("\nGoodbye.\n");
            free(command);
            free(args);

            write_history(history_path); // save history of commands in memory from readline into disk. /home/user/.shell_history 
           
            break; 
        }
        
        if (strcmp(args[0], "cd") == 0) { // handles cd
            if (args[1] == NULL) {
                printf("cd: missing argument\n");
            } else if (chdir(args[1]) != 0) {
                perror("cd"); 
            }
            free(command);
            free(args);
            continue;
        }

        char **commands[MAX_ARGS]; 
        int num_cmds = parse_pipes(args, commands);

        if (num_cmds > 1) {
            // if pipes were found, run pipeline
            execute_piped_commands(commands, num_cmds);
        } else {
            execute_commands(args); 
        }
        
        //free memory for command and arguments.
        free(command); 
        free(args);
    }
    
}

int main(int argc, char *argv[]) {

    run_shell();
    
    return 0;
}