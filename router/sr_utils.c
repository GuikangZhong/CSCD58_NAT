#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sr_protocol.h"
#include "sr_nat.h"

/* Returns the check sum in network byte order */
uint16_t cksum (const void *_data, int len) {
  const uint8_t *data = _data;
  uint32_t sum;

  for (sum = 0;len >= 2; data += 2, len -= 2)
    sum += data[0] << 8 | data[1];
  if (len > 0)
    sum += data[0] << 8;
  while (sum > 0xffff)
    sum = (sum >> 16) + (sum & 0xffff);
  sum = htons (~sum);
  return sum ? sum : 0xffff;
}

int is_private_ip(uint32_t ip) {
  /* Class A: 10.0. 0.0 — 10.255. 255.255 */
  if ((ip & 0xff000000) == 0x0a000000) {
    return 1;
  }

  /* Class B: 172.16. 0.0 — 172.31. 255.255 */
  if ((ip & 0xffff0000) == 0xac100000) {
    return 1;
  }

  /* Class C: 192.168. 0.0 — 192.168. 255.255 */
  if ((ip & 0xffff0000) == 0xc0a80000) {
    return 1;
  }
  return 0;
}

/* Prints out formatted Ethernet address, e.g. 00:11:22:33:44:55 */
void print_addr_eth(uint8_t *addr) {
  int pos = 0;
  uint8_t cur;
  for (; pos < ETHER_ADDR_LEN; pos++) {
    cur = addr[pos];
    if (pos > 0)
      fprintf(stderr, ":");
    fprintf(stderr, "%02X", cur);
  }
  fprintf(stderr, "\n");
}

/* Prints out IP address as a string from in_addr */
void print_addr_ip(struct in_addr address) {
  char buf[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &address, buf, 100) == NULL)
    fprintf(stderr,"inet_ntop error on address conversion\n");
  else
    fprintf(stderr, "%s\n", buf);
}

/* Prints out IP address from integer value */
void print_addr_ip_int(uint32_t ip) {
  uint32_t curOctet = ip >> 24;
  fprintf(stderr, "%d.", curOctet);
  curOctet = (ip << 8) >> 24;
  fprintf(stderr, "%d.", curOctet);
  curOctet = (ip << 16) >> 24;
  fprintf(stderr, "%d.", curOctet);
  curOctet = (ip << 24) >> 24;
  fprintf(stderr, "%d\n", curOctet);
}


/* Prints out fields in Ethernet header. */
void print_hdr_eth(uint8_t *buf) {
  sr_ethernet_hdr_t *ehdr = (sr_ethernet_hdr_t *)buf;
  fprintf(stderr, "ETHERNET header:\n");
  fprintf(stderr, "\tdestination: ");
  print_addr_eth(ehdr->ether_dhost);
  fprintf(stderr, "\tsource: ");
  print_addr_eth(ehdr->ether_shost);
  fprintf(stderr, "\ttype: %d\n", ntohs(ehdr->ether_type));
}

/* Prints out fields in IP header. */
void print_hdr_ip(uint8_t *buf) {
  sr_ip_hdr_t *iphdr = (sr_ip_hdr_t *)(buf);
  fprintf(stderr, "IP header:\n");
  fprintf(stderr, "\tversion: %d\n", iphdr->ip_v);
  fprintf(stderr, "\theader length: %d\n", iphdr->ip_hl);
  fprintf(stderr, "\ttype of service: %d\n", iphdr->ip_tos);
  fprintf(stderr, "\tlength: %d\n", ntohs(iphdr->ip_len));
  fprintf(stderr, "\tid: %d\n", ntohs(iphdr->ip_id));

  if (ntohs(iphdr->ip_off) & IP_DF)
    fprintf(stderr, "\tfragment flag: DF\n");
  else if (ntohs(iphdr->ip_off) & IP_MF)
    fprintf(stderr, "\tfragment flag: MF\n");
  else if (ntohs(iphdr->ip_off) & IP_RF)
    fprintf(stderr, "\tfragment flag: R\n");

  fprintf(stderr, "\tfragment offset: %d\n", ntohs(iphdr->ip_off) & IP_OFFMASK);
  fprintf(stderr, "\tTTL: %d\n", iphdr->ip_ttl);
  fprintf(stderr, "\tprotocol: %d\n", iphdr->ip_p);

  /*Keep checksum in NBO*/
  fprintf(stderr, "\tchecksum: %d\n", iphdr->ip_sum);

  fprintf(stderr, "\tsource: ");
  print_addr_ip_int(ntohl(iphdr->ip_src));

  fprintf(stderr, "\tdestination: ");
  print_addr_ip_int(ntohl(iphdr->ip_dst));
}

/* Prints out TCP header fields */
void print_hdr_tcp(uint8_t *buf) {
  sr_tcp_hdr_t *tcp_hdr = (sr_tcp_hdr_t *)(buf);
  fprintf(stderr, "TCP header:\n");
  fprintf(stderr, "\tsource port: %d\n", ntohs(tcp_hdr->src_port));
  fprintf(stderr, "\tdestination port: %d\n", ntohs(tcp_hdr->dst_port));
  fprintf(stderr, "\tsequence number: %u\n", ntohl(tcp_hdr->seq_num));
  fprintf(stderr, "\tacknowledment number: %u\n", ntohl(tcp_hdr->ack_num));
  fprintf(stderr, "\tdata offset: %d\n", tcp_hdr->data_offset);
  fprintf(stderr, "\tecn: %d\n", tcp_hdr->ecn);
  fprintf(stderr, "\tURG: %d\n", tcp_hdr->URG);
  fprintf(stderr, "\tACK: %d\n", tcp_hdr->ACK);
  fprintf(stderr, "\tPSH: %d\n", tcp_hdr->PSH);
  fprintf(stderr, "\tRST: %d\n", tcp_hdr->RST);
  fprintf(stderr, "\tSYN: %d\n", tcp_hdr->SYN);
  fprintf(stderr, "\tFIN: %d\n", tcp_hdr->FIN);
  fprintf(stderr, "\twindow: %d\n", ntohs(tcp_hdr->window));
  fprintf(stderr, "\tchecksum: %d\n", tcp_hdr->checksum);
  fprintf(stderr, "\turgent_pointer: %d\n", tcp_hdr->urgent_pointer);
}

/* Prints out ICMP header fields */
void print_hdr_icmp(uint8_t *buf) {
  sr_icmp_hdr_t *icmp_hdr = (sr_icmp_hdr_t *)(buf);
  fprintf(stderr, "ICMP header:\n");
  fprintf(stderr, "\ttype: %d\n", icmp_hdr->icmp_type);
  fprintf(stderr, "\tcode: %d\n", icmp_hdr->icmp_code);
  /* Keep checksum in NBO */
  fprintf(stderr, "\tchecksum: %d\n", icmp_hdr->icmp_sum);
  fprintf(stderr, "\tidentifier: %d\n", icmp_hdr->identifier);
  fprintf(stderr, "\tseq_num: %d\n", icmp_hdr->seq_num);
}


/* Prints out fields in ARP header */
void print_hdr_arp(uint8_t *buf) {
  sr_arp_packet_t *arp_hdr = (sr_arp_packet_t *)(buf);
  fprintf(stderr, "ARP header\n");
  fprintf(stderr, "\thardware type: %d\n", ntohs(arp_hdr->ar_hrd));
  fprintf(stderr, "\tprotocol type: %d\n", ntohs(arp_hdr->ar_pro));
  fprintf(stderr, "\thardware address length: %d\n", arp_hdr->ar_hln);
  fprintf(stderr, "\tprotocol address length: %d\n", arp_hdr->ar_pln);
  fprintf(stderr, "\topcode: %d\n", ntohs(arp_hdr->ar_op));

  fprintf(stderr, "\tsender hardware address: ");
  print_addr_eth(arp_hdr->ar_sha);
  fprintf(stderr, "\tsender ip address: ");
  print_addr_ip_int(ntohl(arp_hdr->ar_sip));

  fprintf(stderr, "\ttarget hardware address: ");
  print_addr_eth(arp_hdr->ar_tha);
  fprintf(stderr, "\ttarget ip address: ");
  print_addr_ip_int(ntohl(arp_hdr->ar_tip));
}

void print_sr_mapping(struct sr_nat_mapping *mapping) {
  fprintf(stderr, "\nIP_INT       aux_int       aux_ext          type\n");
  fprintf(stderr, "-----------------------------------------------------------\n");

  struct sr_nat_mapping *curr = mapping;
  while (curr) {
    printf("%.8x       %d          %d            %d\n", curr->ip_int, curr->aux_int, curr->aux_ext, curr->type);
    curr = curr->next;
  }
}