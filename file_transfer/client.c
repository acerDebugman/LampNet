#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>

#define BUF_SIZE 1024

#define uchar unsigned char

typedef struct {
  bool is_abs;
  int len;
  char** arr;
} Path;

Path parse_path(char* path); 
void free_path(Path path);
char* get_file_name(Path* path);

typedef struct {
  int port;
  char* file_name;
  char* real_path;
  Path path;
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
    if (strcmp(argv[i],"-f") == 0) {
      argopt->real_path = argv[++i];
      Path path = parse_path(argv[i]);
      argopt->file_name = get_file_name(&path);
      //printf("file name:%s\n", argopt->file_name);
      argopt->path = path;
      //free_path(path);
    }
  }
  if (argopt->file_name == NULL || argopt->port == 0) {
    return -2;
  }

  return 0;
}

Path parse_path(char* path) {
  int len = 1;
  char* p = path;
  bool is_abs = (*p == '/' ? true : false);
  if (is_abs) {
    p+=1;
  }
  while (*p != '\0') {
    if (*p == '/') {
      len += 1;
    }
    p+=1;
  }
  //printf("xxx path:%s len:%d\n", path, len);
  char** arr = (char**)malloc(len * sizeof(char *)); 
  Path ret = {
    .is_abs = is_abs,
    .len = len,
    .arr = arr,
  };

  int cnt = 0;
  char tmp[1024] = {0};
  int cur = 0;
  p = is_abs ? path + 1 : path;
  for (; *p != '\0'; p++) {
    if ('/' == *p) {
      *(ret.arr + cnt) = (char*)malloc((cur+1) * sizeof(char));
      memset(*(ret.arr+cnt), 0, (cur+1)*sizeof(char));
      strcpy(*(ret.arr+cnt), tmp); 
       
      memset(tmp, 0, sizeof(tmp));
      cur = 0;     
      cnt += 1;
      continue ;
    }
    tmp[cur++] = *p;
  }
  *(ret.arr + cnt) = (char*)malloc((cur+1) * sizeof(char));
  memset(*(ret.arr+cnt), 0, (cur+1)*sizeof(char));
  strcpy(*(ret.arr+cnt), tmp); 

  return ret;
}

void free_path(Path path) {
  for(int i = 0; i < path.len; i++) {
    free(path.arr[i]);
  }
  free(path.arr);
}

char* get_file_name(Path* path) {
  return path->arr[path->len - 1];
}


int main(int argc, char** argv){
  ArgOption* argopt = (ArgOption*)malloc(sizeof(ArgOption));
  if (NULL == argopt) {
    printf("malloc error\n");
    return EXIT_FAILURE;
  }
  int ret = parse_arg(argc, argv, argopt);
  if (ret == -1) {
    printf("no argument, usage: -p ${port} -f ${data_cache}\nrequired: -p, -f\n");
    exit(1);
  }
  if (ret == -2) {
    printf("argument error, usage: -p ${port} -f ${data_cache}\nrequired: -p, -f\n");
    exit(1);
  }

  //创建套接字
  int sock = socket(AF_INET, SOCK_STREAM, 0);

  //向服务器（特定的IP和端口）发起请求
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));  //每个字节都用0填充
  serv_addr.sin_family = AF_INET;  //使用IPv4地址
  serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  //具体的IP地址
  serv_addr.sin_port = htons(argopt->port);  //端口
  int conn_rs = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
  if (conn_rs == -1) {
    printf("connect to server error\n");
    exit(EXIT_FAILURE);
  }
 
  //获取文件内容发送给服务器
  printf("send file: %s\n", argopt->file_name);
  FILE* fp = fopen(argopt->real_path, "rb");
  if (fp == NULL) {
    printf("Can't open file!\n");
    exit(EXIT_FAILURE);
  }
  //printf("file num DP: %d\n", fp->_fileno);
  struct stat file_stat;
  fstat(fp->_fileno, &file_stat);
  if (file_stat.st_size > 0) {
    printf("文件大小是: %ld 字节\n", file_stat.st_size);
  } else {
    printf("无法获取文件大小信息\n");
  }
  long file_size = file_stat.st_size;
  printf("file size: %ld\n", file_size);

  int name_len = strlen(argopt->file_name); 
  if (name_len > BUF_SIZE) {
    printf("file name exceed length: %d\n", BUF_SIZE);
    exit(-1);
  }

  char buf_send[BUF_SIZE] = {0};
  for (int i = 0; i < 8; i++) {
    buf_send[i] = (file_size >> i*8) & 0xFF;
    //printf("i: %d, v: %0x \n", i, (uchar)buf_send[i]);
  }

  buf_send[8] = name_len & 0xFF;
  buf_send[9] = (name_len >> 8);
  //printf("i: %d, v: %0x \n", 8, (uchar)buf_send[8]);
  //printf("i: %d, v: %0x \n", 9, (uchar)buf_send[9]);
  int start_offset = 10;
  strcat(buf_send + start_offset, argopt->file_name);
  
  long send_size = 0;
  int r_cnt = 0, w_len = 0;
  do {
    send_size += w_len;
    if (0 == send_size) {
      r_cnt = fread(buf_send + start_offset + name_len, 1, BUF_SIZE - start_offset - name_len, fp);
      r_cnt += start_offset + name_len;
    } else {
      r_cnt = fread(buf_send, 1, BUF_SIZE, fp);
    }
    //if (-1 == r_cnt) {
    if (r_cnt <= 0) {
      printf("File reading is complete\n");
      break;
    }
  } while ((w_len = write(sock, buf_send, r_cnt)) > 0);

  printf("total send bytes: %ld\n", send_size);
  //printf("r_cnt is: %d, w_len is: %d\n", r_cnt, w_len);

  if (feof(fp)) {
      printf("文件已读取完毕\n");
  } else {
      perror("文件读取错误");
  }


  printf("read from server:\n");
  //读取服务器传回的数据
  char buffer[256] = {0};
  int cnt = read(sock, buffer, sizeof(buffer)-1);
  printf("%d bytes Message form server: %s\n", cnt, buffer);
 
  //关闭套接字
  close(sock);

  return 0;
}
