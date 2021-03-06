/*
    Copyright (c) 2016, Victor Municelli Dario <vmunidario@usp.br>

    This file is part of GhostShell.

    GhostShell is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <debug.h>
#include <fcntl.h>
#include "runcmd.h"
#include "run_commands.h"
#include "job.h"

extern char pwd[MAX_FILENAME];
extern list_t *jobs;

int run_commands_from_string(const char *str)
{
	/*printf("%s\n", str);*/
	return 1;
}

int run_commands_from_file(const char *path)
{
	int fd, go_on, aux, i, j, error, result;
	int *pfd = NULL;
	buffer_t *command_line;
	pipeline_t *pipeline;
	char cmd[RCMD_MAXARGS];

	fd = open(path, O_RDONLY);
	if(fd < 0)
	{
		printf("Error: %s\n", strerror(errno));
		return -1;
	}

	command_line = new_command_line();
	pipeline = new_pipeline();
	error = 0;
	go_on = 1;
	while(go_on)
	{
		aux = read_command_line_from_file(fd, command_line, &error);
		if(error) {
			printf("Error: %s\n", strerror(errno));
			break;
		}
		if(aux < 0) {
			/* EOF */
			go_on = 0;
		}

		/* skip comments */
		if(command_line->buffer[0] == '#') {
			continue;
		}

		if (!parse_command_line(command_line, pipeline))
		{
			if(pipeline->ncommands > 0)
			{
				if (!strcmp(pipeline->command[0][0], "exit") || !strcmp(pipeline->command[0][0], "quit"))
				{
					go_on = 0; /* exit file */
				}
				else if(!strcmp(pipeline->command[0][0], "cd"))
				{
					if (pipeline->narguments[0] == 1)
					{
						printf("Error: missing path argument\n");
						break;
					}
					else
					{
						if(!chdir(pipeline->command[0][1])) {
							/* success */
							getcwd(pwd, MAX_FILENAME);
						}
						else { /* fail */
							printf("Error: %s\n", strerror(errno));
							break;
						}
					}
				}
				else
				{
					memset(cmd, 0, sizeof (cmd)); /* cleaning cmd buffer */
					for (i = 0; pipeline->command[i][0]; i++)
					{
						for (j = 0; pipeline->command[i][j]; j++)
						{
							strcat(cmd, pipeline->command[i][j]);
							strcat(cmd, " ");
						}
					}

					runcmd(cmd, RUN_FOREGROUND(pipeline), &result, pfd);
				}
			}
		}
	}
	release_command_line(command_line);
	release_pipeline(pipeline);
	close(fd);
	return 1;
}

int run_pipe(int *pfd, char *cmd1, char *cmd2, int fg, int *io)
{
    int i, aux, status;
    int pid[2];
    char *args1[RCMD_MAXARGS], *args2[RCMD_MAXARGS];
    char *cmd;

    i = 0;
    args1[i++] = strtok (cmd1, RCMD_DELIM);
    while ((i < RCMD_MAXARGS) && (args1[i++] = strtok (NULL, RCMD_DELIM)));

    i = 0;
    args2[i++] = strtok (cmd2, RCMD_DELIM);
    while ((i < RCMD_MAXARGS) && (args2[i++] = strtok (NULL, RCMD_DELIM)));

    /* Create a subprocess. */
    pid[0] = fork();
    sysfail (pid[0] < 0, -1);

    if (pid[0] > 0)      /* Caller process (parent). */
    {
    	pid[1] = fork();
    	sysfail(pid[1] < 0, -1);
    	/* second process */
    	if(pid[1] > 0) {
	    	/* run fg or bg */
	        if(fg) {
	            aux = wait(&status);
	        }
	        else {
	            aux = run_bg(pid[1], getpgid(pid[1]), cmd2);
	        }
	        sysfail (aux < 0, -1);
	    }
	    else        /* Subprocess (child) */
	    {
	    	close(0);
		    dup(pfd[0]);
		    close(pfd[1]);
		    execvp(args2[0], args2);
	    }
    }
    else        /* Subprocess (child) */
    {
    	close(1);
	    dup(pfd[1]);
	    close(pfd[0]);
	    execvp(args1[0], args1);
    }
	return 1;
}
