#include <sys/socket.h>
#include <unistd.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Following shortens all the typecasts of pointer arguments: */
#define SA  struct sockaddr

#define MAXLINE     4096    /* max text line length */

char *
sock_ntop_host(const struct sockaddr *sa, socklen_t salen);

static void	recvfrom_alarm(int);

void
dg_cli(FILE *fp, int sockfd, const SA *pservaddr, socklen_t servlen)
{
	int				n;
	const int		on = 1;
	char			sendline[MAXLINE], recvline[MAXLINE + 1];
	fd_set			rset;
	sigset_t		sigset_alrm, sigset_empty;
	socklen_t		len;
	struct sockaddr	*preply_addr;
 
	if ((preply_addr = malloc(servlen)) < 0) {
		perror("malloc error");
		exit(1);
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
		perror("setsockopt error");
		exit(1);
	}

	FD_ZERO(&rset);

	if (sigemptyset(&sigset_empty) == -1) {
		perror("sigemptyset error");
		exit(1);
	}

	if (sigemptyset(&sigset_alrm) == -1) {
		perror("sigemptyset error");
		exit(1);
	}

	if (sigaddset(&sigset_alrm, SIGALRM) == -1) {
		perror("sigaddset error");
		exit(1);
	}

	if (signal(SIGALRM, recvfrom_alarm) < 0) {
		perror("signal error");
		exit(1);
	}

	while (fgets(sendline, MAXLINE, fp) != NULL) {
		n = strlen(sendline);
		if (sendto(sockfd, sendline, n, 0, pservaddr, servlen) != n) {
			perror("sendto error");
			exit(1);
		}

		if (sigprocmask(SIG_UNBLOCK, &sigset_alrm, NULL) == -1) {
			perror("sigprocmask error");
			exit(1);
		}
		alarm(5);
		for ( ; ; ) {
			FD_SET(sockfd, &rset);
			n = pselect(sockfd+1, &rset, NULL, NULL, NULL, &sigset_empty);
			if (n < 0) {
				if (errno == EINTR) {
					break;
				} else {
					perror("pselect error");
					exit(1);
				}
			} else if (n != 1) {
				fprintf(stderr, "pselect error: returned %d: %s\n", n, strerror(errno));
				exit(1);
			}

			len = servlen;
			if ((n = recvfrom(sockfd, recvline, MAXLINE, 0, preply_addr, &len)) < 0) {
				perror("recvfrom error");
				exit(1);
			}
			recvline[n] = 0;	/* null terminate */
			printf("from %s: %s",
					sock_ntop_host(preply_addr, len), recvline);
		}
	}
	if (ferror(fp)) {
		perror("fgets error");
		exit(1);
	}
	free(preply_addr);
}

static void
recvfrom_alarm(int signo)
{
	return;		/* just interrupt the recvfrom() */
}
