#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void DupHeader(int connfd, char* buf, char *method, char *uri, char *version, char *host, char *port, char *path);
void SendToServer(int clientfd, int connfd, char* buf, char *method, char *uri, char *version, char *host, char *port, char *path);
void parse_uri(char *uri, char *host, char *port, char *path);

int main(int argc,char **argv) {

  int listenfd,   // lient의 요청을 accept하기 위한 listen 소켓
      connfd,     // client의 요청을 accept한 연결 소켓
      clientfd;   // server(tiny)와 통신하기 위한 연결 소켓

  char buf[MAXLINE],        // 임시 저장 공간
       method[MAXLINE],     // client(browser)가 요청한 method
       uri[MAXLINE],        // client(browser)가 요청한 uri
       version[MAXLINE],    // client(browser)가 요청한 http version
       host[MAXLINE],       // server(tiny)의 host
       port[MAXLINE],       // server(tiny)의 port
       path[MAXLINE];       // server(tiny)의 파일 경로

  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); 

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    /* 1. 클라이언트에서 받은 request header를 복사한다 */
    DupHeader(connfd, buf, method, uri, version, host, port, path);
    /* 2. 복사한 request header 정보를 가지고 서버한테 요청을 함 */
    /* 3. 서버한테 받은 response data를 클라이언트에게 응답한다. */
    SendToServer(clientfd, connfd, buf, method, uri, version, host, port, path);
                                                          
    Close(connfd);       
  }
}

void DupHeader(int connfd, char* buf, char *method, char *uri, char *version, char *host, char *port, char *path)
{
  rio_t rio;
  Rio_readinitb(&rio, connfd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("%s", buf);
  
  /* 1. 클라이언트에서 받은 request header를 복사한다 */
  sscanf(buf, "%s %s %s", method, uri, version);
  parse_uri(uri, host, port, path);
  printf("method: %s\n", method);
  printf("uri: %s\n", uri);
  printf("version: %s\n", version);
  printf("host: %s\n", host);
  printf("port: %s\n", port);
  printf("path: %s\n", path);
}

void parse_uri(char *uri, char *host, char *port, char *path)
{
    char *httpStart = strstr(uri, "http://");

    /* URI에 "http://"이 포함되어 있는 경우 해당 부분 삭제 */
    if (httpStart != NULL) {
        uri = httpStart + strlen("http://");
    }
    char *portStart = strchr(uri, ':');
    char *pathStart = strchr(uri, '/');

    if (portStart && pathStart) { /* port랑 path 다 있는 경우 */
      strncpy(host, uri, portStart - uri); // ":" 이전까지가 host
      strncpy(port, portStart + 1, pathStart - portStart - 1); // ":"과 "/" 사이가 port
      strcpy(path, pathStart); // "/" 이후가 path
    } else if (portStart == NULL && pathStart != NULL) { /* port가 없는 경우, URI 자체가 호스트이고 port는 80 */
      strncpy(host, uri, pathStart - uri); // host
      strcpy(port, "80"); // port
      strcpy(path, pathStart); // path
    } else if (portStart != NULL && pathStart == NULL) { /* port만 있고 path는 없는 경우 */
      strncpy(host, uri, portStart - uri); // host
      strcpy(port, portStart + 1); // port
      strcpy(path, "/"); // path
    } else { /* port랑 path 둘다 없는 경우 */
      strcpy(host, uri); // host
      strcpy(port, "80"); // port
      strcpy(path, "/"); // path
    }
}

void SendToServer(int clientfd, int connfd, char* buf, char *method, char *uri, char *version, char *host, char *port, char *path)
{
  rio_t rio;
  clientfd = Open_clientfd(host, port);
  Rio_readinitb(&rio, clientfd);

  // HTTP 요청 헤더 추가
  sprintf(buf, "%s ", method); 
  sprintf(buf, "%s%s ", buf, path); 
  sprintf(buf, "%s%s\r\n", buf, version); 
  sprintf(buf, "%sHost: %s\r\n", buf, host);
  sprintf(buf, "%sUser-Agent: %s\r\n", buf, user_agent_hdr);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sProxy-Connection: close\r\n", buf);
  sprintf(buf, "%s\r\n", buf);
  printf("%s\n", buf);
  
  /* 2. 복사한 request header 정보를 가지고 서버한테 요청을 함 */
  // HTTP 요청 전송
  Rio_writen(clientfd, buf, strlen(buf));

  ssize_t bytesRead;
  while ((bytesRead = Rio_readlineb(&rio, buf, MAXLINE)) > 0) {
    Rio_writen(connfd, buf, bytesRead); /* 3. 서버한테 받은 response data를 클라이언트에게 응답한다. */
  }
  
  Close(clientfd);
}

