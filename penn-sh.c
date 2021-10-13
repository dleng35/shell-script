#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "tokenizer.h"

#define INPUT_SIZE 1024

pid_t childPid = 0;

void executeShell();

void writeToStdout(char *text);

void sigintHandler(int sig);

void redirectionsSTDOUTtoFile (char* tokenAfter);

void redirectionsSTDINtoFile (char* tokenAfter);

char** createArrayOfTokensBeforePipe (char** commandArray);

char** createArrayOfTokensAfterPipe (char** commandArray);

char **getCommandFromInput();

void registerSignalHandlers();

void killChildProcess();

int main(int argc, char **argv) {
    registerSignalHandlers();

    while (1) {
        executeShell();
        signal(SIGINT, SIG_IGN);
    }
    return 0;
}

/* Sends SIGKILL signal to a child process.
 * Error checks for kill system call failure and exits program if
 * there is an error */
void killChildProcess() {
    if (kill(childPid, SIGKILL) == -1) {
        perror("Error in kill");
        exit(EXIT_FAILURE);
    }
}

/* Signal handler for SIGINT. Catches SIGINT signal (e.g. Ctrl + C) and
 * kills the child process if it exists and is executing. Does not
 * do anything to the parent process and its execution */
void sigintHandler(int sig) {
    if (childPid != 0) {
        killChildProcess();
    }
}

void redirectionsSTDOUTtoFile (char* tokenAfter) {
    int new_stdout = open(tokenAfter, O_WRONLY | O_TRUNC| O_CREAT, 0644);
    if (new_stdout == -1) {
        printf("Error in redirection STDOUT");
        exit(EXIT_FAILURE);
    }
    int fd_dup = dup2(new_stdout, STDOUT_FILENO);
    if (fd_dup == -1) {
        printf("Error, failed to redirect STDOUT");
    }
    close(new_stdout);
}

void redirectionsSTDINtoFile (char* tokenAfter) {
    int new_stdin = open(tokenAfter, O_RDONLY, 0);
    if (new_stdin == -1) {
        printf("Error in redirection STDIN");
        exit(EXIT_FAILURE);
    }
    int fd_dup = dup2(new_stdin, STDIN_FILENO);
    if (fd_dup == -1) {
        printf("Error, failed to redirect STDIN");
    }
    close(new_stdin);
}

char** createArrayOfTokensBeforePipe (char** commandArray) {
    int i = 0;
    char** tokensBeforePipe = (char**) malloc(50 * sizeof(char*));
    if (tokensBeforePipe == NULL) {
        perror("Malloc for tokensBeforePipe failed");
        exit(EXIT_FAILURE);
    }
    while(commandArray[i] != NULL) {
        if (strcmp(commandArray[i], "|") == 0) {
            break;
        }
        tokensBeforePipe[i] = commandArray[i];
        i++;
    }
    tokensBeforePipe[i] = NULL;
    return tokensBeforePipe;
}

char** createArrayOfTokensAfterPipe (char** commandArray) {
    int i = 0;
    int j = 0;
    char** tokensAfterPipe = (char**) malloc(50 * sizeof(char*));
    if (tokensAfterPipe == NULL) {
        perror("Malloc for tokensAfterPipe failed");
        exit(EXIT_FAILURE);
    }
    while(commandArray[i] != NULL) {
        if (strcmp(commandArray[i], "|") == 0) {
            i++;
            while(commandArray[i] != NULL) {
                tokensAfterPipe[j] = commandArray[i];
                i++;
                j++;
            }
            break;
        }
        i++;
    }
    tokensAfterPipe[j] = NULL;
    return tokensAfterPipe;
}

/* Registers SIGALRM and SIGINT handlers with corresponding functions.
 * Error checks for signal system call failure and exits program if
 * there is an error */
void registerSignalHandlers() {
    if (signal(SIGINT, sigintHandler) == SIG_ERR) {
        perror("Error in signal");
        exit(EXIT_FAILURE);
    }
}

/* Prints the shell prompt and waits for input from user.
 * Takes timeout as an argument and starts an alarm of that timeout period
 * if there is a valid command. It then creates a child process which
 * executes the command with its arguments.
 *
 * The parent process waits for the child. On unsuccessful completion,
 * it exits the shell. */
void executeShell() {
    char **command_array;
    int status;
    char minishell[] = "penn-sh> ";
    static int redirection_STDOUT_count = 0; //checking for multiple same redirections
    static int redirection_STDIN_count = 0; //checking for multiple same redirections
    char str1[] = ">";
    char str2[] = "<";
    char str3[] = "|";

    writeToStdout(minishell);

    command_array = getCommandFromInput();
    
    if (command_array == NULL){
        perror("Invalid input command");
        exit(EXIT_FAILURE);
    }
    if (*command_array == NULL) {
        return;
    }
    if (command_array == "") {
        return;
    }
    
    //pipes project2b
    int k = 0;
    while (command_array[k] != NULL) {
        if (strcmp(command_array[k], str3) == 0) {
            int fd[2];
            char** commandArray1 = createArrayOfTokensBeforePipe(command_array);
            char** commandArray2 = createArrayOfTokensAfterPipe(command_array);
            if (pipe(fd) == -1) {
                perror("pipe failed");
                exit(EXIT_FAILURE);
            }
            
            pid_t childPid1 = fork();
            if (childPid1 == -1) {
                perror("fork failed");
                exit(EXIT_FAILURE);
            }
            if (childPid1 == 0) {
                int l = 0;
                int m = 0;
                char* args1[100] = {0}; 
                while(commandArray1[l] != NULL) {
                    if (strcmp(commandArray1[l], str1) == 0) {
                        perror("Invalid");
                        exit(EXIT_FAILURE);
                    }else if (strcmp(commandArray1[l], str2) == 0) {
                        l++;
                        redirectionsSTDINtoFile(commandArray1[l]);
                    }else{
                        args1[m] = commandArray1[l];
                        m++;
                    }
                    l++;
                }
                args1[m] = NULL;
                close(fd[0]); //close unused read end
                dup2(fd[1], STDOUT_FILENO);
                if (execvp(args1[0], args1) == -1) {
                    perror("error in execvp");
                    exit(EXIT_FAILURE);
                }
                close(fd[1]);
            }

            pid_t childPid2 = fork();
            if (childPid2 == -1) {
                perror("fork failed");
                exit(EXIT_FAILURE);
            }
            if (childPid2 == 0) {
                int n = 0;
                int p = 0;
                char* args2[100] = {0};
                while(commandArray2[n] != NULL) {
                    if (strcmp(commandArray2[n], str1) == 0) {
                        n++;
                        redirectionsSTDOUTtoFile(commandArray2[n]);
                    }else if (strcmp(commandArray2[n], str2) == 0) {
                        perror("Invalid");
                        exit(EXIT_FAILURE);
                    }else{
                        args2[p] = commandArray2[n];
                        p++;
                    }
                    n++;
                }
                args2[p] = NULL;
                close(fd[1]); //close unused write end
                dup2(fd[0], STDIN_FILENO);
                if (execvp(args2[0], args2) == -1) {
                    perror("error in execvp");
                    exit(EXIT_FAILURE);
                }
                close(fd[0]);
            }
            close(fd[0]); //closing both read and write of the parents
            close(fd[1]);
            do {
                if (waitpid(childPid1, &status, 0) == -1) {
                    perror("Error in child process termination");
                    exit(EXIT_FAILURE);
                }
                if (waitpid(childPid2, &status, 0) == -1) {
                    perror("Error in child process termination");
                    exit(EXIT_FAILURE);
                }
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
            childPid1 = 0;
            childPid2 = 0;
            free(commandArray1);
            free(commandArray2);
            free(command_array);
            return; //doesn't go one to part 2a
        }
        k++;
    }

    //project 2a
    if (command_array[0] != NULL) {
        childPid = fork();

        if (childPid < 0) {
            perror("Error in creating child process");
            exit(EXIT_FAILURE);
        }

        if (childPid == 0) {
            int i = 0;
            int j = 0;
            char* args[100] = {0};
            while(command_array[j] != NULL) {
                if (strcmp(command_array[j], str1) == 0) {
                    redirection_STDOUT_count++;
                    if (redirection_STDOUT_count > 1) {
                        perror("Invalid: Multiple standard output redirects");
                    }  
                    j++;
                    redirectionsSTDOUTtoFile(command_array[j]);
                }else if (strcmp(command_array[j], str2) == 0) {
                    redirection_STDIN_count++;
                    if (redirection_STDIN_count > 1) {
                        perror("Invalid: Multiple standard input redirects");
                    }
                    j++;
                    redirectionsSTDINtoFile(command_array[j]);
                }else{
                    args[i] = command_array[j];
                    i++;
                }
                j++;
            }
            args[i] = NULL;

            char* bin_file = args[0];
            if (execvp(bin_file, args) == -1) {
                perror("Error in execvp");
                exit(EXIT_FAILURE);
            }
        } else {
            do {
                if (wait(&status) == -1) {
                    perror("Error in child process termination");
                    exit(EXIT_FAILURE);
                }
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
            childPid = 0;
        }
    }
    free(command_array);
}

/* Writes particular text to standard output */
void writeToStdout(char *text) {
    if (write(STDOUT_FILENO, text, strlen(text)) == -1) {
        perror("Error in write");
        exit(EXIT_FAILURE);
    }
}

/* Reads input from standard input till it reaches a new line character.
 * Checks if EOF (Ctrl + D) is being read and exits penn-shredder if that is the case
 * Otherwise, it checks for a valid input and adds the characters to an input buffer.
 *
 * From this input buffer, the first 1023 characters (if more than 1023) or the whole
 * buffer are assigned to command and returned. An \0 is appended to the command so
 * that it is null terminated */
char **getCommandFromInput() {
    TOKENIZER *tokenizer;
    char buffer[INPUT_SIZE] = {'\0'};
    char* tok;
    int i = 0;
    char** str_array_ptr = (char**) malloc(50 * sizeof(char*));

    int byte_count = read(STDIN_FILENO, buffer, INPUT_SIZE);
    if (byte_count < 0) {
        perror("read returned a negative value\n");
        exit(EXIT_FAILURE);
    }else if (byte_count == 0) {
        // catch cntl-D
        perror("user entered EOF!\n");
        exit(2);
    }else if(buffer[0] == '\0' || buffer[0] == '\n') {
        return "";
    }else{
        buffer[byte_count-1] = '\0'; //replace \n with \0
        tokenizer = init_tokenizer(buffer);
        while((tok = get_next_token(tokenizer)) != NULL ) {
            char* token_ptr = (char*) malloc(sizeof(char) * (strlen(tok)+1));
            strcpy(token_ptr, tok);
            str_array_ptr[i] = token_ptr;
            i++;
        }
        str_array_ptr[i++] = NULL;
        free_tokenizer(tokenizer);
    }
    return str_array_ptr;
}
    