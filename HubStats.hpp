#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

class HubStats
{
	int pipefd[2];

	public:

	void Start()
	{
		pid_t cpid;

		if (pipe(pipefd) == -1) {
			perror("pipe");
			exit(EXIT_FAILURE);
		}

		cpid = fork();
		if (cpid == -1) {
			perror("fork");
			exit(EXIT_FAILURE);
		}

		if (cpid == 0) {    /* Child reads from pipe */
			close(pipefd[1]);          /* Close unused write end */

			dup2(pipefd[0], STDIN_FILENO);

			if( fcntl(pipefd[0], F_GETFD) != -1 || errno != EBADF)
			{
#ifdef DEBUG
				std::cout << "Pipe open" << std::endl;
#endif
			}
			else
			{
				std::cout << "Pipe NOT open" << std::endl;
				abort();
			}

			if( fcntl(STDIN_FILENO, F_GETFD) != -1 || errno != EBADF)
			{
#ifdef DEBUG
				std::cout << "STDIN open" << std::endl;
#endif
			}
			else
			{
				std::cout << "STDIN NOT open" << std::endl;
				abort();
			}

			if(std::getenv("HUBSTATS_FILE") != NULL)
			{
				int statsFile = open(std::getenv("HUBSTATS_FILE"), O_APPEND|O_CREAT|O_RDWR);
				dup2(statsFile, STDOUT_FILENO);
				dup2(statsFile, STDERR_FILENO);	
			}

			//int ret = execl("/usr/bin/hubstats", "hubstats", "-c", "dump_metrics", "/usr/bin/cat", NULL);
			int ret = execl("/opt/sgi/uvstats/bin/hubstats", "hubstats", "-c", "dump_metrics", "cat", NULL);
			//int ret = execl("/usr/bin/echo", "echo", "c", "nlSummary", "cat", NULL);
			if (ret == -1)
			{
				close(pipefd[0]);
				perror("execl");
				exit(EXIT_FAILURE);
			}

			_exit(EXIT_SUCCESS);

		} else {            /* Parent writes argv[1] to pipe */
			close(pipefd[0]);          /* Close unused read end */
		}
	}

	void Stop()
	{
		close(pipefd[1]);          /* Reader will see EOF */
		wait(NULL);
	}

};
