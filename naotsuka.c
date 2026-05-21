#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    const char *usage = "Usage for client: %s -c <server_ip> [port (optional 50000 if not specified)] (or Usage for server: %s -s [port (optional 50000 if not specified)])\n";
    // API仕様：エラー時に返す

    const char *protocol = "-t raw -b 16 -c 1 -e signed-integer -r 48000 -";
    char rec_command[1024];
    sprintf(rec_command, "rec %s", protocol);
    // recの引数を指定する
    
    char play_command[1024];
    sprintf(play_command, "play %s", protocol);
    // playの引数を指定する

    if (argc < 2 || argc > 4) {
        fprintf(stderr, usage, argv[0], argv[0]);
        return 1;
    }
    // 引数：client: -c server_ip [port], server: -s [port]
    // clientはserver_ipとportを指定する必要がある。
    // serverはportを指定する必要がある。
    // portは50000でデフォルト。
    // 引数はclient：1+(1)個，server：0+(1)個，合計2個まで。

    int port = 50000;
    char *server_ip = NULL;
    if (strcmp(argv[1], "-c") == 0) {// クライアントのとき
        if (argc < 3) {// 引数が足りない場合
            fprintf(stderr, usage, argv[0], argv[0]);
            return 1;
        }
        server_ip = argv[2];//第２引数：サーバのIPアドレス
        if (argc == 4) {//第３引数：ポート番号(optional)
            port = atoi(argv[3]);//atoi: 文字列を整数に変換
        }
    } else if (strcmp(argv[1], "-s") | strcmp(argv[1], "-l") == 0) {
        if (argc == 3) {//第２引数：ポート番号(optional)
            port = atoi(argv[2]);//atoi: 文字列を整数に変換
        }
    } else {//引数が間違っている場合
        fprintf(stderr, usage, argv[0], argv[0]);
        return 1;
    }

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "invalid port: %d\n", port);
        return 1;
    }
    // portが0以下か65535以上の場合はエラー

    int s = socket(PF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return 1;
    }
    // 待ち受けるソケットを作る(サーバクライアント両方同じコード)

    if (strcmp(argv[1], "-c") == 0) {// クライアントの処理
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        int addr_ok = inet_aton(server_ip, &addr.sin_addr);
        // inet_aton: IPアドレスをバイナリに変換
        // 引数:文字列のIPアドレス,変換結果を書き込む先のアドレス
        // 返り値:1 : 変換成功，0 : 変換失敗

        if (addr_ok != 1) {
            fprintf(stderr, "invalid IPv4 address: %s\n", server_ip);
            close(s);
            return 1;
        }

        int connect_ok = connect(s, (struct sockaddr *)&addr, sizeof(addr));
        // connect: ソケットを接続する
        // 引数:ソケット, 接続先のアドレス, 接続先のアドレスのサイズ
        // (struct sockaddr *)&addr: 接続先のアドレス
        // sizeof(addr): 接続先のアドレスのサイズ
        // 返り値:0 : 成功，-1 : 失敗

        if (connect_ok < 0) {
            perror("connect");
            close(s);
            return 1;
        }
        printf("接続成功: %s:%d\n", server_ip, port);

    } else if (strcmp(argv[1], "-s") == 0) {// サーバの処理
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        int bind_ok = bind(s, (struct sockaddr *)&addr, sizeof(addr));
        // bind: ソケットをバインドする
        // 引数:ソケット, バインドするアドレス, バインドするアドレスのサイズ
        // (struct sockaddr *)&addr: バインドするアドレス
        // sizeof(addr): バインドするアドレスのサイズ
        // 返り値:0 : 成功，-1 : 失敗
        if (bind_ok < 0) {
            perror("bind");
            return 1;
        }

        int listen_ok = listen(s, 10);
        // listen: ソケットをリッスンする
        // 引数:ソケット, リッスンするキューの最大長
        // 返り値:0 : 成功，-1 : 失敗
        if (listen_ok < 0) {
            perror("listen");
            close(s);
            return 1;
        }
        printf("待ち受け開始: %d\n", port);

        int conn = accept(s, NULL, NULL);
        // accept: ソケットを受け取る
        // 引数:ソケット, 受け取るアドレス, 受あ取るアドレスのサイズ
        // (struct sockaddr *)&addr: 受け取るアドレス
        // sizeof(addr): 受け取るアドレスのサイズ
        // 返り値:0 : 成功，-1 : 失敗
        if (conn < 0) {
            perror("accept");
            close(s);
            return 1;
        }
        printf("接続成功: %d\n", port);
        close(s);
        s = conn;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(s);
        return 1;
    }
    // fork： プロセスを複数作る(受信と送信)
    // 返り値： 子プロセスのPID : 成功，-1 : 失敗

    int N = 1024; // バッファサイズ(一度に処理する個数)
    if (pid == 0) {// 子プロセス: 受信した音声を play に流す
        FILE *play_fp = popen(play_command, "w");
        if (play_fp == NULL) {
            perror("popen(play)");
            close(s);
            return 1;
        }
        // popen: コマンドを実行する。
        // 引数:実行するコマンド, モード
        // 返り値:ファイルディスクリプタ : 成功，NULL : 失敗
        // この返り値play_fpはcommandの標準入力に対応する(ここにものを流し込むとplayに流れる)

        while (1) {
            char buf[N];
            int n = recv(s, buf, N, 0);
            if (n < 0) {
                perror("recv");
                pclose(play_fp);
                close(s);
                return 1;
            }
            if (n == 0) { break; } // 標準入力終了
            size_t written = fwrite(buf, 1, n, play_fp);
            // fwrite: ファイルに書き込む
            // 引数:書き込むデータ, 書き込むデータのサイズ, 書き込むデータの個数, 書き込むファイル
            // 返り値:書き込んだデータの個数 : 成功，-1 : 失敗
            // ここで書き込んだデータはplay_fp(標準入力に対応するデータ)として，playに送信される。

            if (written < (size_t)n) {
                perror("fwrite(play)");
                pclose(play_fp);
                close(s);
                return 1;
            }
            fflush(play_fp);
            // fflush: ファイルをフラッシュ(バッファリングしているものをplayに送信する)する
            // 引数:フラッシュするファイル
            // 返り値:0 : 成功，-1 : 失敗
        }
        pclose(play_fp);
        close(s);
        return 0;
    } else {// 親プロセス: rec した音声を送信
        FILE *rec_fp = popen(rec_command, "r");
        if (rec_fp == NULL) {
            perror("popen(rec)");
            close(s);
            return 1;
        }
        // popen: コマンドを実行する。
        // 引数:実行するコマンド, モード
        // 返り値:ファイルディスクリプタ : 成功，NULL : 失敗
        // この返り値rec_fpはcommandの標準出力に対応する(ここから読むとrecから出力される)
        // 先ほどは'w'で標準入力に対応するファイルディスクリプタを作ったが，ここでは'r'で標準出力に対応するファイルディスクリプタを作っている。

        while (1) {
            char buf[N];
            size_t n = fread(buf, 1, N, rec_fp);
            // fread: ファイルから読み込む
            // 引数:読み込むデータ, 読み込むデータのサイズ, 読み込むデータの個数, 読み込むファイル
            // 返り値:読み込んだデータの個数 : 成功，-1 : 失敗
            // ここで読み込んだデータはrec_fp(標準出力に対応するデータ)として，recに送信される。

            if (n == 0 && ferror(rec_fp)) {
                perror("fread");
                pclose(rec_fp);
                close(s);
                return 1;
            }
            if (n == 0) { break; } // 標準出力終了
            size_t sent = 0;
            while (sent < n) { // 送信中
                int m = send(s, buf + sent, n - sent, 0);
                // send: データを送信する
                // 引数:送信するデータ, 送信するデータのサイズ, 送信するデータの個数, 送信するデータのフラグ
                // 返り値:送信したデータの個数 : 成功，-1 : 失敗
                // ここで送信したデータはs(ソケット)に送信される。
                if (m < 0) { // 送信失敗
                    perror("send");
                    pclose(rec_fp);
                    close(s);
                    return 1;
                }
                sent += m;
            }
        }
        pclose(rec_fp);
        shutdown(s, SHUT_WR);
        wait(NULL);
        close(s);
        return 0;
    }
    // 親子のどちらかしか行っていないように見える(ifなので)が，実際は両方行っている，pid==0とそうじゃないのを別ルートに分けている感じ。
}