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
#### Pinging from the internal hosts to any external hosts in NAT mode
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
<div align="center"><b>Fig.1 - client1's echo request to server1 at eth1</b></div> <br>

![alt text](/images/ICMP_echo_request_eth2.PNG "ICMP_echo_request_eth2") <br>
<div align="center"> <b>Fig.2 - client1's echo request to server1_eth2</b></div> <br>
As you can see the red cirles in these two screenshots, after the ICMP request packet goes from eth1 to eth2, the IP address of client 1 has changed to the IP address of the router eth5. And the identifier has been changed from 3337 to 1024 (As 0-1023 are reserverd). <br><br>

Same thing for client 2: <br>
```console
mininet> client2 ping -c3 server1
PING 172.64.3.21 (172.64.3.21) 56(84) bytes of data.
64 bytes from 172.64.3.21: icmp_seq=1 ttl=63 time=790 ms
64 bytes from 172.64.3.21: icmp_seq=2 ttl=63 time=9.95 ms
64 bytes from 172.64.3.21: icmp_seq=3 ttl=63 time=73.4 ms

--- 172.64.3.21 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2002ms
rtt min/avg/max/mdev = 9.948/291.098/789.954/353.693 ms
```
In Wireshark <br>
![alt text](/images/client2_ICMP_echo_request_eth4.PNG "client2_ICMP_echo_request_eth4") <br>
<div align="center"><b>Fig.3 - client2's echo request to server1 at eth4</b></div> <br>

![alt text](/images/client2_ICMP_echo_request_eth4.PNG "client2_ICMP_echo_request_eth2") <br>
<div align="center"> <b>Fig.4 - client2's echo request to server1 at eth2</b></div> <br>

#### Pinging from the external hosts to any internal hosts in NAT mode
```console
mininet> client3 ping -c3 client1
PING 10.0.1.100 (10.0.1.100) 56(84) bytes of data.

--- 10.0.1.100 ping statistics ---
3 packets transmitted, 0 received, 100% packet loss, time 2009ms
```
Enternal hosts can not directly ping any internal host, packets lost as expected. <br>

### TCP Connections
#### Open a TCP connection from client2 to server1 in NAT mode
```console
mininet> client2 wget http://172.64.3.22
--2021-12-04 14:50:26--  http://172.64.3.22/
Connecting to 172.64.3.22:80... connected.
HTTP request sent, awaiting response... 200 OK
Length: 161 [text/html]
Saving to: 'index.html.15'

index.html.15       100%[===================>]     161  --.-KB/s    in 0s

2021-12-04 14:50:28 (97.0 MB/s) - 'index.html.15' saved [161/161]
```
In Wireshark <br>
![alt text](/images/client2_tcp_conn_eth4.PNG "client2_tcp_conn_eth4") <br>
<div align="center"><b>Fig.5 - client2's TCP connection to server2 at eth4</b></div> <br>

![alt text](/images/client2_tcp_conn_eth3.PNG "client2_tcp_conn_eth3") <br>
<div align="center"> <b>Fig.6 - client2's TCP connection to server2 at eth3</b></div> <br>
As you can see, client 2 uses its IP address 10.0.1.101 and port number 49478 to send TCP packets. In the perspective of server2, these incoming packets are from IP 172.64.3.2, Port 1024. Same thing happens when server2 sends responses back to client2, the destination IP and port are changed after NAT processes them. This means our NAT successfully rewrites TCP packets from internal hosts to external hosts, and vice versa. <br>

#### Open a TCP connection from client3 to directly client1 in NAT mode
```console
mininet> client3 ssh client1
ssh: connect to host 10.0.1.100 port 22: No route to host
```
Again, external hosts cannot directly open an connection with any internal host, ICMP host unreachable will be sent.<br>

![alt text](/images/icmp_t3c1.PNG "icmp_t3c1") <br>
<div align="center"> <b>Fig.7 - client 3 receives an ICMP Host unreachable error</b></div> <br>

#### Open a TCP connection from client3 to NAT's external port (simultaneous-open)
```console
mininet> client3 ssh 172.64.3.3
ssh: connect to host 172.64.3.3 port 22: Connection refused
```
req1: Your NAT MUST NOT respond to an unsolicited inbound SYN packet for at least 6 seconds after the packet is received. <br>

req2: If during this interval the NAT receives and translates an outbound SYN for the connection the NAT MUST silently drop the original unsolicited inbound SYN packet.<br>

req3: Otherwise, the NAT MUST send an ICMP Port Unreachable error (Type 3, Code 3) for the original SYN. <br>

![alt text](/images/tcp_syn_timeout.PNG "tcp_syn_timeout") <br>
<div align="center"> <b>Fig.8 - client 3 receives an ICMP Type 3 Code 3 error after around six seconds</b></div> <br>

### Mappings
We use a mapping which has four columns: Internal IP address, internal identifier (identifier for ICMP, port for TCP), external identifier, and mapping type.<br>
Below mapping table was captured from the log when we run "client1 wget http://172.64.3.21"<br>
```console
IP_INT       aux_int       aux_ext          type
-----------------------------------------------------------
0a000164       3236          1030            0
0a000164       3232          1029            0
0a000165       3228          1028            0
0a000165       3224          1027            0
0a000165       3219          1026            0
0a000165       3215          1025            0
0a000165       3211          1024            0
```
When assigning a port to a mapping, we avoid to use the well-known ports (0-1023). Thus, our TCP ports or ICMP identifiers are from (1024-65535) and we had used bitmap to track the avliable numbers.<br>
Also, our mappings is "Endpoint Independent". In our mapping table the (IP_INT, aux_int) pair or the aux_ext can uniquely indentify a row in the table which is independent with the ip of the external host.<br><br>

### Cleaning up defunct mappings
#### ICMP Case
First we run "client2 ping -c1 server1", and captured the below log<br>
``` console
IP_INT       aux_int       aux_ext          type
-----------------------------------------------------------
0a000165       3098          1024            0
[NAT]: packet arrived! 
172.64.3.21
[NAT]: external -> internal
[NAT]: translated public ip to privite ip for ICMP
Forwarding packet ... 
Sending an arp message:
Generating arp packet 
Wrapping in ethernet frame 
Current local time and date: Sun Dec  5 01:36:04 2021
*** -> Received packet of length 42 
Confirmed destination of following frame 
Received the following ARP packet: 
Wrapping in ethernet frame 
Current local time and date: Sun Dec  5 01:36:08 2021
*** -> Received packet of length 42 
Confirmed destination of following frame 
Received the following ARP packet: 
Sending an arp message:
Generating arp packet 
Wrapping in ethernet frame 
Current local time and date: Sun Dec  5 01:36:08 2021
*** -> Received packet of length 42 
Confirmed destination of following frame 
Received the following ARP packet: 
Sending an arp message:
Generating arp packet 
Wrapping in ethernet frame 
[NAT]: ICMP query timeout, clean mapping of id 1024
```
Note on the top of the log, it created a mapping with aux_ext 1024. After 60 seconds (the defualt time out value) the last line was printed out, indicated that the mapping was timeout and got cleaned.<br>

#### TCP Case
Then we run "client1 wget http://172.64.3.21", and captured the below log<br>
``` console
IP_INT       aux_int       aux_ext          type
-----------------------------------------------------------
0a000164       54350          1024            1
========[NAT]: TCP packet arrived! =======
[NAT]: external -> internal
updating connection: external -> internal
[update_connection] foud a map (inbound)
[state]:
CLOSED
[NAT]: translated public ip to privite ip for TCP
Forwarding packet ... 
Wrapping in ethernet frame 
TCP packet to router was forwarded!
[NAT]: TCP mapping timeout, clean mapping of id 1024
Current local time and date: Sun Dec  5 00:52:44 2021
*** -> Received packet of length 98 
Confirmed destination of following frame 
Confirmed integrity of following packet:
IP header:
	version: 4
	header length: 5
	type of service: 0
	length: 84
	id: 59998
	fragment flag: DF
	fragment offset: 0
	TTL: 64
	protocol: 1
	checksum: 33337
	source: 10.0.1.100
	destination: 10.0.1.101

IP_INT       aux_int       aux_ext          type
-----------------------------------------------------------
```
Note on the top of the log, a mapping was created for the TCP transmission with the same aux_ext number 1024 again, which means the number 1024 was released along with the clean of mapping from above execution. <br>
After the close TCP sessions, the connection's state changed to CLOSED. Since there is no valid (valid menas non-closed and non-timeout) connention in the mapping, the mapping got cleaned.<br>
The last two lines of the log can verify that the mapping was indeed cleaned.<br><br>

## Adding command-line flags
1. -n -- Enable NAT functionality
2. -I INTEGER -- ICMP query timeout interval in seconds (default to 60)
3. -E INTEGER -- TCP Established Idle Timeout in seconds (default to 7440)
4. -E INTEGER -- TCP Established Idle Timeout in seconds (default to 7440)

The first flag was already added in the starter code and we have added the reset 3 flags. <br>
In main() at sr_main.c below cases were extened in the end of the switch statement.<br>
```C
            case 'I':
                icmp_query_to = atoi(optarg);
            case 'E':
                tcp_estab_idle_to = atoi(optarg);
            case 'R':
                tcp_transitory_to = atoi(optarg);
```
In sr_nat_init() at sr_nat.c, these timeout values get stored. <br>
```C
  /* Initialize any variables here */
  nat->icmp_query_to = icmp_query_to;
  nat->tcp_estab_idle_to = tcp_estab_idle_to;
  nat->tcp_transitory_to = tcp_transitory_to;
```
If these flags were not used the defult values defined in sr_nat.h will be used. 
```C
    unsigned int icmp_query_to = DEFAULT_ICMP_QUERY_TO;	
    unsigned int tcp_estab_idle_to = DEFAULT_TCP_ESTABLISHED_TO;
    unsigned int tcp_transitory_to = DEFAULT_TCP_TRANSITORY_TO;
```
## Untested Functionality
1. We were unable to find a proper set of TCP commands to test a successful simultaneous open case where "If during this interval the NAT receives and translates an outbound SYN for the connection the NAT MUST silently drop the original unsolicited inbound SYN packet". We wrote codes for such case in nat_handle_tcp function in sr_router.c <br>
```C
int nat_handle_tcp(struct sr_instance* sr, uint8_t *ip_packet, unsigned int ip_packet_len, int is_to_nat) {
  ..........
  else if (is_to_nat == 1) {
    printf("[NAT]: external -> internal\n");
    /* check whether it is unsolicited inbound (external -> internal) SYN. If it is, wait for six seconds.
     * During this interval, if the NAT receives and translate outbound (internal -> external) SYN,
     * the NAT should silently drop the inbound SYN */
    if (tcp_header->SYN == 1 && tcp_header->ACK == 0) {
      printf("[NAT]: inboud SYN\n");
      /* if mapping is valid, check its list of connections*/
      mapping = sr_nat_lookup_external(&sr->nat, ntohs(tcp_header->dst_port), nat_mapping_tcp);
      if (mapping) {
        sr_nat_update_connection(&sr->nat, mapping, (uint8_t *)ip_packet, ip_packet_len, 1);
        return -1;
      }
      /* if no mapping, insert this SYN packet into the unsolicitied packet list */
      else {
        sr_nat_insert_unsolicited_packet(&(sr->nat), (uint8_t *)ip_packet, ip_packet_len);
        return -1;
      }
    }
   .............
```
2. We were able to find a proper set of TCP commands to test TCP Established connection idle-timeout and Transitory connection idle-timeout, but we wrote code for such cases in sr_nat_timeout function in sr_nat.c <br>
```C
........
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
.........
```
