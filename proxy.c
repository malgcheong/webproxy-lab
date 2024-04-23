#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3";

typedef struct cache_object_t
{
  char path[MAXLINE];
  int content_length;
  char *content;
  struct cache_object_t *prev, *next;
}cache_object_t;


void DupHeader(int connfd, char* buf, char *method, char *uri, char *version, char *host, char *port, char *path);
void DeliverServerResponseToClient(int serverfd, int connfd, char* buf, char *method, char *uri, char *version, char *host, char *port, char *path);
void parse_uri(char *uri, char *host, char *port, char *path);
void* thread(void *vargp);
cache_object_t *find_cache(char *path);
void send_cache(cache_object_t *cache_object, int clientfd);
void read_cache(cache_object_t *cache_object);
void save_cache(cache_object_t *cache_object);
cache_object_t *rootp;
cache_object_t *lastp;
int total_cache_size;


int main(int argc,char **argv) {

  int listenfd,
      *connfdp;

  pthread_t tid;

  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); 

  while (1) {
    clientlen = sizeof(clientaddr);
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Pthread_create(&tid, NULL, thread, connfdp);     
  }
  return 0;
}

void* thread(void *vargp)
{
  int connfd,     // client의 요청을 accept한 연결 소켓
      serverfd;   // server(tiny)와 통신하기 위한 연결 소켓

  char buf[MAXLINE],        // 임시 저장 공간
       method[MAXLINE],     // client(browser)가 요청한 method
       uri[MAXLINE],        // client(browser)가 요청한 uri
       version[MAXLINE],    // client(browser)가 요청한 http version
       host[MAXLINE],       // server(tiny)의 host
       port[MAXLINE],       // server(tiny)의 port
       path[MAXLINE];       // server(tiny)의 파일 경로

  connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  
  DupHeader(connfd, buf, method, uri, version, host, port, path); /* 1. 클라이언트에서 받은 request header를 복사한다 */
  DeliverServerResponseToClient(serverfd, connfd, buf, method, uri, version, host, port, path); /* 2. 복사한 request header 정보를 가지고 서버한테 요청을 하고 받은 response data를 클라이언트한테 전달함 */
  
  Close(connfd);
  return NULL;
}

void DupHeader(int connfd, char* buf, char *method, char *uri, char *version, char *host, char *port, char *path)
{
  rio_t rio;
  Rio_readinitb(&rio, connfd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("==========Request Header from client==========\n");
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

void DeliverServerResponseToClient(int serverfd, int connfd, char* buf, char *method, char *uri, char *version, char *host, char *port, char *path)
{
  // 1️⃣ cache find
  cache_object_t *cache_object = find_cache(path);

  // 2️⃣ path가 있다면 cache를 send후 해당 cache를 read해서 최신으로 변경
  if (cache_object != NULL)
  {
    send_cache(cache_object, connfd);
    read_cache(cache_object);
    return;
  }
  // 3️⃣ path가 없다면 server에 요청 후 cache를 save함
  rio_t rio;
  serverfd = Open_clientfd(host, port);
  Rio_readinitb(&rio, serverfd);
  
  // HTTP 요청 헤더 추가
  sprintf(buf, "%s ", method); 
  sprintf(buf, "%s%s ", buf, path); 
  sprintf(buf, "%s%s\r\n", buf, version); 
  sprintf(buf, "%sHost: %s\r\n", buf, host);
  sprintf(buf, "%sUser-Agent: %s\r\n", buf, user_agent_hdr);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sProxy-Connection: close\r\n", buf);
  sprintf(buf, "%s\r\n", buf);
  printf("==========Request Header to server==========\n");
  // 서버한테 HTTP 요청 전송
  printf("%s\n", buf);  
  Rio_writen(serverfd, buf, strlen(buf));


  printf("==========Response Header from server==========\n");
  // 클라이언트한테 HTTP Response Header 요청 전송
  int content_length;
  char *content;
  Rio_readlineb(&rio, buf, MAXLINE);
  while (strcmp(buf, "\r\n"))
  {
    printf("%s", buf);
    Rio_writen(connfd, buf, strlen(buf));
    if (strstr(buf, "Content-length")) 
      content_length = atoi(strchr(buf, ':') + 1);
    Rio_readlineb(&rio, buf, MAXLINE);
  }
  printf("%s",buf);
  Rio_writen(connfd, buf, strlen(buf));

  printf("==========Response Body from server==========\n");
  // 클라이언트한테 HTTP Response Body 요청 전송
  content = malloc(content_length);
  Rio_readnb(&rio, content, content_length);
  Rio_writen(connfd, content, content_length);
  printf("%s\n", content);



  if (content_length <= MAX_OBJECT_SIZE) // content가 캐싱 가능한 크기인 경우
  {
    cache_object_t *cache_object = (cache_object_t *)calloc(1, sizeof(cache_object_t));
    cache_object->content = content;
    cache_object->content_length = content_length;
    strcpy(cache_object->path, path);
    save_cache(cache_object); // 캐시 연결 리스트에 추가
  }
  else
    free(content); // 캐싱하지 않은 경우만 메모리 반환

  Close(serverfd);
}

cache_object_t *find_cache(char *path)
{
  if (!rootp) // 캐시가 비었으면
    return NULL;
  cache_object_t *current = rootp;      // 검사를 시작할 노드
  while (strcmp(current->path, path)) // 현재 검사 중인 노드의 path가 찾는 path와 다르면 반복
  {
    if (!current->next) // 현재 검사 중인 노드의 다음 노드가 없으면 NULL 반환
      return NULL;

    current = current->next;          // 다음 노드로 이동
    if (!strcmp(current->path, path)) // path가 같은 노드를 찾았다면 해당 객체 반환
      return current;
  }
  return current;
}

void send_cache(cache_object_t *cache_object, int clientfd)
{
  // 1️⃣ Response Header 생성 및 전송
  char buf[MAXLINE];
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, cache_object->content_length);
  sprintf(buf, "%sContent-type: text/html\r\n\r\n", buf);
  Rio_writen(clientfd, buf, strlen(buf));

  // 2️⃣ 캐싱된 Response Body 전송
  Rio_writen(clientfd, cache_object->content, cache_object->content_length);
}

void read_cache(cache_object_t *cache_object)
{
  if (cache_object == rootp) // 현재 노드가 이미 root면 변경 없이 종료
    return;

  // 1️⃣ 현재 노드와 이전 & 다음 노드의 연결 끊기
  if (cache_object->next) // '이전 & 다음 노드'가 모두 있는 경우
  {
    // 이전 노드와 다음 노드를 이어줌
    cache_object_t *prev_objtect = cache_object->prev;
    cache_object_t *next_objtect = cache_object->next;
    if (prev_objtect)
      cache_object->prev->next = next_objtect;
    cache_object->next->prev = prev_objtect;
  }
  else // '다음 노드'가 없는 경우 (현재 노드가 마지막 노드인 경우)
  {
    cache_object->prev->next = NULL; // 이전 노드와 현재 노드의 연결을 끊어줌
  }

  // 2️⃣ 현재 노드를 root로 변경
  cache_object->next = rootp; // root였던 노드는 현재 노드의 다음 노드가 됨
  rootp = cache_object;
}

void save_cache(cache_object_t *cache_object)
{
  // total_cache_size에 현재 객체의 크기 추가
  total_cache_size += cache_object->content_length;

  // 최대 총 캐시 크기를 초과한 경우 -> 사용한지 가장 오래된 객체부터 제거
  while (total_cache_size > MAX_CACHE_SIZE)
  {
    total_cache_size -= lastp->content_length;
    lastp = lastp->prev; // 마지막 노드를 마지막의 이전 노드로 변경
    free(lastp->next);   // 제거한 노드의 메모리 반환
    lastp->next = NULL;
  }

  if (!rootp) // 캐시 연결리스트가 빈 경우 lastp를 현재 객체로 지정
    lastp = cache_object;

  // 현재 객체를 루트로 지정
  if (rootp)
  {
    cache_object->next = rootp;
    rootp->prev = cache_object;
  }
  rootp = cache_object;
}
