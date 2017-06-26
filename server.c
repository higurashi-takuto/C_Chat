#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 定数 */
#define PORT 10000    // 使用するポート番号

/* プロトタイプ宣言 */
void errorCheck(int, char *);

/* 構造体 */
struct client{    // クライアント情報
	int sockfd;    // ソケット接続用
	int state;    // 使用状況 1:使用 0:未使用
	char username[16];    // ユーザ名
};

int main(int argc, char* argv[]){
	/* 変数宣言 */
	/*
	announce:一時ソケット接続用
	maxfd:最大ファイルディスクリプタ
	closed:read用
	*/
	int i, j, sockfd, announce, maxfd, closed;
	int active = 0;    // アクティブユーザ数
	int num_of_user = 0;    // 全ユーザ数
	char message[157], orig[141], username[16], enter;    // enter:改行の空読み用
	struct sockaddr_in serv_addr;
	struct client user[4];    // 接続クライアント
	fd_set fds, init;

	/* オプション */
	/*
	起動用のオプション
	-d  デバックモード
		例) ./a.out -d
	*/
	int opt, debug_flag = 0;
	while((opt = getopt(argc, argv, "d")) != -1){
		switch(opt){
			case 'd':
				debug_flag = 1;
				break;
		}
	}

	/* 構造体初期化 */
	for(i = 0; i < 4; i++){
		user[i].state = 0;
	}

	/* ソケット作成 */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	errorCheck(sockfd, "SOCKET");

	/* アドレス作成 */
	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(PORT);

	/* アドレス割当 */
	if(debug_flag){    // デバックモード時強制バインド
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &debug_flag, sizeof(int));
	}
	errorCheck(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)), "BIND");

	/* コネクション要求待ち */
	errorCheck(listen(sockfd, 8), "LISTEN");

	/* fd_set初期化 */
	FD_ZERO(&init);
	FD_SET(sockfd, &init);
	FD_SET(0, &init);
	maxfd = sockfd;

	while(1){
		/* select設定 */
		memcpy(&fds, &init, sizeof(fd_set));    // select対象のグループ初期化
		errorCheck(select(maxfd+1, &fds, NULL, NULL, NULL), "SELECT");    // selectで通信の監視対象を設定

		/* 新規ユーザ登録 */
		if(FD_ISSET(sockfd, &fds)){    // 新規の通信
			/* ユーザ上限 */
			/*
			ユーザ数が上限に達している場合その旨を送信
			*/
			if(4 <= num_of_user){
				announce = accept(sockfd, NULL, NULL);
				errorCheck(announce, "ACCEPT");
				errorCheck(read(announce, username, 16), "READ");
				sprintf(message, "申し訳ありませんが、現在、ユーザ数が上限です。");
				errorCheck(write(announce, message, 157), "WRITE");
			}
			/* 通常追加 */
			else{
				/* select対象に */
				user[num_of_user].sockfd = accept(sockfd, NULL, NULL);
				errorCheck(user[num_of_user].sockfd, "ACCEPT");
				FD_SET(user[num_of_user].sockfd, &init);
				if(maxfd < user[num_of_user].sockfd){
					maxfd = user[num_of_user].sockfd;
				}
				/* ユーザ数情報更新 */
				num_of_user++;
			}
		}

		/* 既存ユーザからの受信 */
		for(i = 0; i < num_of_user; i++){
			if(FD_ISSET(user[i].sockfd, &fds)){    // ユーザからの通信
				errorCheck(read(user[i].sockfd, orig, 141), "READ");
				/* 新規登録メッセージ */
				/*
				ユーザを追加し，ユーザ追加情報を全クライアントに送信
				*/
				if(strncmp(orig, "system_add_username:", 20) == 0){
					active++;
					user[i].state = 1;
					strncpy(user[i].username, orig+20, 15);
					sprintf(message, "%sが入室しました。現在の入室人数は%d人です。", user[i].username, active);
					for(j = 0; j < num_of_user; j++){
						if(user[j].state){
							errorCheck(write(user[j].sockfd, message, 157), "WRITE");
						}
					}
					continue;
				}
				/* 退出 */
				/*
				ユーザを未使用にし，退出情報を全クライアントに送信
				*/
				if(strcmp(orig, "exit") == 0){
					for(j = 0; j < num_of_user; j++){
						if(user[j].state){
							sprintf(message, "%sが退室しました。", user[i].username);
							errorCheck(write(user[j].sockfd, message, 157), "WRITE");
						}
					}
					FD_CLR(user[i].sockfd, &init);    // select監視グループから消去
					user[i].state = 0;
					active--;
					continue;
				}
				/* 空メッセージ処理 */
				if(strcmp(orig, "") == 0){
					continue;
				}
				/* 通常 */
				sprintf(message, "%-15s:%s", user[i].username, orig);
				for(i = 0; i < num_of_user; i++){
					if(user[i].state){
						errorCheck(write(user[i].sockfd, message, 157), "WRITE");
					}
				}
			}
		}

		/* サーバコマンド */
		if(FD_ISSET(0, &fds)){    // キーボードからの標準入力
			scanf("%140[^\n]%*[^\n]", orig);
			scanf("%c", &enter);
			/* サーバダウン */
			/*
			サーバ終了のメッセージを全クライアントに送信する
			*/
			if(strcmp(orig, "exit") == 0){
				for(i = 0; i < num_of_user; i++){
					if(user[i].state){
						errorCheck(write(user[i].sockfd, "サーバがダウンしました。", 157), "WRITE");
					}
				}
				break;
			}
			/* サーバメッセージ */
			/*
			全クライアントにサーバからのメッセージを送信する
			*/
			sprintf(message, "server:%s", orig);
			for(i = 0; i <= num_of_user; i++){
				if(user[i].state){
					errorCheck(write(user[i].sockfd, message, 157), "WRITE");
				}
			}
		}
	}

	/* クライアント終了確認 */
	for(i = 0; i < num_of_user; i++){
		if(user[i].state){
			while(1){
				closed = read(user[i].sockfd, orig, 141);
				errorCheck(closed, "READ");
				if(closed == 0){
					break;
				}
			}
		}
	}

	/* サーバサイド終了 */
	for(i = 0; i < num_of_user; i++){
		if(user[i].state){
			errorCheck(close(user[i].sockfd), "CLOSE");
		}
	}
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