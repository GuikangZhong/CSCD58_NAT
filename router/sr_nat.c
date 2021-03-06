
#include <signal.h>
#include <assert.h>
#include "sr_nat.h"
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sr_utils.h"
#include "sr_protocol.h"
#include "sr_router.h"
#include "sr_rt.h"

int find_next_id(struct sr_nat *nat);
sr_tcp_state_type _determine_state(struct sr_nat_connection *conn, sr_tcp_hdr_t *buf);

/*---------------------------------------------------------------------
 * Method: sr_nat_init(void)
 * Scope:  Global
 *
 * Initialize the NAT
 *---------------------------------------------------------------------*/
int sr_nat_init(struct sr_instance *sr, unsigned int icmp_query_to, unsigned int tcp_estab_idle_to, unsigned int tcp_transitory_to) {
  assert(sr);
  assert(&(sr->nat));

  struct sr_nat *nat = &(sr->nat);
  /* Acquire mutex lock */
  pthread_mutexattr_init(&(nat->attr));
  pthread_mutexattr_settype(&(nat->attr), PTHREAD_MUTEX_RECURSIVE);
  int success = pthread_mutex_init(&(nat->lock), &(nat->attr));

  /* Initialize timeout thread */

  pthread_attr_init(&(nat->thread_attr));
  pthread_attr_setdetachstate(&(nat->thread_attr), PTHREAD_CREATE_JOINABLE);
  pthread_attr_setscope(&(nat->thread_attr), PTHREAD_SCOPE_SYSTEM);
  pthread_attr_setscope(&(nat->thread_attr), PTHREAD_SCOPE_SYSTEM);
  pthread_create(&(nat->thread), &(nat->thread_attr), sr_nat_timeout, sr);

  /* CAREFUL MODIFYING CODE ABOVE THIS LINE! */

  nat->mappings = NULL;
  /* Initialize any variables here */
  nat->icmp_query_to = icmp_query_to;
  nat->tcp_estab_idle_to = tcp_estab_idle_to;
  nat->tcp_transitory_to = tcp_transitory_to;

  int i = 0;
  for ( i = 0; i < TOTAL_PORTS/32; i++ )
    nat->bitmap[i] = 0;

  return success;
}

/*---------------------------------------------------------------------
 * Method: sr_nat_destroy(int)
 * Scope:  Global
 *
 * Destroys the nat (free memory)
 *---------------------------------------------------------------------*/
int sr_nat_destroy(struct sr_nat *nat) {
  pthread_mutex_lock(&(nat->lock));

  /* free nat memory here */
  struct sr_nat_mapping *curr = nat->mappings;
  struct sr_nat_mapping *temp;
  while (curr != NULL) {
    temp = curr;
    curr = curr->next;
    free(temp);
  }

  pthread_kill(nat->thread, SIGKILL);
  return pthread_mutex_destroy(&(nat->lock)) &&
    pthread_mutexattr_destroy(&(nat->attr));

}

/*---------------------------------------------------------------------
 * Method: sr_nat_timeout(void)
 * Scope:  Global
 *
 * Periodic Timout handling
 *---------------------------------------------------------------------*/
void *sr_nat_timeout(void *sr_ptr) {
  struct sr_instance *sr = sr_ptr;
  struct sr_nat *nat = &(sr->nat);
  while (1) {
    sleep(1.0);
    pthread_mutex_lock(&(nat->lock));

    time_t curtime = time(NULL);
    
    /* handle periodic tasks here */
    /* first, check whether the unsolicited packets has a corresponding mapping */
    struct sr_nat_unsol_pkt *dummy_unsol_head = (struct sr_nat_unsol_pkt *)calloc(1, sizeof(struct sr_nat_unsol_pkt));
    dummy_unsol_head->next = nat->unsol_pkt;
    struct sr_nat_unsol_pkt *prev_unsol = dummy_unsol_head;
    struct sr_nat_unsol_pkt *curr_unsol = nat->unsol_pkt;

    while (curr_unsol != NULL) {
      sr_ip_hdr_t *ip_header = (sr_ip_hdr_t *)(curr_unsol->ip_packet);
      sr_rt_t *lpm = sr_rt_lookup(sr->routing_table, ip_header->ip_src);
      if (lpm && difftime(curtime, curr_unsol->last_updated) > DEFAULT_TCP_SYN_TO) {
        printf("[NAT]: TCP inbound SYN timeout\n");
        sr_send_icmp(sr, curr_unsol->ip_packet, lpm->interface, icmp_type_dstunreachable, 3);
        prev_unsol->next = curr_unsol->next;
        free(curr_unsol);
        curr_unsol = prev_unsol->next;
      }
      else {
        prev_unsol = curr_unsol;
        curr_unsol = curr_unsol->next;
      }
    }
    nat->unsol_pkt = dummy_unsol_head->next;
    free(dummy_unsol_head);

    struct sr_nat_mapping *dummy_head = (struct sr_nat_mapping *)calloc(1, sizeof(struct sr_nat_mapping));
    dummy_head->next = nat->mappings;
    struct sr_nat_mapping *prev = dummy_head;
    struct sr_nat_mapping *curr = nat->mappings;

    while (curr != NULL) {
      /* icmp timeout case */
      if (curr->type == nat_mapping_icmp) {
        if (difftime(curtime, curr->last_updated) > nat->icmp_query_to) {
          prev->next = curr->next;
          printf("[NAT]: ICMP query timeout, clean mapping of id %d\n", curr->aux_ext);
          ClearBit(nat->bitmap, curr->aux_ext);
          free(curr);
          curr = prev->next;
        } 
        else {
          prev = curr;
          curr = curr->next;
        }
      }
      
      
      /* tcp connection timeout case */
      else if (curr->type == nat_mapping_tcp) {
        
        struct sr_nat_connection *dummy_conn = (struct sr_nat_connection *)calloc(1, sizeof(struct sr_nat_connection));
        dummy_conn->next = curr->conns;
        struct sr_nat_connection *pre_conn = dummy_conn;
        struct sr_nat_connection *cur_conn = curr->conns;
        
        while (cur_conn != NULL) {
          if (cur_conn->state == ESTAB && difftime(curtime,cur_conn->last_updated) > nat->tcp_estab_idle_to) {
              pre_conn->next = cur_conn->next;
              free(cur_conn);
              cur_conn = pre_conn->next;
          }

          else if (cur_conn->state == CLOSED) {
            pre_conn->next = cur_conn->next;
            free(cur_conn);
            cur_conn = pre_conn->next;
          }

          else if (difftime(curtime,cur_conn->last_updated) > nat->tcp_transitory_to) {
            pre_conn->next = cur_conn->next;
            free(cur_conn);
            cur_conn = pre_conn->next;
          }
          else {
            pre_conn = cur_conn;
            cur_conn = cur_conn->next;          
          }
        }
        
        curr->conns = dummy_conn->next;
        free(dummy_conn);
        if (curr->conns == NULL) {
          prev->next = curr->next;
          printf("[NAT]: TCP mapping timeout, clean mapping of id %d\n", curr->aux_ext);
          ClearBit(nat->bitmap, curr->aux_ext);
          free(curr);
          curr = prev->next;
        } 
        else {
          prev = curr;
          curr = curr->next;
        }
      }
    }
    nat->mappings = dummy_head->next;
    free(dummy_head);
    pthread_mutex_unlock(&(nat->lock));
  }
  return NULL;
}

/*---------------------------------------------------------------------
 * Method: sr_nat_lookup_external(struct sr_nat_mapping *)
 * Scope:  Global
 *
 * Get the mapping associated with given external port.
 * You must free the returned structure if it is not NULL.
 *---------------------------------------------------------------------*/
struct sr_nat_mapping *sr_nat_lookup_external(struct sr_nat *nat,
    uint16_t aux_ext, sr_nat_mapping_type type ) {
  

  pthread_mutex_lock(&(nat->lock));

  /* handle lookup here, malloc and assign to copy */
  struct sr_nat_mapping *copy = NULL;
  struct sr_nat_mapping *curr = nat->mappings;
  while (curr) {
    if (curr->aux_ext == aux_ext) {
      break;
    }
    curr = curr->next;
  }
  

  if (curr) {
    copy = (struct sr_nat_mapping *) malloc(sizeof(struct sr_nat_mapping)); 
    memcpy(copy, curr, sizeof(struct sr_nat_mapping));
  }
  

  pthread_mutex_unlock(&(nat->lock));
  return copy;
}

/*---------------------------------------------------------------------
 * Method: sr_nat_lookup_internal(struct sr_nat_mapping *)
 * Scope:  Global
 *
 * Get the mapping associated with given internal (ip, port) pair.
 * You must free the returned structure if it is not NULL.
 *---------------------------------------------------------------------*/
struct sr_nat_mapping *sr_nat_lookup_internal(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type ) {

  pthread_mutex_lock(&(nat->lock));

  /* handle lookup here, malloc and assign to copy. */
  struct sr_nat_mapping *copy = NULL;
  struct sr_nat_mapping *curr = nat->mappings;
  while (curr) {
    if (curr->ip_int == ip_int && curr->aux_int == aux_int) {
      break;
    }
    curr = curr->next;
  }

  if (curr) {
    copy = (struct sr_nat_mapping *) malloc(sizeof(struct sr_nat_mapping)); 
    memcpy(copy, curr, sizeof(struct sr_nat_mapping));
  }

  pthread_mutex_unlock(&(nat->lock));
  return copy;
}

/*---------------------------------------------------------------------
 * Method: sr_nat_insert_mapping(struct sr_nat_mapping *)
 * Scope:  Global
 *
 * Insert a new mapping into the nat's mapping table.
 * Actually returns a copy to the new mapping, for thread safety.
 * You must free the returned structure if it is not NULL.
 *---------------------------------------------------------------------*/
struct sr_nat_mapping *sr_nat_insert_mapping(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type ) {

  pthread_mutex_lock(&(nat->lock));

  /* handle insert here, create a mapping, and then return a copy of it */
  
  struct sr_nat_mapping *new_entry;

  new_entry = (struct sr_nat_mapping *)calloc(1, sizeof(struct sr_nat_mapping));
  new_entry->ip_int = ip_int;
  new_entry->type = type;
  new_entry->aux_int = aux_int;
  new_entry->aux_ext = find_next_id(nat);
  new_entry->last_updated = time(NULL);
  new_entry->next = nat->mappings;
  nat->mappings = new_entry;

  struct sr_nat_mapping *mapping = (struct sr_nat_mapping *) malloc(sizeof(struct sr_nat_mapping)); 
  memcpy(mapping, new_entry, sizeof(struct sr_nat_mapping));

  pthread_mutex_unlock(&(nat->lock));
  return mapping;
}

/*---------------------------------------------------------------------
 * Method: find_next_id(int)
 * Scope:  Global
 *
 * Return the next avaliable id used for external icmp identifier or tcp port.
 *---------------------------------------------------------------------*/
int find_next_id(struct sr_nat *nat) {
  /* loop through the bitmap */
  int i;
  for (i=DEFAULT_ID/32; i < TOTAL_PORTS/32; i++) {
    int num = nat->bitmap[i];
    int j;
    for ( j = 0; j < 32; j++) {
      if (!(num & 0x01)) {
          int res = (i * 32) + j;
          nat->bitmap[i] |= (1 << (res % 32));
          return res;
      }
      num = num >> 1;
    }
  }
  return -1;
}

/*---------------------------------------------------------------------
 * Method: sr_nat_insert_connection(struct sr_nat_connection *)
 * Scope:  Global
 *
 * Insert a new connection into the coresponsing mapping wrt the nat's external port.
 * Actually returns a copy to the new connection, for thread safety.
 * You must free the returned structure if it is not NULL.
 *---------------------------------------------------------------------*/
struct sr_nat_connection *sr_nat_insert_connection(struct sr_nat *nat, uint16_t ext_port, uint8_t *ip_packet, unsigned int state) {

  pthread_mutex_lock(&(nat->lock));

  struct sr_nat_connection *copy = NULL;
  struct sr_nat_connection *new_entry;

  /* find the corresponding ext_port */
  struct sr_nat_mapping *curr = nat->mappings;
  while (curr) {
    if (curr->aux_ext == ext_port) {
      break;
    }
    curr = curr->next;
  }

  if (curr) {
    sr_ip_hdr_t *ip_header = (sr_ip_hdr_t *)(ip_packet);
    unsigned int ip_header_len = (ip_header->ip_hl)*4;
    sr_tcp_hdr_t *tcp_header = (sr_tcp_hdr_t *)(ip_packet + ip_header_len);

    new_entry = (struct sr_nat_connection *)calloc(1, sizeof(struct sr_nat_connection));
    new_entry->peer_ip = ip_header->ip_dst;
    new_entry->peer_port = tcp_header->dst_port;
    new_entry->state = state;
    new_entry->last_updated = time(NULL);
    
    printf("[insert_connection] found a mapping\n");
    new_entry->next = curr->conns;
    curr->conns = new_entry;
    copy = (struct sr_nat_connection *) malloc(sizeof(struct sr_nat_connection)); 
    memcpy(copy, new_entry, sizeof(struct sr_nat_connection));

    /* remove the corresponding SYN packet */
    sr_nat_remove_unsolicited_packet(nat, ip_packet);
  } else {
    printf("[insert_connection] did not find a mapping\n");
  }
  
  pthread_mutex_unlock(&(nat->lock));
  return copy;
}

/*---------------------------------------------------------------------
 * Method: sr_nat_update_connection(struct sr_nat_connection *)
 * Scope:  Global
 *
 * Look up a connection for a given mapping and update the connection's state.
 * Actually returns a copy to the updated connection, for thread safety.
 * You must free the returned structure if it is not NULL.
 *---------------------------------------------------------------------*/
struct sr_nat_connection *sr_nat_update_connection(struct sr_nat *nat, struct sr_nat_mapping *mapping, uint8_t *ip_packet, 
  unsigned int ip_packet_len, int direction) {
  
  /* 1: external -> internal ; ow */
  pthread_mutex_lock(&(nat->lock));

  struct sr_nat_connection *curr = mapping->conns;
  struct sr_nat_connection *copy = NULL;

  sr_ip_hdr_t *ip_header = (sr_ip_hdr_t *)(ip_packet);
  unsigned int ip_header_len = (ip_header->ip_hl)*4;
  sr_tcp_hdr_t *tcp_header = (sr_tcp_hdr_t *)(ip_packet + ip_header_len);


  while (curr) {
    /* external -> internal */
    if (direction == 1 && curr->peer_ip == ip_header->ip_src && curr->peer_port == tcp_header->src_port) {
      printf("[update_connection] found a mapping (inbound)\n");
      break;
    } 
    /* internal -> external */
    else if (direction == 0 && curr->peer_ip == ip_header->ip_dst && curr->peer_port == tcp_header->dst_port) {
      printf("[update_connection] found a mapping (outbound)\n");
      break;
    }
    curr = curr->next;
  }

  if (curr) {
    /* external -> internal */
    if (direction == 1) {
      curr->state = _determine_state(curr, tcp_header);
      curr->last_updated = time(NULL);
      /* if external to internal, find the connection with peer_ip and peer_port
          matched to the input */
    } 
    /* internal -> external */
    else 
    {
      curr->state = _determine_state(curr, tcp_header);
      curr->last_updated = time(NULL);
    }
    copy = (struct sr_nat_connection *)malloc(sizeof(struct sr_nat_connection));
    memcpy(copy, curr, sizeof(struct sr_nat_connection));
  } else {
    /* if not found, it means the external host initiates this connection first, 
      so buffer this unsolicited inbound SYN */
    sr_nat_insert_unsolicited_packet(nat, ip_packet, ip_packet_len);
  }
  
  pthread_mutex_unlock(&(nat->lock));
  return copy;
}

/*---------------------------------------------------------------------
 * Method: sr_nat_remove_unsolicited_packet(struct sr_nat *nat, uint8_t* ip_packet)
 * Scope:  global
 *
 * Remove the unsolicited SYN packet from sr_nat_mapping
 *---------------------------------------------------------------------*/
void sr_nat_remove_unsolicited_packet(struct sr_nat *nat, uint8_t* ip_packet) {
  pthread_mutex_lock(&(nat->lock));

  printf("[NAT]: remove unsolicited packet\n");
  struct sr_nat_unsol_pkt *dummy = calloc(1, sizeof(struct sr_nat_unsol_pkt));
  struct sr_nat_unsol_pkt *prev = dummy;
  struct sr_nat_unsol_pkt *curr = nat->unsol_pkt;
  dummy->next = nat->unsol_pkt;

  sr_ip_hdr_t *ip_header = (sr_ip_hdr_t *)(ip_packet);
  unsigned int ip_header_len = (ip_header->ip_hl)*4;
  sr_tcp_hdr_t *tcp_header = (sr_tcp_hdr_t *)(ip_packet + ip_header_len);

  sr_ip_hdr_t *temp_ip_header;
  sr_tcp_hdr_t *temp_tcp_header;
  uint8_t *temp_pkt;

  while (curr) {
    temp_pkt = curr->ip_packet;
    temp_ip_header = (sr_ip_hdr_t *)(temp_pkt);
    temp_tcp_header = (sr_tcp_hdr_t *)(temp_pkt + ip_header_len);
    if (ip_header->ip_src == temp_ip_header->ip_src && ip_header->ip_dst == temp_ip_header->ip_dst && 
      tcp_header->src_port == temp_tcp_header->src_port && tcp_header->dst_port == temp_tcp_header->dst_port &&
      tcp_header->seq_num == temp_tcp_header->seq_num) {
      prev->next = curr->next;
      free(curr);
      curr = prev->next;
    }
    else {
      prev = curr;
      curr = curr->next;
    }
  }
  pthread_mutex_unlock(&(nat->lock));
}

/*---------------------------------------------------------------------
 * Method: sr_nat_insert_unsolicited_packet(struct sr_nat *nat, uint8_t* ip_packet, unsigned int ip_packet_len)
 * Scope:  global
 *
 * Insert the unsolicited SYN packet into sr_nat_mapping
 *---------------------------------------------------------------------*/
void sr_nat_insert_unsolicited_packet(struct sr_nat *nat, uint8_t* ip_packet, unsigned int ip_packet_len) {
  
  pthread_mutex_lock(&(nat->lock));
  
  printf("[NAT]: insert the unsolicited packet\n");
  struct sr_nat_unsol_pkt *dummy = calloc(1, sizeof(struct sr_nat_unsol_pkt));
  struct sr_nat_unsol_pkt *prev = dummy;
  struct sr_nat_unsol_pkt *curr = nat->unsol_pkt;
  dummy->next = nat->unsol_pkt;
  

  sr_ip_hdr_t *ip_header = (sr_ip_hdr_t *)(ip_packet);
  unsigned int ip_header_len = (ip_header->ip_hl)*4;
  sr_tcp_hdr_t *tcp_header = (sr_tcp_hdr_t *)(ip_packet + ip_header_len);

  sr_ip_hdr_t *temp_ip_header;
  sr_tcp_hdr_t *temp_tcp_header;
  uint8_t *temp_pkt;
  
  while (curr) {
    temp_pkt = curr->ip_packet;
    temp_ip_header = (sr_ip_hdr_t *)(temp_pkt);
    temp_tcp_header = (sr_tcp_hdr_t *)(temp_pkt + ip_header_len);
    if (ip_header->ip_src == temp_ip_header->ip_src && ip_header->ip_dst == temp_ip_header->ip_dst && 
      tcp_header->src_port == temp_tcp_header->src_port && tcp_header->dst_port == temp_tcp_header->dst_port &&
      tcp_header->seq_num == temp_tcp_header->seq_num) {
      printf("[NAT]: drop the unsolicited packet\n");
      break;
    }
    prev = curr;
    curr = curr->next;
  }
  if (!curr) {
  
    curr = calloc(1, sizeof(struct sr_nat_unsol_pkt));
    uint8_t *copy = malloc(ip_packet_len);
    memcpy(copy, ip_packet, ip_packet_len);
    curr->ip_packet = copy;
    curr->last_updated = time(NULL);
    prev->next = curr;
    nat->unsol_pkt = dummy->next;
    free(dummy);
  }
  pthread_mutex_unlock(&(nat->lock));
}

/*---------------------------------------------------------------------
 * Method: _determine_state(sr_tcp_state_type)
 * Scope:  Local
 *
 * Based on the current connection state and the tcp flags determine the next state
 *---------------------------------------------------------------------*/
sr_tcp_state_type _determine_state(struct sr_nat_connection *conn, sr_tcp_hdr_t *buf) {
  if (buf->SYN == 1 && buf->ACK == 0) {
    return SYN_SENT;
  }
  else if (conn->state == SYN_SENT && buf->SYN == 1 && buf->ACK == 1) {
    return ESTAB;
  }
  else if (conn->state == SYN_SENT && buf->SYN == 1) {
    return SYN_RCVD;
  }
  else if (conn->state == SYN_RCVD && buf->ACK == 1) {
    return ESTAB;
  }
  else if (conn->state == ESTAB && buf->FIN == 1) {
    return FIN_WAIT_1;
  }
  else if (conn->state == FIN_WAIT_1 && buf->FIN == 1) {
    return CLOSING;
  }
  else if (conn->state == CLOSING && buf->ACK == 1) {
    return CLOSED;
  }
  else if (conn->state == FIN_WAIT_1 && buf->FIN == 1 && buf->ACK == 1) {
    return CLOSED;
  }
  else if (conn->state == FIN_WAIT_1 && buf->ACK == 1){
    return FIN_WAIT_2;
  }
  else if (conn->state == FIN_WAIT_2 && buf->FIN == 1) {
    return CLOSED;
  }
  return conn->state;
}