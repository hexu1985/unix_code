#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAXLINE     4096    /* max text line length */

#define	MAXN	16384		/* max # bytes to request from server */

int
tcp_connect(const char *host, const char *serv);

ssize_t						/* Read "n" bytes from a descriptor. */
readn(int fd, void *vptr, size_t n);

int
main(int argc, char **argv)
{
	int		i, j, fd, nchildren, nloops, nbytes;
	pid_t	pid;
	ssize_t	n;
	char	request[MAXLINE], reply[MAXN];

	if (argc != 6) {
		fprintf(stderr, "usage: client <hostname or IPaddr> <port> <#children> "
				 "<#loops/child> <#bytes/request>\n");
		exit(1);
	}

	nchildren = atoi(argv[3]);
	nloops = atoi(argv[4]);
	nbytes = atoi(argv[5]);
	snprintf(request, sizeof(request), "%d\n", nbytes);	/* newline at end */

	for (i = 0; i < nchildren; i++) {
		if ( (pid = fork()) < 0) {
			perror("fork error");
			exit(1);
		} else if (pid == 0) {		/* child */
			for (j = 0; j < nloops; j++) {
				fd = tcp_connect(argv[1], argv[2]);

				/*
				 * We want to see what happens to the server when it has
				 * connections outstanding and an RST arrives for one of
				 * them, before the connection is accepted.
				 * With the XTI server, this should generate some
				 * T_DISCONNECT events from t_accept(), which must be
				 * handled correctly.
				 *
				 * We do this for every third connection from the third
				 * client child.  (Could add more command-line args ...)
				 */

				if (i == 2 && (j % 3) == 0) {
					struct linger	ling;

					ling.l_onoff = 1;
					ling.l_linger = 0;
					if (setsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) < 0) {
						perror("setsockopt error");
						exit(1);
					}
					if (close(fd) == -1) {
						perror("close error");
						exit(1);
					}

					/* and just continue on for this client connection ... */
					fd = tcp_connect(argv[1], argv[2]);
				}

				n = strlen(request);
				if (write(fd, request, n) != n) {
					perror("write error");
					exit(1);
				}

				if ( (n = readn(fd, reply, nbytes)) < 0) {
					perror("readn error");
					exit(1);
				} else if (n != nbytes) {
					fprintf(stderr, "server returned %d bytes\n", n);
					exit(1);
				}

				if (close(fd) == -1) {		/* TIME_WAIT on client, not server */
					perror("close error");
					exit(1);
				}
			}
			printf("child %d done\n", i);
			exit(0);
		}
		/* parent loops around to fork() again */
	}

	while (wait(NULL) > 0)	/* now parent waits for all children */
		;
	if (errno != ECHILD) {
		perror("wait error");
		exit(1);
	}

	exit(0);
}
