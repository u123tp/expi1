#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>



int client(int client_port){

    // host
    int ss = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr; /* 最終的にbind に渡すアドレス情報 */
    addr.sin_family = AF_INET; /* このアドレスはIPv4 アドレスです */
    addr.sin_port = htons(client_port); /* ポート...で待ち受けしたいです */
    addr.sin_addr.s_addr = INADDR_ANY; /* どのIP アドレスでも待ち受けしたいです */
    bind(ss, (struct sockaddr *)&addr, sizeof(addr));
    listen(ss, 10);

    // client
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(struct sockaddr_in);
    int s = accept(ss, (struct sockaddr *)&client_addr, &len);

    // データ読み込み
    FILE *fp;
    char *cmdline = "rec -t raw -b 16 -c 1 -e signed-integer -r 44100 -";
    fp=popen(cmdline, "r");

    char recorded_sound[4096];
    while (1) {
        size_t n = fread(recorded_sound, 1, sizeof(recorded_sound), fp);
        if (n == 0) {
            if (feof(fp)) {
                break;
            }
            if (ferror(fp)) {
                perror("fread");
                break;
            }
            continue;
        }
        
        
        size_t sent = 0;
        while (sent < n) {
            ssize_t m = send(s, recorded_sound + sent, n - sent, 0);
            if (m < 0) {
                perror("send");
                break;
            }
            if (m == 0) {
                break;
            }
            sent += (size_t)m;
        }
        if (sent < n) {
            break;
        }
    }

    pclose(fp);
    close(s);
    close(ss);

    return 0;
}



int server(int host_port){
    int s = socket(PF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("192.168.100.34");
    addr.sin_port = htons(host_port);
    
    int ret = connect(s, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        perror("connect");
        close(s);
        return 1;
    };

    FILE *fp;
    const char *cmdline = "play -t raw -b 16 -c 1 -e signed-integer -r 44100 -";
    fp = popen(cmdline, "w");
    

    char data[4096];
    while (1) {
        ssize_t n = recv(s, data, sizeof(data), 0);
        if (n < 0) {
            perror("recv");
            break;
        }
        if (n == 0) {
            break;
        }

        if (fwrite(data, 1, n, fp) != (size_t)n) {
            perror("fwrite");
            break;
        }
    }

    close(s);
    pclose(fp);
    return 0;
}


int main(int argc, char *argv[]) {
    int port = 50000;

    if (argc >= 2) {
        if (strcmp(argv[1], "client") == 0) {
            return client(port);
        }

        if (strcmp(argv[1], "server") == 0) {
            return server(port);
        }
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // 子プロセス: client 側を実行
        execlp(argv[0], argv[0], "client", NULL);
        perror("execlp client");
        exit(1);
    } else {
        // 親プロセス: client が listen するまで少し待つ
        sleep(1);

        // 親プロセス: server 側を実行
        execlp(argv[0], argv[0], "server", NULL);
        perror("execlp server");

        wait(NULL);
        return 1;
    }
}
