# CSCD58_NAT Project

## Team member
Guikang Zhong 1004304807 zhonggui<br>
Jingwei Wang 1003777754 wangj589<br>

## Acknowledgement
This project is build on Linda Lo's simple router code in Fall 2021.

## Contributions from each member of the team
Guikang Zhong:<br>
1.Built Nat Mapping<br>
2.Handled IP translation for ICMP case<br>
3.Handled IP translationfor TCP case<br>

Jingwei Wang:<br>
1.Handled IP translation for ICMP case<br>
2.Handled IP translationfor TCP case<br>
3.Handled TCP state trasition<br>

## Documentation for function implementing the required and missed functionalities in the starter code
We finished all the required functionality.<br>
1.The router can route packets between the Internet and the application servers.<br>
```
This functionality is implemented in below function in router/sr_router.c:
```
```C
void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{...}
```
2.The router correctly handles ARP requests and replies.<br>
```
In side the sr_handlepacket() there are two main cases, the first case is used to handle ARP request.
```
```C
void sr_handlepacket(...) {
        if (ethtype == ethertype_arp) {...}
        else if (ethtype == ethertype_ip) {...}
}
```
3.The router correctly handles traceroutes through it and to it.<br>
```
To handles traceroutes, we need to handle TCP/UDP and ICMP packets properly.
These packets are belong to IP packets, the second case of below function implemented these.
```
```C
void sr_handlepacket(...) {
        if (ethtype == ethertype_arp) {...}
        else if (ethtype == ethertype_ip) {...}
}
```
4.The router responds correctly to ICMP echo requests.<br>
```
In the case of a IP packet, if it is target to one of the router's interface and it's protocal is ICMP 
we check the icmp_type. If it's an icmp echo request (icmp_type == 8), we send back a icmp echo respose
(icmp_type == 0).
Note: construct_icmp_header() is the helper functions we defined to handle headers which locate at:
router/sr_router.c:
```
```C
void sr_handlepacket(...) {
        if (ethtype == ethertype_arp) {...}
        else if (ethtype == ethertype_ip) {
                ...
                if (target_if) {
                        ...
                        if (protocol == ip_protocol_icmp) {
                                ...
                                if (icmp_hdr->icmp_type == (uint8_t)8) {
                                        fprintf(stderr, "sending an ICMP echo response\n");
                                        uint16_t sum = icmp_hdr->icmp_sum;
                                        icmp_hdr->icmp_sum = 0;
                                        icmp_hdr->icmp_sum = cksum(icmp_hdr, len-sizeof(sr_ethernet_hdr_t)-sizeof(sr_ip_hdr_t));
                                        if (sum != icmp_hdr->icmp_sum) {
                                                fprintf(stderr, "Incorrect checksum\n");
                                                return;
                                        }

                                        /* construct icmp echo response */
                                        construct_icmp_header(packet, source_if, 0, 0, len);

                                        fprintf(stdout, "sending ICMP (type:0, code:0)\n");
                                        print_hdrs(packet, len);
                                        sr_send_packet(sr, packet, len, source_if->name);
                                }
                                
                        }
                        else if (protocol == ip_protocol_tcp || protocol == ip_protocol_udp) {...}
                } else {...}
        }
}
```
5.The router handles TCP/UDP packets sent to one of its interfaces by responding an ICMP port unreachable.<br>
```
In the case of a IP packet, if it is target to one of the router's interface and it's protocal is TCP or UDP, 
we send an ICMP unreachable (ICMP type 3 code 3).
```
```C
void sr_handlepacket(...) {
        if (ethtype == ethertype_arp) {...}
        else if (ethtype == ethertype_ip) {
                ...
                if (target_if) {
                        ...
                        if (protocol == ip_protocol_icmp) {...}
                        else if (protocol == ip_protocol_tcp || protocol == ip_protocol_udp) {
                                fprintf(stderr, "---------case2.1.2: tcp/udp ----------\n");
                                /* construct icmp echo response */
                                uint8_t *reply = construct_icmp_header(packet, source_if, 3, 3, len);
                                unsigned long new_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);

                                fprintf(stdout, "sending ICMP (Type 3, Code 3) unreachable\n");
                                sr_send_packet(sr, reply, new_len, source_if->name);
                                free(reply);
                        }
                } else {...}
        }
}
```
6.The router maintains an ARP cache whose entries are invalidated after a timeout period.<br>
```
When a packet is a IP packet and it does not destinate to one of the router's interfaces, we will first look up the cache.
If there is an valid entry in the cache, send the packet to the ip address.
If there is no entry for the distination or the entry is timeout, send an ARP request to the destination and put the packet to the arpcache_queue.
```
```C
void sr_handlepacket(...) {
        if (ethtype == ethertype_arp) {...}
        else if (ethtype == ethertype_ip) {
                ...
                if (target_if) {...} 
                else {
                        ...
                        /* send packet to next_hop_ip */
                        struct sr_arpentry *entry = sr_arpcache_lookup(&(sr->cache), ntohl(ip_hdr->ip_dst));
                        if (entry) {
                                /* use next_hop_ip->mac mapping in entry to send the packet
                                  free entry */
                                memcpy(ehdr->ether_dhost, entry->mac, ETHER_ADDR_LEN);
                                memcpy(ehdr->ether_shost, oif->addr, ETHER_ADDR_LEN);
                                sr_send_packet(sr, packet, len, oif_name);
                                free(entry);
                        }
                        else {
                                struct sr_arpreq *req = sr_arpcache_queuereq(&(sr->cache), ntohl(ip_hdr->ip_dst), packet, len, oif_name);
                                handle_arpreq(sr, req);
                        }
                }
        }
}
```
7.The router queues all packets waiting for outstanding ARP replies. If a host does not respond to 5 ARP requests, the queued packet is dropped and an ICMP host unreachable message is sent back to the source of the queued packet.<br>
```
This functionality is implemented in the handle_arpreq() function in router/sr_arpcache.c
```
```C
void handle_arpreq(struct sr_instance *sr, struct sr_arpreq *req) {
    time_t curtime = time(NULL);
    if (difftime(curtime, req->sent) > 1.0) {
        if (req->times_sent >= 5) {
            /* send icmp host unreachable to source addr of all pkts waiting
               on this request  */
            struct sr_packet *pkt;
            for (pkt=req->packets; pkt != NULL; pkt=pkt->next) {
                sr_ip_hdr_t *reply_ip_hdr = (sr_ip_hdr_t *)((pkt->buf + sizeof(sr_ethernet_hdr_t)));
                uint32_t ip_addr = reply_ip_hdr->ip_src;
                char *iname = get_interface_by_LPM(sr, ip_addr);
                struct sr_if *oif = sr_get_interface(sr, iname);
                struct sr_if *iif = sr_get_interface(sr, pkt->iface);
                /* construct icmp unreachable response */
                unsigned long icmp_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
                uint8_t *reply = construct_icmp_header(pkt->buf, iif, 3, 1, icmp_len);
                construct_eth_header(reply,((sr_ethernet_hdr_t *) pkt->buf)->ether_shost, oif->addr, ethertype_ip);
                fprintf(stdout, "sending ICMP (Type 3, Code 1) unreachable\n");
                sr_send_packet(sr, reply, icmp_len, iname);
            }
            sr_arpreq_destroy(&(sr->cache), req);
        }
        else {...}
   }
```
8.The router drop packets when the packet is smaller than the minimum length requirements, or the checksum is invalid, or when the router needs to send ICMP Type 3 or Type 11 messages back to sending hosts.<br>
```
If the packet is an ip packet and it does not destinate to one of the router's interfaces, check the TTL.
If the original TTL is 0, then new computed TTL is < 0, a ICMP type 11 code 0 will be sent out.
```
```C
void sr_handlepacket(...) {
        if (ethtype == ethertype_arp) {...}
        else if (ethtype == ethertype_ip) {
                ...
                if (target_if) {...} 
                else {
                        fprintf(stderr, "---------case2.2: to other place----------\n");
                        /* decrement TTL by 1 */
                        ip_hdr->ip_ttl = ip_hdr->ip_ttl - 1;
                        ip_hdr->ip_sum = 0;
                        ip_hdr->ip_sum = cksum(ip_hdr, sizeof(sr_ip_hdr_t));

                        /* Sent ICMP type 11 code 0, if an IP packet is discarded during processing because the TTL field is 0 */
                        if (ip_hdr->ip_ttl < 0) {
                                /* construct icmp echo response */
                                uint8_t *reply = construct_icmp_header(packet, source_if, 11, 0, len);
                                unsigned long new_len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
                                fprintf(stdout, "sending ICMP (Type 11, Code 0) unreachable\n");
                                sr_send_packet(sr, reply, new_len, source_if->name);
                                print_hdrs(reply, new_len);
                                free(reply);
                                return;
                        }
                        ...
                }
        }
}
```
9.The router guarantees on timeouts. If an ARP request is not responded to within a fixed period of time, the ICMP host unreachable message is generated and sent out.<br>
```
The sr_arpcache_sweepreqs() function in router/sr_arpcache.c will be called every seconds. 
And the handle_arpreq() will send an ICMP type 3 code 1 if an ARP requests are not responsed after it has been 5 times sent. 
Thus, a 5 sencond timeout has been enforced. 
```
```C
void sr_arpcache_sweepreqs(struct sr_instance *sr) { 
    /* Fill this in */
    struct sr_arpreq *req;
    for (req=sr->cache.requests; req != NULL; req=req->next) {
        handle_arpreq(sr, req);
    }
}
```
## List of tests cases run and results
1. Pinging from the client to any of the router's interfaces (192.168.2.1, 172.64.3.1, 10.0.1.1).
```console
mininet> client ping -c 3 192.168.2.1 
PING 192.168.2.1 (192.168.2.1) 56(84) bytes of data.
64 bytes from 192.168.2.1: icmp_seq=1 ttl=64 time=29.6 ms
64 bytes from 192.168.2.1: icmp_seq=2 ttl=64 time=9.45 ms
64 bytes from 192.168.2.1: icmp_seq=3 ttl=64 time=35.2 ms

--- 192.168.2.1 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2003ms
rtt min/avg/max/mdev = 9.445/24.732/35.198/11.052 ms
```
```console
mininet> client ping -c 3 172.64.3.1
PING 172.64.3.1 (172.64.3.1) 56(84) bytes of data.
64 bytes from 172.64.3.1: icmp_seq=1 ttl=64 time=39.9 ms
64 bytes from 172.64.3.1: icmp_seq=2 ttl=64 time=19.5 ms
64 bytes from 172.64.3.1: icmp_seq=3 ttl=64 time=40.6 ms

--- 172.64.3.1 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2004ms
rtt min/avg/max/mdev = 19.528/33.326/40.574/9.761 ms
```
```console
mininet> client ping -c 3 10.0.1.1
PING 10.0.1.1 (10.0.1.1) 56(84) bytes of data.
64 bytes from 10.0.1.1: icmp_seq=1 ttl=64 time=1.72 ms
64 bytes from 10.0.1.1: icmp_seq=2 ttl=64 time=34.0 ms
64 bytes from 10.0.1.1: icmp_seq=3 ttl=64 time=4.46 ms

--- 10.0.1.1 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2003ms
rtt min/avg/max/mdev = 1.718/13.379/33.965/14.599 ms
```
2. Tracerouting from the client to any of the router's interfaces
```console
mininet> client traceroute -n 192.168.2.1
traceroute to 192.168.2.1 (192.168.2.1), 30 hops max, 60 byte packets
 1  10.0.1.1  11.347 ms  22.432 ms  23.532 ms
```
```console
mininet> client traceroute -n 172.64.3.1
traceroute to 172.64.3.1 (172.64.3.1), 30 hops max, 60 byte packets
 1  10.0.1.1  49.727 ms  64.603 ms  65.291 ms
```
```console
traceroute to 10.0.1.1 (10.0.1.1), 30 hops max, 60 byte packets
 1  10.0.1.1  37.239 ms  49.942 ms  50.877 ms
```
3. Pinging from the client to any of the app servers (192.168.2.2, 172.64.3.10)
```console
mininet> client ping -c 3 server1
PING 192.168.2.2 (192.168.2.2) 56(84) bytes of data.
64 bytes from 192.168.2.2: icmp_seq=1 ttl=63 time=102 ms
64 bytes from 192.168.2.2: icmp_seq=2 ttl=63 time=69.5 ms
64 bytes from 192.168.2.2: icmp_seq=3 ttl=63 time=53.6 ms

--- 192.168.2.2 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2004ms
rtt min/avg/max/mdev = 53.640/75.032/101.915/20.087 ms
```
```console
mininet> client ping -c 3 server2
PING 172.64.3.10 (172.64.3.10) 56(84) bytes of data.
64 bytes from 172.64.3.10: icmp_seq=1 ttl=63 time=102 ms
64 bytes from 172.64.3.10: icmp_seq=2 ttl=63 time=26.1 ms
64 bytes from 172.64.3.10: icmp_seq=3 ttl=63 time=2.72 ms

--- 172.64.3.10 ping statistics ---
3 packets transmitted, 3 received, 0% packet loss, time 2003ms
rtt min/avg/max/mdev = 2.724/43.667/102.200/42.472 ms
```
4. Tracerouting from the client to any of the app servers
```console
mininet> client traceroute -n server1
traceroute to 192.168.2.2 (192.168.2.2), 30 hops max, 60 byte packets
 1  10.0.1.1  25.248 ms  33.581 ms  79.141 ms
 2  * * *
 3  * * *
 4  * * *
 5  192.168.2.2  183.733 ms  184.227 ms  184.681 ms
```
```console
mininet> client traceroute -n server2
traceroute to 172.64.3.10 (172.64.3.10), 30 hops max, 60 byte packets
 1  10.0.1.1  35.903 ms  42.843 ms  88.390 ms
 2  * * *
 3  * * *
 4  * * *
 5  172.64.3.10  163.734 ms  164.343 ms  164.921 ms
```
5. Downloading a file using HTTP from one of the app servers
```console
mininet> client wget http://192.168.2.2
--2021-10-28 15:35:37--  http://192.168.2.2/
Connecting to 192.168.2.2:80... connected.
HTTP request sent, awaiting response... 200 OK
Length: 161 [text/html]
Saving to: 'index.html.1'

index.html.1        100%[===================>]     161  --.-KB/s    in 0s

2021-10-28 15:35:37 (75.9 MB/s) - 'index.html.1' saved [161/161]
```
```console
mininet> client wget http://172.64.3.10
--2021-10-28 15:36:09--  http://172.64.3.10/
Connecting to 172.64.3.10:80... connected.
HTTP request sent, awaiting response... 200 OK
Length: 161 [text/html]
Saving to: 'index.html.2'

index.html.2        100%[===================>]     161  --.-KB/s    in 0s

2021-10-28 15:36:09 (74.8 MB/s) - 'index.html.2' saved [161/161]
```

6. Destination net unreachable (type 3, code 0)
```console
mininet> client ping -c 3 12.12.12.12
PING 12.12.12.12 (12.12.12.12) 56(84) bytes of data.
From 10.0.1.1 icmp_seq=1 Destination Net Unreachable
From 10.0.1.1 icmp_seq=2 Destination Net Unreachable
From 10.0.1.1 icmp_seq=3 Destination Net Unreachable

--- 12.12.12.12 ping statistics ---
3 packets transmitted, 0 received, +3 errors, 100% packet loss, time 2004ms
```
![alt text](/image/Type3Code0Wireshark.PNG "Wireshark Capture 2")

7.Destination host unreachable (type 3, code 1)
```console
mininet> client ping -c 3 192.168.2.3
PING 192.168.2.3 (192.168.2.3) 56(84) bytes of data.
From 192.168.2.1 icmp_seq=3 Destination Host Unreachable
From 192.168.2.1 icmp_seq=2 Destination Host Unreachable
From 192.168.2.1 icmp_seq=1 Destination Host Unreachable

--- 192.168.2.3 ping statistics ---
3 packets transmitted, 0 received, +3 errors, 100% packet loss, time 2033ms
pipe 3
```
We inserted a routing entry with IP address 192.168.2.3 in the routing table, which does not exist in the network.
![alt text](/image/rtable.PNG "Wireshark Capture 3")<br>
After the router sent five ARP requests, the Wireshark captured the Type3 Code1 messages from the router.
![alt text](/image/Type3Code1Wireshark.PNG "Wireshark Capture 4")

8.Port unreachable (type 3, code 3)
```console
mininet> client traceroute -n 192.168.2.1
traceroute to 192.168.2.1 (192.168.2.1), 30 hops max, 60 byte packets
 1  10.0.1.1  15.341 ms  29.956 ms  31.024 ms
```
The wireshark captured the (type 3, code 3) messages
![alt text](/image/Type3Code3Wireshark.PNG "Wireshark Capture 5")
