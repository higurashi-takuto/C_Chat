#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 定数 */
#define PORT 10000    // 使用するポート番号

/* プロトタイプ宣言 */
void errorCheck(int, char *);

int main(int argc,char *argv[]){
	/* 変数宣言 */
	int sockfd;
	char username[16], message[157], orig[141], enter;    // enter:改行の空読み用
	struct sockaddr_in serv_addr;
	fd_set fds, init;

	/* オプション */
	/*
	起動用のオプション
	-d  デバックモード
		例) ./a.out -d
	-i  IPアドレス指定
		例) ./a.out -i 172.28.34.104
	*/
	int opt, debug_flag = 0;
	char ip[20] = "XXX.XXX.XXX.XXX";
	while((opt = getopt(argc, argv, "di:")) != -1){
		switch(opt){
			case 'd':
				debug_flag = 1;
				break;
			case 'i':
				strcpy(ip,optarg);
				printf("IPアドレス:%s\n", ip);
				break;
		}
	}

	/* ソケット作成 */
	errorCheck(sockfd = socket(AF_INET, SOCK_STREAM, 0), "SOCKET");

	/* アドレス作成 */
	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip);
	serv_addr.sin_port = htons(PORT);

	/* コネクション要求 */
	errorCheck(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)), "CONNECT");
	printf("\nWelcome to \"C Chat\"!\n\n");
	printf("・退室したい時には\"exit\"と入力してください。\n\n");

	/* ユーザ登録 */
	printf("ユーザ名\n\t");
	scanf("%15[^\n]%*[^\n]", username);
	scanf("%c", &enter);
	sprintf(message, "system_add_username:%s", username);
	errorCheck(write(sockfd, message, 141), "WRITE");

	/* fd_set初期化 */
	FD_ZERO(&init);
	FD_SET(sockfd, &init);
	FD_SET(0, &init);

	while(1){
		/* select設定 */
		memcpy(&fds, &init, sizeof(fd_set));    // select対象のグループ初期化
		errorCheck(select(sockfd+1, &fds, NULL, NULL, NULL), "SELECT");    // selectで通信の監視対象を設定

		/* 読み込み */
		if(FD_ISSET(sockfd, &fds)){    // 新規の通信
			errorCheck(read(sockfd, message, 157), "READ");
			printf("%s\n", message);
			/* サーバダウン受信時 */
			if(strcmp(message, "サーバがダウンしました。") == 0){
				break;
			}
			/* ユーザ上限時 */
			if(strcmp(message, "申し訳ありませんが、現在、ユーザ数が上限です。") == 0){
				break;
			}
		}

		/* 書き込み */
		if(FD_ISSET(0, &fds)){    // キーボードからの標準入力
			scanf("%140[^\n]%*[^\n]", orig);
			scanf("%c", &enter);
			errorCheck(write(sockfd, orig, 141), "WRITE");
			if(strcmp(orig, "exit") == 0){
				break;
			}
			/* メッセージの初期化 */
			strcpy(orig, "");
		}

		if(debug_flag){    // デバックモード時1秒スリープ
			sleep(1);
		}
	}

	/* クローズ */
	errorCheck(close(sockfd), "CLOSE");
	return 0;
}

/**********
関数名:	errorCheck()
概要　:	関数のエラーチェックを行う関数
引数　:	int:関数の戻り値，char *:関数名
戻り値:	void
詳細　:	第1引数で受け取った関数の戻り値について値が-1であれば，エラーを出力する．
		その際にどの関数のエラーなのかがわかるように第2引数で関数名を受け取り，表示する．
**********/
void errorCheck(int rtn, char *func){
	if(rtn == -1){
		char message[64];
		sprintf(message, "ERROR AT %s", func);
		perror(message);
		exit(1);
	}
	return;
}