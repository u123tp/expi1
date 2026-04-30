#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

void start_duplex(int sock_fd)
{
	// 低遅延設定
	int nodelay = 1;
	setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

	pid_t pid = fork();

	if (pid < 0) {
		perror("fork");
		exit(1);
	}

	if (pid == 0) {
		/* 子プロセス: 受信 (Network -> Play) */
		FILE* play_fp = popen("play -q -t raw -b 16 -c 1 -e s -r 44100 -", "w");
		if (!play_fp) {
			perror("popen play");
			exit(1);
		}

		char buf[BUFFER_SIZE];
		ssize_t n;
		while ((n = read(sock_fd, buf, sizeof(buf))) > 0) {
			fwrite(buf, 1, n, play_fp);
		}
		pclose(play_fp);
		exit(0);
	}
	else {
		/* 親プロセス: 送信 (Rec -> Network) */
		FILE* rec_fp = popen("rec -q -t raw -b 16 -c 1 -e s -r 44100 -", "r");
		if (!rec_fp) {
			perror("popen rec");
			exit(1);
		}

		char buf[BUFFER_SIZE];
		ssize_t n;
		int rec_fd = fileno(rec_fp);
		while ((n = read(rec_fd, buf, sizeof(buf))) > 0) {
			if (write(sock_fd, buf, n) <= 0) break;
		}
		pclose(rec_fp);
		kill(pid, SIGKILL);	 // 送信が終われば受信プロセスも終了させる
	}
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage (Server): %s <Port>\n", argv[0]);
		fprintf(stderr, "Usage (Client): %s <IP> <Port>\n", argv[0]);
		return 1;
	}

	int sock_fd;
	if (argc == 2) {  // Server Mode
		int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
		int opt = 1;
		setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		struct sockaddr_in addr = {.sin_family = AF_INET,
								   .sin_port = htons(atoi(argv[1])),
								   .sin_addr.s_addr = INADDR_ANY};
		bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
		listen(listen_fd, 1);
		printf("Waiting for caller...\n");
		sock_fd = accept(listen_fd, NULL, NULL);
		close(listen_fd);
	}
	else {	// Client Mode
		sock_fd = socket(AF_INET, SOCK_STREAM, 0);
		struct sockaddr_in addr = {.sin_family = AF_INET,
								   .sin_port = htons(atoi(argv[2]))};
		inet_pton(AF_INET, argv[1], &addr.sin_addr);
		printf("Calling %s...\n", argv[1]);
		if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			perror("connect");
			return 1;
		}
	}

	printf("Connected! Start talking.\n");
	start_duplex(sock_fd);
	close(sock_fd);
	return 0;
}