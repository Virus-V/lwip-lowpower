#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "lwip/tcpip.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dhcp.h"

#include "tapif.h"

#include "tcpecho/tcpecho.h"
#include "udpecho/udpecho.h"
#include "chargen/chargen.h"
#include "httpserver/httpserver-netconn.h"

#include "iperf/iperf.h"
#include "ping/ping.h"

/* exported in lwipopts.h */
unsigned char debug_flags = LWIP_DBG_ON;

#define NETIF_MAX 64

struct tapif {
  struct eth_addr *ethaddr;
  /* Add whatever per-interface state that is needed here. */
  int fd;
  char *name;
  ip_addr_t ip_addr;
  ip_addr_t netmask;
  ip_addr_t gw;
};

int parse_address(char* addr,struct addrinfo* info,int family) {
  struct addrinfo hints;
  struct addrinfo *result;
  struct addrinfo *r;
  int res;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = 0;
  hints.ai_protocol = 0;
  hints.ai_flags = AI_NUMERICSERV;
  res = getaddrinfo(addr,0,&hints,&result);
  if (res != 0) {
    fprintf(stderr, "getaddrinfo: %d\n", res);
    return -1;
  }
  for (r = result; r != NULL; r = r->ai_next) {
    if (r->ai_family == family) {
      *info = *r;
      return 0;
    }
  }
  return -1;
}

int
parse_pair(struct tapif* tapif,char* key,char* value)
{
  struct addrinfo addr;
  uint8_t* p;

  if (key == 0 || *key == 0)
    return -1;

#define PARSE_IP4(ip_addr)                                        \
  do {                                                            \
    if (value == 0 || *value == 0)                                \
      return -1;                                                  \
    if (parse_address(value,&addr,AF_INET) != 0)                  \
      return -1;                                                  \
    p = (uint8_t*)&((struct sockaddr_in*)addr.ai_addr)->sin_addr; \
    IP4_ADDR(&(ip_addr),*p,*(p+1),*(p+2),*(p+3));                 \
    return 0;                                                     \
  } while(0)

  if (!strcmp(key,"name")) {
    if (value == 0 || *value == 0)
      return -1;
    tapif->name = value;
    return 0;
  } else if (!strcmp(key,"addr")) {
    PARSE_IP4(tapif->ip_addr);
  } else if (!strcmp(key,"netmask")) {
    PARSE_IP4(tapif->netmask);
  } else if (!strcmp(key,"gw")) {
    PARSE_IP4(tapif->gw);
  } else {
    return -1;
  }
}

int
parse_interface(struct tapif* tapif,char* param)
{
  enum {
    KEY_WAIT, KEY, VALUE_WAIT, VALUE, END
  };
  int state = KEY_WAIT;
  char* p = param;
  char* key = 0;
  char* value = 0;

  while (state != END) {
    switch (*p) {
    case '\0':
      if (parse_pair(tapif,key,value) != 0)
        return -1;
      state = END;
      break;
    case ',':
      if (state == KEY_WAIT) {
        p++;
        break;
      }
      state = KEY_WAIT;
      *p++ = 0;
      if (parse_pair(tapif,key,value) != 0)
        return -1;
      key = value = 0;
      break;
    case '=':
      if (state == KEY)
        state = VALUE_WAIT;
      else
        return -1;
      *p++ = 0;
      break;
    case ' ':
      if (state != KEY_WAIT && state != VALUE_WAIT)
        return -1;
      p++;
      break;
    default:
      if (state == KEY_WAIT) {
        state = KEY;
        key = p;
      } else if (state == VALUE_WAIT) {
        state = VALUE;
        value = p;
      }
      p++;
    }
  }
  return 0;
}

void
help(void)
{
#ifdef LWIP_DEBUG
  fprintf(stderr,"Usage: lwip-tap [-CEHSdh] -i addr=<addr>,netmask=<addr>,name=<name>,gw=<addr> [...]\n");
#else
  fprintf(stderr,"Usage: lwip-tap [-CEHSh] -i addr=<addr>,netmask=<addr>,name=<name>,gw=<addr> [...]\n");
#endif
  exit(0);
}

#define IP4_OR_NULL(ip_addr) ((ip_addr).addr == IPADDR_ANY ? 0 : &(ip_addr))

int
main(int argc,char *argv[])
{
  struct tapif tapif[NETIF_MAX];
  struct netif netif[NETIF_MAX];
  int ch;
  int n = 0;

  memset(tapif,0,sizeof(tapif));
  memset(netif,0,sizeof(netif));

  tcpip_init(NULL,NULL);

#ifdef LWIP_DEBUG
  while ((ch = getopt(argc,argv,"dhi:")) != -1) {
#else
  while ((ch = getopt(argc,argv,"hi:")) != -1) {
#endif
    switch (ch) {
#ifdef LWIP_DEBUG
    case 'd':
      debug_flags |= (LWIP_DBG_ON|LWIP_DBG_TRACE|LWIP_DBG_STATE|
                      LWIP_DBG_FRESH|LWIP_DBG_HALT);
      break;
#endif
    case 'i':
      if (n >= NETIF_MAX)
        break;

      if (parse_interface(&tapif[n],optarg) != 0) {
        help();
      }
      netif_add(&netif[n],
                IP4_OR_NULL(tapif[n].ip_addr),
                IP4_OR_NULL(tapif[n].netmask),
                IP4_OR_NULL(tapif[n].gw),
                &tapif[n],
                tapif_init,
                tcpip_input);

      if (n == 0) {
        netif_set_default(&netif[n]);
      }
      netif_set_up(&netif[n]);
      if (IP4_OR_NULL(tapif[n].ip_addr) == 0 &&
          IP4_OR_NULL(tapif[n].netmask) == 0 &&
          IP4_OR_NULL(tapif[n].gw) == 0) {
        dhcp_start(&netif[n]);
      }
      n++;
      break;
    case 'h':
    default:
      help();
    }
  }
  argc -= optind;
  argv += optind;
  if (n <= 0) {
    help();
  }

  /* init tcp echp by default */
  tcpecho_init();

  while(1) {
    sleep(100);
  }
  return 0;
}
