#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h> // file descriptors

#include "shell.h"

char* trim(char* s);
int makeargv(char *s, char *delimiters, char ***argvp);

void shell() {
	while (1) {
		// Retrieve PID of the parent process. 
		pid_t shell_pid = getpid();

		// Print shell prompt. 
		printf("{%i}$ ", shell_pid);

		// Retrieve input from stdin.
		char* input = NULL;
		size_t input_size = 0;
		getline(&input, &input_size, stdin);
		char* index = strchr(input, (int)'\n');
		*index = '\0';

		// Handle builtins
		char** builtin;
		makeargv(input, " ", &builtin);
		if (builtin[0] != NULL) {
			if (strcmp(builtin[0], "exit") == 0)
				exit(0);
			else if (strcmp(builtin[0], "cd") == 0) {
				chdir(builtin[1]);
				continue;
			}
		}
		free(builtin);

		// Create a child process to handle user input.
		pid_t pid = fork();

		if (pid != 0) { // Parent
			wait(NULL);
		} 
		else { // Child process
			char* command = NULL;
			int in, out;
			char kind;
			int i = strlen(input);
			for (; i > 0; i--) {
				if (input[i-1] == '|' || input[i-1] == '>' || input[i-1] == '<') {
					command = input + i;
					kind = input[i-1];
					input[i-1] = '\0';

					if (kind == '|') {
						int pipe2[2]; // 0 - read, 1 - write
						pipe(pipe2);
						pid_t newpid = fork();

						if (newpid == 0) {
							// This is the child, it's going leftwards
							// and it's stdout should be to it's parents
							// stdin.
							dup2(pipe2[1], STDOUT_FILENO);
							continue;
						}
						else { // Parent, right side thing
							dup2(pipe2[0], STDIN_FILENO);
							close(pipe2[1]); // The magical fix for continuous STDIN

							char** argv;
							makeargv(command, " ", &argv);
							execute(argv);
						}
					}
					else if (kind == '<') {
						char* trimmed = trim(command);
						in = open(trimmed, O_RDONLY);
						free(trimmed);
						dup2(in, 0);
					}
					else if (kind == '>') {
						char* trimmed = trim(command);
						out = open(trimmed, O_RDWR|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR|S_IWOTH|S_IROTH|S_IRGRP|S_IWGRP);
						free(trimmed);
						dup2(out, 1);
					}
				}
			}
			// We got here after no piping
			char** argv = NULL;
			makeargv(input, " \n", &argv);
			execute(argv); // Child should kill itself
		}

		// Free memory allocated by getline(). 
		free(input);
	}
}

int execute(char* args[]) {
	int err = execvp(args[0], args);
	if (err < 0) {
		printf("Error running %s, errno:%d\n", args[0], errno);
		return 0;
		//printf("Error running %s, errno:%d:%s\n", args[0], errno, (char*)strerror(errno));
	}

	return 1;
}

char* trim(char* s) {
	char* news = malloc(strlen(s)+1);
	int i, j = 0;
	for (i = 0; i < strlen(s); i++) {
		if (!isspace(s[i])) {
			news[j] = s[i];
			j++;
		}
	}

	news[j] = '\0';
	return news;
}

/*
 * Make argv array (*arvp) for tokens in s which are separated by
 * delimiters.   Return -1 on error or the number of tokens otherwise.
 */
int makeargv(char *s, char *delimiters, char ***argvp)
{
   char *t;
   char *snew;
   int numtokens;
   int i;
    /* snew is real start of string after skipping leading delimiters */
   snew = s + strspn(s, delimiters);
                              /* create space for a copy of snew in t */
   if ((t = calloc(strlen(snew) + 1, sizeof(char))) == NULL) {
      *argvp = NULL;
      numtokens = -1;
   } else {                     /* count the number of tokens in snew */
      strcpy(t, snew);
      if (strtok(t, delimiters) == NULL)
         numtokens = 0;
      else
         for (numtokens = 1; strtok(NULL, delimiters) != NULL;
              numtokens++)
              ;  
                /* create an argument array to contain ptrs to tokens */
      if ((*argvp = calloc(numtokens + 1, sizeof(char *))) == NULL) {
         free(t);
         numtokens = -1;
      } else {            /* insert pointers to tokens into the array */
         if (numtokens > 0) {
            strcpy(t, snew);
            **argvp = strtok(t, delimiters);
            for (i = 1; i < numtokens + 1; i++)
               *((*argvp) + i) = strtok(NULL, delimiters);
         } else {
           **argvp = NULL;
           free(t);
         }
      }
   }   
   return numtokens;
}
