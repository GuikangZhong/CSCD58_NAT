# CSCD58_NAT Project

## Team member
Guikang Zhong 1004304807 zhonggui<br>
Jingwei Wang 1003777754 wangj589<br>

## Acknowledgement
This project is built on Linda Lo's simple router code in Fall 2021.

## Description of the project
Network Address Translation (NAT) is a technique that solves the shortage of IPv4 addresses. The local router translates the private IP addresses of local hosts into one of the router's public IP address so that the local hosts can communicate with hosts in the external network. In this project, we follow this NAT handout (https://github.com/mininet/mininet/wiki/Network-Address-Translator-%28NAT%29) to build a simple Network Address Translator that can handle IP address translation for ICMP and TCP packets. 

## Implementation details and Documentation

### Starter Code Changes
1. We would like to point out a problem of the starter code. Originally, there was a swtich lying in the network topology shown in the NAT handout. Somehow, this switch is broken and we are not able to detect the error location. Therefore, we deleted this problematic switch.<br>
2. To test whether NAT works for mutiple internal hosts, we added an additional host (client2) into the topology.<br>
3. To test whether NAT can handle the "simulataneous open" case, we added an external peer (client3) into the topology.<br><br>
Please refer to the topology below instead of the one in the NAT handout<br>
![alt text](/images/modified_topology.svg "modified topology")

## Contributions from each member of the team
Guikang Zhong:<br>
1.Built Nat Mapping<br>
2.Handled IP translation for ICMP case<br>
3.Handled IP translationfor TCP case<br>

Jingwei Wang:<br>
1.Handled IP translation for ICMP case<br>
2.Handled IP translationfor TCP case<br>
3.Handled TCP state trasition<br>



## implementation details and documentation
sr_rt.c<br>
sr_rt.h<br>
```C
/*---------------------------------------------------------------------
 * Method: nat_handle_tcp
 * Input: struct sr_instance* sr, uint8_t *ip_packet,
 * unsigned int ip_packet_len, int is_to_nat
 * Output: int
 * Scope:  Global
 *
 * Translate the ip address and or port for a given tcp packet with an ip header,
 * return a status code, 1 for successful translation, 0 for no translation required
 * and -1 is a signal to the router to drop the packet due to connection no found or timeout.
 *---------------------------------------------------------------------*/
int nat_handle_tcp(struct sr_instance* sr, uint8_t *ip_packet, unsigned int ip_packet_len, int is_to_nat);

/*---------------------------------------------------------------------
 * Method: nat_handle_icmp
 * Input: struct sr_instance* sr, uint8_t *ip_packet,
 * Output: int
 * Scope:  Global
 *
 * Translate the ip address and or ICMP identifier for a given ICPM packet with an ip header,
 * return a status code, 1 for successful translation, 0 for no translation required.
 *---------------------------------------------------------------------*/
int nat_handle_icmp(struct sr_instance* sr, uint8_t *ip_packet);
```

sr_utils.c<br>
sr_utils.h<br>
```C
/* return 1 if the given ip is one of the private class of ip */
int is_private_ip(uint32_t ip);

/* Prints out formated connection state */
void print_state(sr_tcp_state_type state);

/* print out formatted mappings */
void print_sr_mapping(struct sr_nat_mapping *mapping);
```

sr_nat.c<br>
sr_nat.h<br>
```C
/*---------------------------------------------------------------------
 * Method: sr_nat_init(void)
 * Scope:  Global
 *
 * Initialize the NAT
 *---------------------------------------------------------------------*/
int sr_nat_init(struct sr_instance *sr, unsigned int icmp_query_to, 
                unsigned int tcp_estab_idle_to, unsigned int tcp_transitory_to);

/*---------------------------------------------------------------------
 * Method: sr_nat_destroy(int)
 * Scope:  Global
 *
 * Destroys the nat (free memory)
 *---------------------------------------------------------------------*/
int sr_nat_destroy(struct sr_nat *nat);

/*---------------------------------------------------------------------
 * Method: sr_nat_timeout(void)
 * Scope:  Global
 *
 * Periodic Timout handling
 *---------------------------------------------------------------------*/
void *sr_nat_timeout(void *sr_ptr);

/*---------------------------------------------------------------------
 * Method: sr_nat_lookup_external(struct sr_nat_mapping *)
 * Scope:  Global
 *
 * Get the mapping associated with given external port.
 * You must free the returned structure if it is not NULL.
 *---------------------------------------------------------------------*/
struct sr_nat_mapping *sr_nat_lookup_external(struct sr_nat *nat,
    uint16_t aux_ext, sr_nat_mapping_type type );

/*---------------------------------------------------------------------
 * Method: sr_nat_lookup_internal(struct sr_nat_mapping *)
 * Scope:  Global
 *
 * Get the mapping associated with given internal (ip, port) pair.
 * You must free the returned structure if it is not NULL.
 *---------------------------------------------------------------------*/
struct sr_nat_mapping *sr_nat_lookup_internal(struct sr_nat *nat,
    uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type );

/*---------------------------------------------------------------------
 * Method: sr_nat_insert_mapping(struct sr_nat_mapping *)
 * Scope:  Global
 *
 * Insert a new mapping into the nat's mapping table.
 * Actually returns a copy to the new mapping, for thread safety.
 * You must free the returned structure if it is not NULL.
 *---------------------------------------------------------------------*/
struct sr_nat_mapping *sr_nat_insert_mapping(struct sr_nat *nat,
  uint32_t ip_int, uint16_t aux_int, sr_nat_mapping_type type );

/*---------------------------------------------------------------------
 * Method: find_next_id(int)
 * Scope:  Global
 *
 * Return the next avaliable id used for external icmp identifier or tcp port.
 *---------------------------------------------------------------------*/
int find_next_id(struct sr_nat *nat);

/*---------------------------------------------------------------------
 * Method: sr_nat_insert_connection(struct sr_nat_connection *)
 * Scope:  Global
 *
 * Insert a new connection into the coresponsing mapping wrt the nat's external port.
 * Actually returns a copy to the new connection, for thread safety.
 * You must free the returned structure if it is not NULL.
 *---------------------------------------------------------------------*/
struct sr_nat_connection *sr_nat_insert_connection(struct sr_nat *nat, uint16_t ext_port, uint8_t *ip_packet, unsigned int state);

/*---------------------------------------------------------------------
 * Method: sr_nat_update_connection(struct sr_nat_connection *)
 * Scope:  Global
 *
 * Look up a connection for a given mapping and update the connection's state.
 * Actually returns a copy to the updated connection, for thread safety.
 * You must free the returned structure if it is not NULL.
 *---------------------------------------------------------------------*/
struct sr_nat_connection *sr_nat_update_connection(struct sr_nat *nat, struct sr_nat_mapping *mapping, uint8_t *ip_packet, int direction);

/*---------------------------------------------------------------------
 * Method: _determine_state(sr_tcp_state_type)
 * Scope:  Local
 *
 * Based on the current connection state and the tcp flags determine the next state
 *---------------------------------------------------------------------*/
sr_tcp_state_type _determine_state(struct sr_nat_connection *conn, sr_tcp_hdr_t *buf);
```


## List of tests cases run and results
### ICMP Echo Request/Reply
1. Pinging from the internal hosts to any external hosts.
```console
mininet> client1 ping -c3 server1
PING 172.64.3.21 (172.64.3.21) 56(84) bytes of data.
64 bytes from 172.64.3.21: icmp_seq=1 ttl=63 time=500 ms
64 bytes from 172.64.3.21: icmp_seq=2 ttl=63 time=21.9 ms
64 bytes from 172.64.3.21: icmp_seq=3 ttl=63 time=90.7 ms

--- 172.64.3.21 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2003ms
rtt min/avg/max/mdev = 21.926/204.334/500.355/211.193 ms
```
In Wireshark <br>
![alt text](/images/ICMP_echo_request_eth1.PNG "ICMP_echo_request_eth1") <br>
<b>Fig.1 - ICMP_echo_request_eth1</b>

![alt text](/images/ICMP_echo_request_eth2.PNG "ICMP_echo_request_eth2") <br>
<b>Fig.2 - ICMP_echo_request_eth2</b>
As you can see the red cirles in these two screenshots, after the ICMP request packet goes from eth1 to eth2, the IP address of client 1 has changed to the IP address of the router eth5. And the identifier has been changed from 3337 to 1024 (As 0-1023 are reserverd).
