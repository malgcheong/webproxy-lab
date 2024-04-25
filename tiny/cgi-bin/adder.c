/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char *buf;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  /* Extract the two arguments */
  if ((buf = getenv("QUERY_STRING")) != NULL) {
    // 'a=' 문자열을 찾습니다.
    char *ptr_a = strstr(buf, "a=");
    if (ptr_a != NULL) {
        // 'a=' 다음 문자열부터 시작하여 '&' 문자가 나타나는 위치를 찾습니다.
        char *ptr_amp = strchr(ptr_a, '&');
        if (ptr_amp != NULL) {
            // 'a=' 다음 문자열부터 '&' 문자 직전까지를 추출합니다.
            char value[10];
            strncpy(value, ptr_a + 2, ptr_amp - (ptr_a + 2));
            value[ptr_amp - (ptr_a + 2)] = '\0';
            strcpy(arg1, value);
            n1 = atoi(arg1);
        }
    }

    char *ptr_b = strstr(buf, "b=");
    if (ptr_b != NULL){ 
      // 'b=' 다음 문자열부터 시작하여 널 종료 문자까지를 추출합니다.
      printf("Value of 'b': %s\n", ptr_b + 2);
      strcpy(arg2, ptr_b + 2);
      n2 = atoi(arg2);
    }
  }



  /* Make the response body */
  sprintf(content, "QUERY_STRING=%s", buf);
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n"); /* 빈 줄 한 개가 헤더를 종료하고 있다. */
  if(strcasecmp(getenv("REQUEST_METHOD"),"HEAD") != 0){ /* HEAD method는 content를 제외하고 header만 출력 */
    printf("%s", content);
    fflush(stdout);
  }

  exit(0);
}
/* $end adder */