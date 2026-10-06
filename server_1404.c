#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PORT 7404
#define NID "8614"
#define MAX_CLIENTS 64
#define MAX_NAME 64
#define MAX_ROOM 64
#define MAX_LINE 4096
#define MAX_FILE_SIZE (10UL * 1024UL * 1024UL)
#define LOG_FILE "netmsg_IT23861404.log"
#define STORAGE_ROOT "./storage/IT23861404"

typedef struct Client Client;
typedef struct Room Room;
struct Client { int fd; char username[MAX_NAME]; int registered; Client *next; };
struct Room { char name[MAX_ROOM]; Client *members[MAX_CLIENTS]; int count; Room *next; };

static Client *clients = NULL;
static Room *rooms = NULL;
static pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t running = 1;
static int listen_fd = -1;

static void log_event(const char *fmt, ...) {
    pthread_mutex_lock(&log_lock);
    FILE *f = fopen(LOG_FILE, "a");
    if (f) {
        time_t now = time(NULL); struct tm tmv; localtime_r(&now, &tmv);
        fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] ", tmv.tm_year+1900,tmv.tm_mon+1,tmv.tm_mday,tmv.tm_hour,tmv.tm_min,tmv.tm_sec);
        va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap); fputc('\n', f); fclose(f);
    }
    pthread_mutex_unlock(&log_lock);
}

static ssize_t send_all(int fd, const void *buf, size_t n) {
    size_t sent = 0; const char *p = buf;
    while (sent < n) { ssize_t r = send(fd, p+sent, n-sent, 0); if (r <= 0) return -1; sent += (size_t)r; }
    return (ssize_t)sent;
}

static int send_response(Client *c, const char *fmt, ...) {
    char body[MAX_LINE]; va_list ap; va_start(ap, fmt); vsnprintf(body,sizeof(body),fmt,ap); va_end(ap);
    char out[MAX_LINE+32]; snprintf(out,sizeof(out),"%s NID:%s\n",body,NID); return send_all(c->fd,out,strlen(out)) < 0 ? -1 : 0;
}
static int send_msg(Client *c, const char *fmt, ...) {
    char out[MAX_LINE]; va_list ap; va_start(ap,fmt); vsnprintf(out,sizeof(out),fmt,ap); va_end(ap); return send_all(c->fd,out,strlen(out)) < 0 ? -1 : 0;
}

static Client *find_client_locked(const char *name) { for(Client *c=clients;c;c=c->next) if(c->registered && strcmp(c->username,name)==0) return c; return NULL; }
static Room *find_room_locked(const char *name) { for(Room *r=rooms;r;r=r->next) if(strcmp(r->name,name)==0) return r; return NULL; }
static int room_has(Room *r, Client *c) { for(int i=0;i<r->count;i++) if(r->members[i]==c) return 1; return 0; }
static void remove_from_rooms_locked(Client *c) {
    Room *r=rooms, *prev=NULL;
    while(r){ int w=0; for(int i=0;i<r->count;i++) if(r->members[i]!=c) r->members[w++]=r->members[i]; r->count=w;
        Room *next=r->next; if(r->count==0){ if(prev) prev->next=next; else rooms=next; free(r); } else prev=r; r=next; }
}
static void broadcast_presence(const char *msg, Client *except) {
    pthread_mutex_lock(&state_lock); for(Client *c=clients;c;c=c->next) if(c->registered && c!=except) send_msg(c,"MSG BCAST SERVER %s\n",msg); pthread_mutex_unlock(&state_lock);
}
static int safe_component(const char *s){ if(!s||!*s) return 0; for(const unsigned char *p=(const unsigned char*)s;*p;p++) if(*p=='/'||*p=='\\'||(*p=='.'&&p[1]=='.')) return 0; return 1; }
static int mkdir_p(const char *path){ char tmp[1024]; snprintf(tmp,sizeof(tmp),"%s",path); for(char *p=tmp+1;*p;p++) if(*p=='/'){*p=0; mkdir(tmp,0755); *p='/';} return mkdir(tmp,0755)==-1 && errno!=EEXIST ? -1:0; }

static void handle_register(Client *c, char *arg){
    if(!arg || !*arg){ send_response(c,"ERR 001 INVALID_USERNAME"); return; }
    char name[MAX_NAME]; snprintf(name,sizeof(name),"%s",arg);
    pthread_mutex_lock(&state_lock);
    if(find_client_locked(name)){ pthread_mutex_unlock(&state_lock); send_response(c,"ERR 001 USERNAME_TAKEN"); return; }
    snprintf(c->username,sizeof(c->username),"%s",name); c->registered=1;
    pthread_mutex_unlock(&state_lock);
    send_response(c,"OK REGISTERED %s",name); log_event("REGISTER user=%s",name); char p[MAX_LINE]; snprintf(p,sizeof(p),"%s joined",name); broadcast_presence(p,c);
}

static void list_users(Client *c){ char out[MAX_LINE]="OK USERS "; pthread_mutex_lock(&state_lock); int first=1; for(Client *x=clients;x;x=x->next) if(x->registered){ if(!first) strncat(out,",",sizeof(out)-strlen(out)-1); strncat(out,x->username,sizeof(out)-strlen(out)-1); first=0; } pthread_mutex_unlock(&state_lock); send_response(c,"%s",out); }
static void list_rooms(Client *c){ char out[MAX_LINE]="OK ROOMS "; pthread_mutex_lock(&state_lock); int first=1; for(Room*r=rooms;r;r=r->next){ if(!first)strncat(out,",",sizeof(out)-strlen(out)-1); strncat(out,r->name,sizeof(out)-strlen(out)-1); first=0;} pthread_mutex_unlock(&state_lock); send_response(c,"%s",out); }

static void handle_join(Client*c,const char*name){ if(!safe_component(name)){send_response(c,"ERR 003 ROOM_NOT_FOUND");return;} pthread_mutex_lock(&state_lock); Room*r=find_room_locked(name); if(!r){r=calloc(1,sizeof(*r));snprintf(r->name,sizeof(r->name),"%s",name);r->next=rooms;rooms=r;} if(!room_has(r,c)&&r->count<MAX_CLIENTS)r->members[r->count++]=c; pthread_mutex_unlock(&state_lock); send_response(c,"OK JOINED %s",name); log_event("ROOM_JOIN user=%s room=%s",c->username,name); }
static void handle_leave(Client*c,const char*name){ pthread_mutex_lock(&state_lock); Room*r=find_room_locked(name); if(!r){pthread_mutex_unlock(&state_lock);send_response(c,"ERR 003 ROOM_NOT_FOUND");return;} for(int i=0;i<r->count;i++)if(r->members[i]==c){r->members[i]=r->members[--r->count];break;} if(r->count==0){Room **pp=&rooms;while(*pp&&*pp!=r)pp=&(*pp)->next;if(*pp){*pp=r->next;free(r);}} pthread_mutex_unlock(&state_lock); send_response(c,"OK LEFT %s",name); log_event("ROOM_LEAVE user=%s room=%s",c->username,name); }

static void handle_bcast(Client*c,const char*msg){ pthread_mutex_lock(&state_lock); for(Client*x=clients;x;x=x->next) if(x->registered&&x!=c) send_msg(x,"MSG BCAST %s %s\n",c->username,msg); pthread_mutex_unlock(&state_lock); send_response(c,"OK SENT"); log_event("BCAST from=%s text=%s",c->username,msg); }
static void handle_pmsg(Client*c,char *args){ char *sp=strchr(args,' '); if(!sp){send_response(c,"ERR 002 USER_NOT_FOUND");return;}*sp=0; char*name=args;char*msg=sp+1; pthread_mutex_lock(&state_lock);Client*t=find_client_locked(name); if(t)send_msg(t,"MSG PRIV %s %s\n",c->username,msg); pthread_mutex_unlock(&state_lock); if(!t){send_response(c,"ERR 002 USER_NOT_FOUND");return;} send_response(c,"OK SENT");log_event("PMSG from=%s to=%s text=%s",c->username,name,msg);}
static void handle_rmsg(Client*c,char*args){ char*sp=strchr(args,' ');if(!sp){send_response(c,"ERR 003 ROOM_NOT_FOUND");return;}*sp=0;char*rn=args;char*msg=sp+1;pthread_mutex_lock(&state_lock);Room*r=find_room_locked(rn);if(r)for(int i=0;i<r->count;i++)if(r->members[i]!=c)send_msg(r->members[i],"MSG ROOM %s %s %s\n",rn,c->username,msg);pthread_mutex_unlock(&state_lock);if(!r){send_response(c,"ERR 003 ROOM_NOT_FOUND");return;}send_response(c,"OK SENT");log_event("RMSG room=%s from=%s text=%s",rn,c->username,msg);}

static int recv_exact(int fd, void *buf, size_t n){ size_t got=0;char*p=buf;while(got<n){ssize_t r=recv(fd,p+got,n-got,0);if(r<=0)return -1;got+=(size_t)r;}return 0;}
static void handle_file(Client*c,char*args){
    char target[MAX_NAME],filename[256]; unsigned long long size;
    if(sscanf(args,"%63s %255s %llu",target,filename,&size)!=3 || size>MAX_FILE_SIZE || !safe_component(filename)) { send_response(c,size>MAX_FILE_SIZE?"ERR 004 FILE_TOO_LARGE":"ERR 004 INVALID_FILE"); return; }
    pthread_mutex_lock(&state_lock); Client*t=find_client_locked(target); Room*r=find_room_locked(target); pthread_mutex_unlock(&state_lock);
    if(!t&&!r){send_response(c,"ERR 002 USER_NOT_FOUND");return;}
    char dir[1024],path[1536]; snprintf(dir,sizeof(dir),"%s/%s",STORAGE_ROOT,c->username); mkdir_p(dir); snprintf(path,sizeof(path),"%s/%s",dir,filename);
    FILE*f=fopen(path,"wb"); if(!f){send_response(c,"ERR 004 FILE_SAVE_FAILED");return;}
    char buf[8192]; unsigned long long rem=size;
    while(rem){size_t want=rem<sizeof(buf)?(size_t)rem:sizeof(buf);if(recv_exact(c->fd,buf,want)<0){fclose(f);unlink(path);return;}if(fwrite(buf,1,want,f)!=want){fclose(f);unlink(path);send_response(c,"ERR 004 FILE_SAVE_FAILED");return;}rem-=want;} fclose(f);
    FILE*in=fopen(path,"rb"); if(!in){send_response(c,"ERR 004 FILE_SAVE_FAILED");return;}
    unsigned char *data=NULL; if(size){data=malloc((size_t)size);if(!data){fclose(in);send_response(c,"ERR 004 FILE_SAVE_FAILED");return;}if(fread(data,1,(size_t)size,in)!=(size_t)size){free(data);fclose(in);send_response(c,"ERR 004 FILE_SAVE_FAILED");return;}} fclose(in);
    char hdr[512]; snprintf(hdr,sizeof(hdr),"FILE %s %s %llu\n",c->username,filename,size);
    if(t && t!=c){send_all(t->fd,hdr,strlen(hdr));if(size)send_all(t->fd,data,(size_t)size);}
    if(r){pthread_mutex_lock(&state_lock);for(int i=0;i<r->count;i++){Client*x=r->members[i];if(x==c)continue;send_all(x->fd,hdr,strlen(hdr));if(size)send_all(x->fd,data,(size_t)size);}pthread_mutex_unlock(&state_lock);}
    free(data); send_response(c,"OK FILE_RECEIVED %s",filename); log_event("FILE from=%s target=%s filename=%s size=%llu",c->username,target,filename,size);
}

static int recv_line(int fd, char *line, size_t cap) {
    size_t n=0;
    while(n+1<cap) {
        char ch;
        ssize_t r=recv(fd,&ch,1,0);
        if(r<=0) return -1;
        if(ch=='\n') { line[n]='\0'; return 0; }
        if(ch=='\r') continue;
        line[n++]=ch;
    }
    line[cap-1]='\0';
    return -2;
}

static void *client_thread(void *arg){
    Client *c=arg;
    char line[MAX_LINE];
    while(running) {
        int rr=recv_line(c->fd,line,sizeof(line));
        if(rr==-1) break;
        if(rr==-2) { send_response(c,"ERR 005 MALFORMED_INPUT"); continue; }
        char *cmd=line, *args=strchr(line,' ');
        if(args) { *args++='\0'; }
        if(!c->registered && strcmp(cmd,"REGISTER")!=0) {
            send_response(c,"ERR 005 REGISTER_REQUIRED");
            continue;
        }
        if(strcmp(cmd,"REGISTER")==0) handle_register(c,args);
        else if(strcmp(cmd,"LIST")==0) list_users(c);
        else if(strcmp(cmd,"BCAST")==0) {
            if(!args||!*args) send_response(c,"ERR 005 INVALID_MESSAGE"); else handle_bcast(c,args);
        } else if(strcmp(cmd,"PMSG")==0) {
            if(!args) send_response(c,"ERR 002 USER_NOT_FOUND"); else handle_pmsg(c,args);
        } else if(strcmp(cmd,"JOIN")==0) {
            if(!args) send_response(c,"ERR 003 ROOM_NOT_FOUND"); else handle_join(c,args);
        } else if(strcmp(cmd,"LEAVE")==0) {
            if(!args) send_response(c,"ERR 003 ROOM_NOT_FOUND"); else handle_leave(c,args);
        } else if(strcmp(cmd,"ROOMS")==0) list_rooms(c);
        else if(strcmp(cmd,"RMSG")==0) {
            if(!args) send_response(c,"ERR 003 ROOM_NOT_FOUND"); else handle_rmsg(c,args);
        } else if(strcmp(cmd,"SENDFILE")==0) {
            if(!args) send_response(c,"ERR 004 INVALID_FILE"); else handle_file(c,args);
        } else if(strcmp(cmd,"QUIT")==0) {
            send_response(c,"OK BYE");
            break;
        } else send_response(c,"ERR 005 INVALID_COMMAND");
    }
    pthread_mutex_lock(&state_lock);
    int was_registered=c->registered;
    char uname[MAX_NAME]; snprintf(uname,sizeof(uname),"%s",c->username);
    if(c->registered) remove_from_rooms_locked(c);
    Client **pp=&clients; while(*pp&&*pp!=c) pp=&(*pp)->next;
    if(*pp) *pp=c->next;
    pthread_mutex_unlock(&state_lock);
    if(was_registered) {
        log_event("DISCONNECT user=%s",uname);
        char p[MAX_LINE]; snprintf(p,sizeof(p),"%s left",uname); broadcast_presence(p,NULL);
    }
    close(c->fd); free(c); return NULL;
}

static void stop_server(int sig){(void)sig;running=0;if(listen_fd>=0)close(listen_fd);}
int main(void){signal(SIGPIPE,SIG_IGN);signal(SIGINT,stop_server);mkdir_p(STORAGE_ROOT);log_event("SERVER_START port=%d nid=%s",PORT,NID);listen_fd=socket(AF_INET,SOCK_STREAM,0);if(listen_fd<0){perror("socket");return 1;}int opt=1;setsockopt(listen_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_addr.s_addr=INADDR_ANY;a.sin_port=htons(PORT);if(bind(listen_fd,(struct sockaddr*)&a,sizeof(a))<0){perror("bind");return 1;}if(listen(listen_fd,MAX_CLIENTS)<0){perror("listen");return 1;}printf("NetMessenger server listening on TCP port %d (NID:%s)\n",PORT,NID);while(running){struct sockaddr_in ca;socklen_t cl=sizeof(ca);int fd=accept(listen_fd,(struct sockaddr*)&ca,&cl);if(fd<0){if(!running)break;if(errno==EINTR)continue;perror("accept");continue;}Client*c=calloc(1,sizeof(*c));c->fd=fd;pthread_mutex_lock(&state_lock);c->next=clients;clients=c;pthread_mutex_unlock(&state_lock);char ip[INET_ADDRSTRLEN];inet_ntop(AF_INET,&ca.sin_addr,ip,sizeof(ip));log_event("CONNECT ip=%s",ip);pthread_t tid;pthread_create(&tid,NULL,client_thread,c);pthread_detach(tid);}log_event("SERVER_STOP");return 0;}
