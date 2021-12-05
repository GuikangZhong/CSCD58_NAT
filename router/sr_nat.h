
#ifndef SR_NAT_TABLE_H
#define SR_NAT_TABLE_H

#include <inttypes.h>
#include <time.h>
#include <pthread.h>
#include "sr_protocol.h"

#define DEFAULT_ID 1024
#define DEFAULT_ICMP_QUERY_TO 60
#define DEFAULT_TCP_ESTABLISHED_TO 7440
#define DEFAULT_TCP_TRANSITORY_TO 300
#define TOTAL_PORTS 65536
#define DEFAULT_TCP_SYN_TO 6

#define ClearBit(A,k)   ( A[(k/32)] &= ~(1 << (k%32)) ) /* Reference website: http://www.mathcs.emory.edu/~cheung/Courses/255/Syllabus/1-C-intro/bit-array.html */

struct sr_instance;

typedef enum {
  nat_mapping_icmp,
  nat_mapping_tcp
  /* nat_mapping_udp, */
} sr_nat_mapping_type;

typedef enum {
  CLOSED,
  LISTEN,
  SYN_SENT,
  SYN_RCVD,
  ESTAB,
  FIN_WAIT_1,
  FIN_WAIT_2,
  CLOSING,
  TIME_WAIT,
  CLOST_WAIT,
  LAST_ACK
} sr_tcp_state_type;

struct sr_nat_unsol_pkt {
  uint8_t *ip_packet;
  time_t last_updated;
  struct sr_nat_unsol_pkt *next;
};

struct sr_nat_connection {
  /* add TCP connection state data members here */
  sr_tcp_state_type state;
  uint32_t peer_ip;
  uint16_t peer_port;
  time_t last_updated; /* use to timeout connections */
  struct sr_nat_connection *next;
};

struct sr_nat_mapping {
  sr_nat_mapping_type type;
  uint32_t ip_int; /* internal ip addr */
  uint32_t ip_ext; /* external ip addr */
  uint16_t aux_int; /* internal port or icmp id */
  uint16_t aux_ext; /* external port or icmp id */
  time_t last_updated; /* use to timeout mappings */
  struct sr_nat_connection *conns; /* list of connections. null for ICMP */
  struct sr_nat_mapping *next;
};

struct sr_nat {
  /* add any fields here */
  struct sr_nat_mapping *mappings;
  unsigned int icmp_query_to; /* icmp query timeout */
  unsigned int tcp_estab_idle_to; /* tcp established timeout */
  unsigned int tcp_transitory_to; /* tcp transitory timeout */
  int bitmap[TOTAL_PORTS / 32]; /* bit map to check if a ext id is in use */
  struct sr_nat_unsol_pkt *unsol_pkt; /* unsolicited packets */

  /* threading */
  pthread_mutex_t lock;
  pthread_mutexattr_t attr;
  pthread_attr_t thread_attr;
  pthread_t thread;
};


int   sr_nat_init(struct sr_instance *sr, unsigned int icmp_query_to, unsigned int tcp_estab_idle_to, unsigned int tcp_transitory_to);     /* Initializes the nat */
int   sr_nat_destroy(struct sr_nat *nat);  /* Destroys the nat (free memory) */
void *sr_nat_timeout(void *sr_ptr);  /* Periodic Timout */

/* Get the mapping associated with given external port.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_lookup_external(struct sr_nat *nat,
    uint16_t aux_ext, sr_nat_mapping_type type );

/* Get the mapping associated with given internal (ip, port) pair.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_lookup_internal(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type );

/* Insert a new mapping into the nat's mapping table.
   You must free the returned structure if it is not NULL. */
struct sr_nat_mapping *sr_nat_insert_mapping(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type );

struct sr_nat_connection *sr_nat_update_connection(struct sr_nat *nat, struct sr_nat_mapping *mapping, uint8_t *ip_packet, unsigned int ip_packet_len, int direction);

struct sr_nat_connection *sr_nat_insert_connection(struct sr_nat *nat, uint16_t ext_port, uint8_t *ip_buf, unsigned int state);



#endif