#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 7404
#define MAX_LINE 4096
#define MAX_FILE 10UL*1024UL*1024UL
static int sockfd; static volatile int running=1;
static ssize_t send_all(const void*b,size_t n){size_t s=0;while(s<n){ssize_t r=send(sockfd,(char*)b+s,n-s,0);if(r<=0)return -1;s+=r;}return s;}
static int recv_exact(void*b,size_t n){size_t g=0;while(g<n){ssize_t r=recv(sockfd,(char*)b+g,n-g,0);if(r<=0)return -1;g+=r;}return 0;}
static int recv_line(int fd,char *line,size_t cap){size_t n=0;while(n+1<cap){char ch;ssize_t r=recv(fd,&ch,1,0);if(r<=0)return -1;if(ch=='\n'){line[n]='\0';return 0;}if(ch=='\r')continue;line[n++]=ch;}line[cap-1]='\0';return -2;}
static void *receiver(void*unused){(void)unused;char line[MAX_LINE];while(running){int rr=recv_line(sockfd,line,sizeof(line));if(rr<0){running=0;break;}if(strncmp(line,"FILE ",5)==0){char sender[64],fn[256];unsigned long long size;if(sscanf(line+5,"%63s %255s %llu",sender,fn,&size)==3&&size<=MAX_FILE){char out[512];snprintf(out,sizeof(out),"received_%s",fn);FILE*f=fopen(out,"wb");if(!f){fprintf(stderr,"Cannot save %s\\n",out);char discard[8192];unsigned long long rem=size;while(rem){size_t w=rem<sizeof(discard)?(size_t)rem:sizeof(discard);if(recv_exact(discard,w)<0){running=0;break;}rem-=w;}}else{char b[8192];unsigned long long rem=size;while(rem){size_t w=rem<sizeof(b)?(size_t)rem:sizeof(b);if(recv_exact(b,w)<0){fclose(f);remove(out);running=0;break;}if(fwrite(b,1,w,f)!=w){fclose(f);remove(out);running=0;break;}rem-=w;}if(running){fclose(f);printf("\\n[FILE] %s sent %s (%llu bytes), saved as %s\\n> ",sender,fn,size,out);fflush(stdout);}}}}else{printf("\\n%s\\n> ",line);fflush(stdout);}}return NULL;}
static void usage(void){printf("Commands:\nREGISTER <username>\nLIST\nBCAST <message>\nPMSG <username> <message>\nJOIN <room>\nLEAVE <room>\nROOMS\nRMSG <room> <message>\nSENDFILE <target> <filename> <filesize>\nQUIT\n");}
int main(int argc,char**argv){if(argc<3){fprintf(stderr,"Usage: %s <server-ip> <username>\n",argv[0]);return 1;}sockfd=socket(AF_INET,SOCK_STREAM,0);if(sockfd<0){perror("socket");return 1;}struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_port=htons(PORT);if(inet_pton(AF_INET,argv[1],&a.sin_addr)<=0){fprintf(stderr,"Invalid IP\n");return 1;}if(connect(sockfd,(struct sockaddr*)&a,sizeof(a))<0){perror("connect");return 1;}char reg[MAX_LINE];snprintf(reg,sizeof(reg),"REGISTER %s\n",argv[2]);send_all(reg,strlen(reg));pthread_t t;pthread_create(&t,NULL,receiver,NULL);usage();char input[MAX_LINE];while(running){printf("> ");fflush(stdout);if(!fgets(input,sizeof(input),stdin))break;if(strncmp(input,"SENDFILE ",9)==0){char target[64],fn[256];unsigned long long size;if(sscanf(input+9,"%63s %255s %llu",target,fn,&size)!=3){printf("Usage: SENDFILE <target> <filename> <filesize>\n");continue;}FILE*f=fopen(fn,"rb");if(!f){perror("file");continue;}char header[MAX_LINE];snprintf(header,sizeof(header),"SENDFILE %s %s %llu\n",target,fn,size);if(send_all(header,strlen(header))<0){fclose(f);break;}char b[8192];unsigned long long rem=size;while(rem){size_t w=rem<sizeof(b)?(size_t)rem:sizeof(b);size_t r=fread(b,1,w,f);if(r!=w){fprintf(stderr,"File shorter than declared size\n");break;}if(send_all(b,r)<0){rem=0;break;}rem-=r;}fclose(f);if(rem!=0)printf("File transfer failed\n");}else{if(send_all(input,strlen(input))<0)break;if(strncmp(input,"QUIT",4)==0)break;}}running=0;shutdown(sockfd,SHUT_RDWR);close(sockfd);pthread_join(t,NULL);return 0;}
