#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <errno.h>

#define BUF_SIZE 1024
#define CACHE_SIZE 1024*1024*64
#define uchar unsigned char

typedef struct {
    char value;
} byte;

typedef struct {
  int port;
  char* dir_name;
} ArgOption;

//-1: has no argument; -2: argument num is incorrect; -3: option name not right
int parse_arg(int argc, char** argv, ArgOption* argopt) {
  if (argc <= 1) {
    return -1;
  }
  if (argc % 2 == 0) {
    return -2;
  }

  memset(argopt, 0, sizeof(ArgOption));
  for (int i=1; i<argc; i++) {
    if (strcmp(argv[i],"-p") == 0) {
      argopt->port = atoi(argv[++i]);
    }
    if (strcmp(argv[i],"-d") == 0) {
      argopt->dir_name = argv[++i];
    }
  }
  if (argopt->dir_name == NULL || argopt->port == 0) {
    return -2;
  }

  return 0;
} 

void flush_to_file(char* cache, int len, FILE* fp) {
  fwrite(cache, 1, len, fp);
  return ;
}

int main(int argc, char** argv){
  ArgOption* argopt = (ArgOption*)malloc(sizeof(ArgOption));
  if (NULL == argopt) {
    printf("malloc error\n");
    exit(EXIT_FAILURE);
  }  
  int ret = parse_arg(argc, argv, argopt);
  if (ret == -1) {
    printf("no argument, usage: -p ${port} -d ${data_cache}\nrequired: -p, -d\n");
    exit(1);
  }
  if (ret == -2) {
    printf("argument error, usage: -p ${port} -d ${data_cache}\nrequired: -p, -d\n");
    exit(1);
  }

  int serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;  //IPv4地址
  serv_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
  serv_addr.sin_port = htons(argopt->port); 
  int opt = 1;
  setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt, sizeof(opt));
  ret = bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  if (-1 == ret) {
    perror("bind socket error");
    close(serv_sock);
    exit(1);
  }

  listen(serv_sock, 20);

  //char cache[CACHE_SIZE] = {0}; //用栈数组容易超出栈帧大小
  char *cache = (char *)malloc(CACHE_SIZE*sizeof(char));
  memset(cache, 0, CACHE_SIZE*sizeof(char));
  int cache_cur;
  while (1) {
    cache_cur = 0;
    //接收客户端请求
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    int clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
    
    FILE* fp = NULL;
    long recv_size = 0, file_size = -1;
    int name_len = 0;
    char file_name[256] = {0};
    char buffer[BUF_SIZE] = {0};
    int n_cnt = 0;
    while ((recv_size != file_size) && (n_cnt = read(clnt_sock, buffer, BUF_SIZE)) > 0) {
      //FILE* fp = fopen("");
      //printf("recv msg: %s \n", buffer);
      //printf("recv size: %ld, file_size: %ld\n", recv_size, file_size);
      if (0 == recv_size) {
        file_size = 0;
        for(int i=7; i>=0; i--) {
          //printf("i:%d v:%x\n", i, (uchar)buffer[i]);
          file_size = (file_size << 8) | (uchar)buffer[i];
        }
        name_len = (buffer[9] << 8) | buffer[8];
        int offset = 10;
        printf("file size length: %ld\n", file_size);
        printf("file name length: %d\n", name_len);
        strcat(file_name, argopt->dir_name);
        strcat(file_name, "/");
        memcpy(file_name + strlen(argopt->dir_name) + 1, buffer + offset, name_len); 
        printf("receiving file: %s\n", file_name + strlen(argopt->dir_name) + 1);
        //open file
        fp = fopen(file_name, "wb");
        if (fp == NULL) {
          printf("can't open file: %s\n", file_name);
          exit(EXIT_FAILURE);
        }

        memcpy(cache + cache_cur, buffer + offset + name_len, n_cnt - offset - name_len); 
        cache_cur += n_cnt - offset - name_len;

        recv_size += cache_cur;
        continue ;
      }
      
      if (cache_cur + n_cnt > CACHE_SIZE) {
        double progress = recv_size/(file_size * 1.0)*100;
        if (((long)progress % 10) < 2) {
          printf("-> %.2f%% ", progress);
        } else {
          printf(".");
        }
        //putchar('.');
        fflush(stdout);
        flush_to_file(cache, cache_cur, fp);
        cache_cur = 0;
      } 
      
      memcpy(cache + cache_cur, buffer, n_cnt);
      recv_size += n_cnt;
      cache_cur += n_cnt;
    } //while
    //printf("recv size: %ld, file_size: %ld\n", recv_size, file_size);
    //printf(".\n");
    printf("-> %.2f%%\n", recv_size/(file_size * 1.0)*100);
    flush_to_file(cache, cache_cur, fp);
    if (0 != fclose(fp)) {
      printf("close file something wrong!");
      exit(EXIT_FAILURE);
    }

    //向客户端发送数据
    char str[256] = {0};
    strncat(str, file_name + strlen(argopt->dir_name) + 1, name_len > 200 ? 200 : name_len);
    strncat(str, " is done", 9);
    write(clnt_sock, str, sizeof(str));
    printf("recv name: %s\n", str);
    
    //关闭套接字
    close(clnt_sock);
  } //while

  close(serv_sock);

  free(argopt);
  return 0;
}
