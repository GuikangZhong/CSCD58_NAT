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
3. To test whether NAT can handle the "simulataneous open" case, we added an external peer (client3) into the topology.<br>
Please refer to the topology below instead of the one in the NAT handout<br>

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

## List of tests cases run and results
