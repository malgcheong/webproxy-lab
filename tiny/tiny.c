/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

/*
* main: 웹 서버의 진입점으로, 클라이언트의 연결을 받아들이고 요청을 처리합니다.
*/
int main(int argc, char **argv) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); /* Port를 소켓에 바인딩하고 소켓을 대기 상태로 설정하고 클라이언트의 연결을 기다리는 소켓 디스크립터를 반환 
                                        반환된 소켓 디스크립터는 호출된 프로세스의 파일 디스크립터 테이블에서 사용 가능한 가장 작은 숫자를 할당 
                                        일반적으로 첫 번째로 열리는 소켓의 경우 3번 소켓 디스크립터를 할당 받음
                                      */
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  /* line:netp:tiny:accept, listenfd 소켓에서 연결을 기다리다가 실제로 클라이언트와 연결이 이루어지면 새로운 소켓을 생성하고 그 소켓을 반환 */
    printf(" \n\n\nfd: %d\n\n\n ", connfd);
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); /* 클라이언트의 주소를 호스트 이름과 포트 번호로 변환 */
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   /* line:netp:tiny:doit, 클라이언트의 요청을 처리 내지 클라이언트와 연결을 처리하는 함수 호출 */
    Close(connfd);  /* line:netp:tiny:close, 연결 처리가 완료되면 연결된 소켓을 닫아 해당 클라이언트와의 통신이 종료 */
  }
}

/*
* doit: 클라이언트와의 연결에서 요청을 처리합니다. 정적인 컨텐츠에 대한 요청이면 해당 파일을 제공하고, 동적인 컨텐츠에 대한 요청이면 해당 CGI 프로그램을 실행합니다.
*/
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version); /* Request Header 첫 줄은 method, uri, version이 들어가며, sscanf는 이를 공백을 기준으로 구분하여 읽음(GET / HTTP/1.1)*/
  if (strcasecmp(method, "GET")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio); /* Requset Header 단순 출력 */

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);

  if (stat(filename, &sbuf) < 0) {
      clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
      return;
  }
  
  if (is_static) {  /* Serve static content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { /* S_ISREG는 파일의 모드를 검색해 하당 파일이 일반 파일인지 여부를 확인하는 매크로, S_IRUSR & sbuf.st_mode는 파일 소유자(user)에 대한 읽기 권한이 있는지 확인한다. */
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size); /* response static file,  */
  }
  else {  /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { /* S_ISREG는 파일의 모드를 검색해 하당 파일이 일반 파일인지 여부를 확인하는 매크로, S_IRUSR & sbuf.st_mode는 파일 소유자(user)에 대한 실행 권한이 있는지 확인한다. */
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs); /* response dynamic files, cgiargs는 parse_uri 함수에서 cgi 인자로 세팅함 */
  }
}

/*
* clienterror: 클라이언트에게 오류 응답을 보냅니다.
*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/*
* read_requesthdrs: 요청 헤더를 읽어들입니다.
*/
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/*
* parse_uri: URI를 구문 분석하여 요청한 파일의 경로와 CGI 인자를 결정합니다.
*/
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin")) {  /* Static content */
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/') {
      strcat(filename, "home.html");
    }
    return 1;
  }
  else {  /* Dynamic content */
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    }
    else {
      strcpy(cgiargs, "");
    }
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

/*
* serve_static: 정적인 파일을 클라이언트에게 제공합니다.
*/
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf)); /*  클라이언트와 연결된 소켓인 fd를 통해 buf에 저장된 내용을 클라이언트에게 보내는 역할을 합니다. Rio_writen 함수는 데이터를 버퍼링 없이 소켓으로 직접 보내기 때문에 클라이언트는 이를 즉시 받게 됩니다. */
  printf("Response headers:\n");
  printf("%s", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0); /* O_RDONLY 플래그를 사용하여 파일 디스크립터를 열면 해당 파일은 읽기 전용으로 열리며, 쓰기 작업은 허용되지 않습니다. */
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); /* 읽기 옵션으로 filename을 읽어서 메모리에 맵핑하고 그 시작 포인터 srcp를 반환합니다. PROT_READ가 읽기옵션이고 MAP_PRIVATE는 파일의 변경 사항이 메모리 매핑에 반영되지 않습니다.(복사본 만들때 유용) MAP_SHARED는 한 프로세스에서 파일의 데이터를 변경하면 해당 변경 사항이 다른 프로세스에서도 반영됩니다. */
  close(srcfd);
  Rio_writen(fd, srcp, filesize); /*  srcp 포인터가 가리키는 메모리 영역에 있는 데이터를 파일 디스크립터 fd로 지정된 소켓에 쓰게 됩니다. 이 경우 srcp는 메모리 매핑된 파일의 시작 지점을 가리키며, filesize 만큼의 데이터를 쓰게 됩니다. */
  Munmap(srcp, filesize); /* 메모리 영역을 해제하고 해당 메모리 영역을 다시 사용할 수 있도록 함. srcp가 가리키는 메모리 영역을 filesize만큼 해제합니다. */
}

/* 
* get_filetype: 파일의 확장자를 통해 파일의 MIME 타입을 결정합니다.
*/
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}

/*
* serve_dynamic: 동적인 콘텐츠를 제공하기 위해 CGI 프로그램을 실행합니다.
*/
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { /* Child, 현재 프로세스를 복제하여 자식 프로세스를 만들고, 그 자식 프로세스 내에서 실행되는 코드 블록이다. 이 코드 블록은 부모 프로세스와는 독립적으로 실행된다.  */
    setenv("QUERY_STRING", cgiargs, 1); /* Real server would set all CGI vars here */
    Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client, 파일 디스크립터를 복제하는데 사용되는 시스템 호출이다. 여기서 fd는 파일 디스크립터이고, STDOUT_FILENO는 표준 출력의 파일 디스크립터를 나타냅니다. 파일 디스크립터 fd를 표준 출력에 복제합니다. 즉, 파일 디스크립터 fd가 현재 가리키는 파일과 표준 출력이 동일한 파일을 가리키게 됩니다. 이렇게 함으로써, 이후에 프로그램에서 표준 출력을 사용할 때 실제로는 파일 디스크립터 fd가 가리키는 파일로 출력됩니다. 일반적으로 이 시스템 호출은 프로세스 간 통신이나 입출력 리디렉션에 사용됩니다. 특히, 이 코드에서는 자식 프로세스의 표준 출력을 클라이언트 소켓으로 리디렉션합니다. 이렇게 하면 CGI 프로그램이 생성하는 출력이 클라이언트로 전송됩니다.*/
    Execve(filename, emptylist, environ); /* Run CGI program, Execve 함수를 호출하여 지정된 CGI 프로그램을 실행합니다. 이 함수는 새로운 프로그램을 현재 프로세스로 교체하여 실행합니다. 그리고 새로운 프로그램의 경로와 인자 배열, 환경 변수 배열을 새로운 프로그램이 선택적으로 받을 수 있습니다. */
  }
  Wait(NULL); /* Parent waits for and reaps child */
}

