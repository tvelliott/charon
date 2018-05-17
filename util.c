//MIT License
//
//Copyright (c) 2018 tvelliott
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE 1 
#include <netdb.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h> 
#include <stdint.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>

#include "util.h"


///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
uint32_t get_netif_ip(char *if_name, char *ip_a) {

uint32_t ip_addr;
struct ifaddrs *ifaddr;
struct ifaddrs *ifa;
struct sockaddr_in *sa;
int family;
int s;
char host[1024];
struct in_addr addr;

  if (getifaddrs(&ifaddr) == -1) {
    perror("getifaddrs");
    return 0xffffffff;
  }


  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) continue;  

    s=getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
      if (ifa==NULL) {
        return 0xffffffff;
      }

    if((strcmp(ifa->ifa_name,if_name)==0)&&(ifa->ifa_addr->sa_family==AF_INET)) {
      //printf("\tInterface : <%s>\n",ifa->ifa_name );
      //printf("\t  Address : <%s>\n", host); 
      strncpy(ip_a, host, sizeof(host)-1);
      inet_aton(host, &addr);

      //sa = (struct sockaddr_in *) ifa->ifa_addr;
      //if(sa!=NULL) {
      //  memcpy( &ip_addr, &sa->sin_addr, 4);
      //  freeifaddrs(ifaddr);
        //return htonl(ip_addr);
        return (uint32_t) addr.s_addr; 
      //}
    }
  }

  freeifaddrs(ifaddr);

  return 0xffffffff; 
}

////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
int read_fw_attr_value(const char*config_key, char *value, int max_len){

FILE * fp;
char line[1024];
char *ptr;
char *tok1;

  memset(line,0x00,sizeof(line));
  if((fp = popen("/usr/sbin/fw_printenv","r")) != NULL){
    while(fgets(line,sizeof(line),fp)!=NULL){
      if(line[0]=='\n' || line[0]=='\r' || line[0]==' ') {
        memset(line,0x00,sizeof(line));
        continue;
      }

      if( strncmp(config_key, &line[0], strlen(config_key))==0 ) {
        ptr = &line[strlen(config_key)];
        tok1 = strtok(ptr, " =\r\n");
        memcpy(value, tok1, max_len-1);

        fprintf(stderr, "\nfound %s key:  value:->%s<-", config_key, value);

        pclose(fp);
        return 0;
      }
      memset(line,0x00,sizeof(line));
    }
    goto clean; 
  } else {                    
    perror("read_config:fopen:");
    goto clean; 
  }

clean:
if(fp!=NULL) pclose(fp);
  return -1;
}
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
int is_batman_route(const char*org_dest_mac_a, uint8_t *dst_route){

FILE * fp;
char line[1024];
char *ptr;
char *tok1;
char *tok2;
char *tok3;
char *tok4;
char *tok5;
char *tok6;
char *tok7;
char *tok8;

  memset(line,0x00,sizeof(line));
  if((fp = fopen("/sys/kernel/debug/batman_adv/bat0/originators","r")) != NULL){
    if(fp==NULL) return 0;

    while(fgets(line,sizeof(line),fp)!=NULL){
      if(line[0]==' ') {
        memset(line,0x00,sizeof(line));
        continue;
      }

      if( strncmp(org_dest_mac_a, &line[0], strlen(org_dest_mac_a))==0 ) {
        ptr = &line[strlen(org_dest_mac_a)];

        tok1 = strtok(ptr, " ");
        if(tok1==NULL) goto clean;
          //fprintf(stderr, "\ntok1: %s", tok1);

        tok2 = strtok(NULL, ")");
        if(tok2==NULL) goto clean;

        tok3 = strtok(NULL, " "); //this is the next hop for this originator
        if(tok3==NULL) goto clean;

        tok4 = strtok(NULL, " "); 
        if(tok4==NULL) goto clean;
        tok5 = strtok(NULL, " "); 
        if(tok5==NULL) goto clean;
        tok6 = strtok(NULL, " "); 
        if(tok6==NULL) goto clean;  //next potential hop

          if( strlen(tok3) < 17 ) goto clean;

          fprintf(stderr, "\nnext hop-> %s", tok3);
          tok3[2] = 0;
          tok3[5] = 0;
          tok3[8] = 0;
          tok3[11] = 0;
          tok3[14] = 0;
          dst_route[0] = strtol(&tok3[0],NULL,16);
          dst_route[1] = strtol(&tok3[3],NULL,16);
          dst_route[2] = strtol(&tok3[6],NULL,16);
          dst_route[3] = strtol(&tok3[9],NULL,16);
          dst_route[4] = strtol(&tok3[12],NULL,16);
          dst_route[5] = strtol(&tok3[15],NULL,16);

        fclose(fp);
        return 1;
      }
      memset(line,0x00,sizeof(line));
    }
    goto clean; 
  } else {                    
    perror("read_config:fopen:");
    goto clean; 
  }

clean:
if(fp!=NULL) fclose(fp);
  return 0;
}
