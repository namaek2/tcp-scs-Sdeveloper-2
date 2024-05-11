#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <arpa/inet.h>
#include <sys/socket.h>
#endif // __linux
#ifdef WIN32
#include <winsock2.h>
#include "../mingw_net.h"
#endif // WIN32
#include <thread>

#ifdef WIN32
void myerror(const char* msg) { fprintf(stderr, "%s %ld\n", msg, GetLastError()); }
#else
void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }
#endif

#define MAX_CLIENT 5

void usage() {
	printf("tcp server %s\n",
#include "../version.txt"
   );
	printf("\n");
	printf("syntax: ts <port> [-e[-b]] [-si <src ip>]\n");
	printf("  -e : echo\n");
    printf("  -b : broadcast\n");
	printf("sample: ts 1234 -e -b\n");
}

struct Param {
	bool echo{false};
    bool broad_cast{false};
	uint16_t port{0};
	uint32_t srcIp{0};

	bool parse(int argc, char* argv[]) {
		for (int i = 1; i < argc;) {
			if (strcmp(argv[i], "-e") == 0) {
				echo = true;
				i++;
				continue;
			}

            if (strcmp(argv[i], "-b") == 0) {
                broad_cast = true;
                i++;
                continue;
            }

			if (strcmp(argv[i], "-si") == 0) {
				int res = inet_pton(AF_INET, argv[i + 1], &srcIp);
				switch (res) {
					case 1: break;
					case 0: fprintf(stderr, "not a valid network address\n"); return false;
					case -1: myerror("inet_pton"); return false;
				}
				i += 2;
				continue;
			}

			if (i < argc) port = atoi(argv[i++]);
		}
		return port != 0;
	}
} param;

void recvThread(int sd, int sd_arr[], int *sd_i) {
    if(*sd_i == MAX_CLIENT) {
        printf("MAX_CLIENT\n");
        printf("refused\n");
        fflush(stdout);
        ::close(sd);
    }

    for(int i=0; i < MAX_CLIENT; i++) {
        if(sd_arr[i] == -1) {
            sd_arr[i] = sd;
            *sd_i+=1;
            break;
        }
    }

    printf("sd_i: %d\n", *sd_i);
    printf("sd_arr[%d]: %d\n", *sd_i - 1, sd_arr[*sd_i - 1]);

	printf("connected %d\n", sd);
	fflush(stdout);
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];
	while (true) {
		ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
		if (res == 0 || res == -1) {
			fprintf(stderr, "recv return %ld", res);
			myerror(" ");
			break;
		}
		buf[res] = '\0';
		printf("%s", buf);
		fflush(stdout);

		if (param.echo) {
            if (param.broad_cast) {
                for(int i=0; i<MAX_CLIENT; i++) {
                    if (sd_arr[i] != -1) {
                        res = ::send(sd_arr[i], buf, res, 0);
                        if (res == 0 || res == -1) {
                            fprintf(stderr, "send return %ld", res);
                            myerror(" ");
                            break;
                        }
                    }
                }
            }
		}
	}
	printf("disconnected\n");
	fflush(stdout);
    *sd_i-=1;
    for (int i=0; i<MAX_CLIENT; i++) {
        if(sd_arr[i] == sd) {
            sd_arr[i] = -1;
        }
    }
	::close(sd);
}

int main(int argc, char* argv[]) {
	if (!param.parse(argc, argv)) {
		usage();
		return -1;
	}

#ifdef WIN32
	WSAData wsaData;
	WSAStartup(0x0202, &wsaData);
#endif // WIN32

	//
	// socket
	//
	int sd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		myerror("socket");
		return -1;
	}

#ifdef __linux__
	//
	// setsockopt
	//
	{
		int optval = 1;
		int res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
		if (res == -1) {
			myerror("setsockopt");
			return -1;
		}
	}
#endif // __linux

	//
	// bind
	//
	{
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = param.srcIp;
		addr.sin_port = htons(param.port);

		ssize_t res = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
		if (res == -1) {
			myerror("bind");
			return -1;
		}
	}

	//
	// listen
	//
	{
		int res = listen(sd, 5);
		if (res == -1) {
			myerror("listen");
			return -1;
		}
	}

    int sd_arr[MAX_CLIENT] = {};
    int sd_i = 0;

    for(int i=0; i<MAX_CLIENT; i++) {
        sd_arr[i] = -1;
    }

	while (true) {
		struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int newsd = ::accept(sd, (struct sockaddr *)&addr, &len);
        //왜 새로 커넥트하면 newsd가 4 부터 시작? 그리고 4, 5 가 연결되고 5를 디스커넥트 하고 다시 연결하면 6이 연결.
        // 6을 끊고 다시 연결하면 5로 연결됨

		if (newsd == -1) {
			myerror("accept");
			break;
		}
		std::thread* t = new std::thread(recvThread, newsd, sd_arr, &sd_i);
		t->detach();
	}
	::close(sd);
}
