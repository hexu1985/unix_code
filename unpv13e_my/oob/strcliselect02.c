#include <sys/socket.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define MAXLINE     4096    /* max text line length */

#define max(a, b)	((a) < (b) ? (b) : (a))

ssize_t
readline(int fd, void *vptr, size_t maxlen);

ssize_t
writen(int fd, const void *vptr, size_t n);


void
heartbeat_cli(int servfd_arg, int nsec_arg, int maxnprobes_arg);

void
str_cli(FILE *fp, int sockfd)
{
	int			maxfdp1, stdineof = 0;
	fd_set		rset;
	char		sendline[MAXLINE], recvline[MAXLINE];
	int 		n;

	heartbeat_cli(sockfd, 1, 5);

	FD_ZERO(&rset);
	for ( ; ; ) {
		if (stdineof == 0)
			FD_SET(fileno(fp), &rset);
		FD_SET(sockfd, &rset);
		maxfdp1 = max(fileno(fp), sockfd) + 1;
		if (select(maxfdp1, &rset, NULL, NULL, NULL) < 0) {
			if (errno == EINTR) {
				continue;
			} else {
				perror("select error");
				exit(1);
			}
		}

		if (FD_ISSET(sockfd, &rset)) {	/* socket is readable */
			if ((n = readline(sockfd, recvline, MAXLINE)) < 0) {
				perror("readline error");
				exit(1);
			} if (n == 0) {
				if (stdineof == 1) {
					return;		/* normal termination */
				} else {
					fprintf(stderr, "str_cli: server terminated prematurely\n");
					exit(1);
				}
			}

			n = strlen(recvline);
			if (writen(STDOUT_FILENO, recvline, n) != n) {
				perror("writen error");
				exit(1);
			}
		}

		if (FD_ISSET(fileno(fp), &rset)) {  /* input is readable */
			if (fgets(sendline, MAXLINE, fp) == NULL) {
				if (ferror(fp)) {
					perror("fgets error");
					exit(1);
				}
				stdineof = 1;
				alarm(0);			/* turn off heartbeat */
				if (shutdown(sockfd, SHUT_WR) < 0) {	/* send FIN */
					perror("shutdown error");
					exit(1);
				}
				FD_CLR(fileno(fp), &rset);
				continue;
			}

			n = strlen(sendline);
			if (writen(sockfd, sendline, n) != n) {
				perror("writen error");
				exit(1);
			}
		}
	}
}
