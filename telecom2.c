
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <string.h>

int connect_retry(const char *server_ip, int server_port)
{
    while (1)
    {
        int s = socket(PF_INET, SOCK_STREAM, 0);
        if (s < 0)
        {
            perror("socket");
            sleep(1);
            continue;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(server_ip);
        addr.sin_port = htons(server_port);

        int ret = connect(s, (struct sockaddr *)&addr, sizeof(addr));
        if (ret == 0)
        {
            printf("connected\n");
            return s;
        }

        perror("connect");
        close(s);

        // 1秒待って再試行
        sleep(1);
    }
}

int client_recv(char *server_ip, int server_port)
{
    // 接続が成功するまでやり直す.
    int s = connect_retry(server_ip, server_port);

    FILE *fp;
    const char *cmdline = "play -t raw -b 16 -c 1 -e signed-integer -r 44100 -";
    fp = popen(cmdline, "w");
    if (fp == NULL)
    {
        perror("popen");
        close(s);
        return 1;
    }

    char data[4096];
    while (1)
    {
        ssize_t n = recv(s, data, sizeof(data), 0);
        if (n < 0)
        {
            perror("recv");
            break;
        }
        if (n == 0)
        {
            break;
        }

        if (fwrite(data, 1, n, fp) != (size_t)n)
        {
            perror("fwrite");
            break;
        }
    }

    close(s);
    pclose(fp);
    return 0;
}

int server_send(int port)
{
    // host
    int ss = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;           /* 最終的にbind に渡すアドレス情報 */
    addr.sin_family = AF_INET;         /* このアドレスはIPv4 アドレスです */
    addr.sin_port = htons(port);       /* ポート...で待ち受けしたいです */
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
    fp = popen(cmdline, "r");

    char recorded_sound[4096];
    while (1)
    {
        size_t n = fread(recorded_sound, 1, sizeof(recorded_sound), fp);
        if (n == 0)
        {
            if (feof(fp))
            {
                break;
            }
            if (ferror(fp))
            {
                perror("fread");
                break;
            }
            continue;
        }

        size_t sent = 0;
        while (sent < n)
        {
            // このmはnより小さくなりうる.だから,nになるまで詰める.
            ssize_t m = send(s, recorded_sound + sent, n - sent, 0);
            if (m < 0)
            {
                perror("send");
                break;
            }
            if (m == 0)
            {
                break;
            }
            sent += (size_t)m;
        }
        if (sent < n)
        {
            break;
        }
    }

    pclose(fp);
    close(s);
    close(ss);

    return 0;
}

int main(int argc, char *argv[])
{
    int server_port = 50000;
    // char *server_ip = "192.168.100.14";
    char *server_ip = "192.168.100.37";

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return 1;
    }

    if (pid == 0)
    {
        // 子プロセス: client 側を実行
        client_recv(server_ip, server_port);
        return 1;
    }

    // 親プロセス: server 側を実行
    server_send(server_port);
}
