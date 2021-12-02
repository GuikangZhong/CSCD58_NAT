
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

int sr_nat_init(struct sr_nat *nat, unsigned int icmp_query_to, unsigned int tcp_estab_idle_to, unsigned int tcp_transitory_to) { /* Initializes the nat */

  assert(nat);

  /* Acquire mutex lock */
  pthread_mutexattr_init(&(nat->attr));
  pthread_mutexattr_settype(&(nat->attr), PTHREAD_MUTEX_RECURSIVE);
  int success = pthread_mutex_init(&(nat->lock), &(nat->attr));

  /* Initialize timeout thread */

  pthread_attr_init(&(nat->thread_attr));
  pthread_attr_setdetachstate(&(nat->thread_attr), PTHREAD_CREATE_JOINABLE);
  pthread_attr_setscope(&(nat->thread_attr), PTHREAD_SCOPE_SYSTEM);
  pthread_attr_setscope(&(nat->thread_attr), PTHREAD_SCOPE_SYSTEM);
  pthread_create(&(nat->thread), &(nat->thread_attr), sr_nat_timeout, nat);

  /* CAREFUL MODIFYING CODE ABOVE THIS LINE! */

  nat->mappings = NULL;
  /* Initialize any variables here */
  nat->ext_id = DEFAULT_ID;
  nat->icmp_query_to = icmp_query_to;
  nat->tcp_estab_idle_to = tcp_estab_idle_to;
  nat->tcp_transitory_to = tcp_transitory_to;

  return success;
}


int sr_nat_destroy(struct sr_nat *nat) {  /* Destroys the nat (free memory) */

  pthread_mutex_lock(&(nat->lock));

  /* free nat memory here */

  pthread_kill(nat->thread, SIGKILL);
  return pthread_mutex_destroy(&(nat->lock)) &&
    pthread_mutexattr_destroy(&(nat->attr));

}

void *sr_nat_timeout(void *nat_ptr) {  /* Periodic Timout handling */
  struct sr_nat *nat = (struct sr_nat *)nat_ptr;
  while (1) {
    sleep(1.0);
    pthread_mutex_lock(&(nat->lock));

    time_t curtime = time(NULL);
    
    /* handle periodic tasks here */
    struct sr_nat_mapping *dummy_head = (struct sr_nat_mapping *)calloc(1, sizeof(struct sr_nat_mapping));
    dummy_head->next = nat->mappings;
    struct sr_nat_mapping *prev = dummy_head;
    struct sr_nat_mapping *curr = nat->mappings;

    while (curr != NULL) {
      /* icmp timeout case */
      if (curr->type == nat_mapping_icmp && difftime(curtime, curr->last_updated) > nat->icmp_query_to) {
        prev->next = curr->next;
        curr = curr->next;
      }
      /* tcp connection timeout case */
      else if (curr->type == nat_mapping_tcp) {
        
      }
      else {
        prev = curr;
        curr = curr->next;      
      }

    }
    nat->mappings = dummy_head->next;
    free(dummy_head);
    pthread_mutex_unlock(&(nat->lock));
  }
  return NULL;
}

/* Get the mapping associated with given external port.
   You must free the returned structure if it is not NULL. */
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

/* Get the mapping associated with given internal (ip, port) pair.
   You must free the returned structure if it is not NULL. */
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

/* Insert a new mapping into the nat's mapping table.
   Actually returns a copy to the new mapping, for thread safety.
 */
struct sr_nat_mapping *sr_nat_insert_mapping(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type ) {

  pthread_mutex_lock(&(nat->lock));

  /* handle insert here, create a mapping, and then return a copy of it */
  
  struct sr_nat_mapping *new_entry;

  new_entry = (struct sr_nat_mapping *)calloc(1, sizeof(struct sr_nat_mapping));
  new_entry->ip_int = ip_int;
  new_entry->type = type;
  new_entry->aux_int = aux_int;
  new_entry->aux_ext = nat->ext_id;
  nat->ext_id++;
  new_entry->last_updated = time(NULL);
  new_entry->next = nat->mappings;
  nat->mappings = new_entry;

  struct sr_nat_mapping *mapping = (struct sr_nat_mapping *) malloc(sizeof(struct sr_nat_mapping)); 
  memcpy(mapping, new_entry, sizeof(struct sr_nat_mapping));

  pthread_mutex_unlock(&(nat->lock));
  return mapping;
}

struct sr_nat_connection *sr_nat_insert_connection(struct sr_nat *nat, uint16_t ext_port, 
  uint32_t peer_ip, uint16_t peer_port, uint32_t peer_seq_num) {

  pthread_mutex_lock(&(nat->lock));

  struct sr_nat_connection *copy = NULL;
  struct sr_nat_connection *new_entry;

  new_entry = (struct sr_nat_connection *)calloc(1, sizeof(struct sr_nat_connection));
  new_entry->peer_ip = peer_ip;
  new_entry->peer_port = peer_port;
  new_entry->peer_seq_num = peer_seq_num;
  new_entry->last_updated = time(NULL);

  /* find the corresponding ext_port */
  struct sr_nat_mapping *curr = nat->mappings;
  while (curr) {
    if (curr->aux_ext == ext_port) {
      break;
    }
    curr = curr->next;
  }

  if (curr) {
    new_entry->next = curr->conns;
    curr->conns = new_entry;
    struct sr_nat_connection *copy = (struct sr_nat_connection *) malloc(sizeof(struct sr_nat_connection)); 
    memcpy(copy, new_entry, sizeof(struct sr_nat_connection));
  }
  
  pthread_mutex_unlock(&(nat->lock));
  return copy;
}

struct sr_nat_connection *sr_nat_lookup_connection(struct sr_nat *nat, struct sr_nat_mapping *mapping, 
  uint32_t peer_ip, uint16_t peer_port, uint32_t peer_seq_num) {

  pthread_mutex_lock(&(nat->lock));

  struct sr_nat_connection *curr = mapping->conns;
  struct sr_nat_connection *copy = NULL;
  while (curr) {
    if (curr->peer_ip == peer_ip && curr->peer_seq_num == peer_seq_num 
      && curr->peer_port == peer_port) {
      break;
    }
    curr = curr->next;
  }

  if (curr) {
    copy = (struct sr_nat_connection *)malloc(sizeof(struct sr_nat_connection));
    memcpy(copy, curr, sizeof(struct sr_nat_connection));
  }
  
  pthread_mutex_unlock(&(nat->lock));
  return copy;
}