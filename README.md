# l3roamd

l3roamd, pronunced *ɛl θriː ɹoʊm diː*, is a core part of layer 3 mesh networks.
At first it will be built to work with [babeld](https://github.com/jech/babeld).

integrating tightly with mac80211 and netlink, l3roamd will be doing the following things:
 - monitor the presence of clients
 - allow network-wide search for IP-addresses
 - manage distribution of prefixes across the mesh (for router advertisements) [RA]
 - monitor babeld for duplicate host routes that this node also announces
 
Ideally, I'd like to split this up into multiple daemons communicating using standardized protocols:

## Managing clients

l3roamd will directly monitor a set of interfaces for clients.
On the mac80211/fdb layer it will monitor clients and act whenever a new client appears (query the database or create an entry)
or when a client disappears. 
In case of a disappearing client a node removes all host routes for that client. It does
not yet forget about them completely in case the client re-appears later.
The routes presence in the routing table is controlled by the presence of the client (subject to some timeout).
The routes presence in the database is subject to the timeout of the IPs lifetime (as was announced by the RA).
In worst case two nodes may have to switch routes fast and repeatably due to a client having bad connectivity
to either node. The whole cycle should be
These nodes may not be connected directly.

 
## [DB] Distributed Database
 

*Does anyone know a client with this $MAC and if so, what IPs did it use?*

This information is available by querying a special node-client-IP that is
calculated based on any given clients mac address. This address is assigned to
the loopback-interface of the node serving the client.

Using this address, a node can be notified to drop one of its local 
clients, release all host routes and send an info packet to the 
requesting node.
The node requesting this drop will parse the info-packet, extract the
IP-addresses used by this client and immediately adjust the 
corresponding
routes.

### Data stored about clients

 - MAC
 - a set of IPs
 - (optional) a set of multicast groups

## [RA] Router Advertisements and Prefix Management

*THIS PART IS NOT IMPLEMENTED YET AND LIKELY WILL NEVER BE INSIDE L3ROAMD*
Instead this should be implemented in a separate daemon called prefixd.

Any node should be able to announce a prefix (a /64) to be used by clients.
This must be announced both within l3roamd and as a default route with a source prefix (set to the announced prefix!)
through babeld.
A metric (e.g. uplink bandwidth, reliability, ...) should be included, too.
Nodes should announce a small subset of prefixes from nearby uplinks (actually, metric based) to clients via normal
router advertisements.
Lifetime of these prefixes should be managed such that clients always use the best uplink available.
This is where early loadbalancing can reasonably take place.
Clients are expected to cope with changing prefixes.
Clients are also expected to hold onto deprecated prefixes as long as active connections require it.
Routing for all, even deprecated, prefixes will be maintained as long as reasonably possible to avoid breaking a clients (TCP) connection.
Multiple default routes for prefixes may be common (think multi homed AS), in this case loadbalancing is delegated to babeld.
This means, that multiple nodes will announce the same set of prefixes with possibly different metrics.
l3roamd will manages the prefixes it announces to a client on a per-client basis, if possibly.
I.e. it will actively deprecate prefixes of clients it deems unreliably.
This is likely to happen during roaming longer distances when a completely different set of uplinks should be used.
As stated before, this will not break active connections.

## IPv4?
IPv4-Clients are supported as well. Inside the mesh there is only 
ipv6, so we need clat on the node and plat somewhere else on the net. We 
support using a single exit at the moment and are working on 
multi-exit-support for plat.

## Improvements welcome!

If you can improve this specifications (typos, better wording, restructering, ...) or even new important aspects, feel free to open
a pull request. Please prefix your commits with "README: $my message" and try to summarize the changes in the commit
message even if the commit message turns out to be longer than the change. Say, if you change a singel word, write a message like

    README: corrected singel to single
    
    This corrects a typo in the "Improvements welcome!" section.

This approach makes reviewing and reasoning about changes a lot easier.

# Intercom Packets

There are currently three packet types used by l3roamd:

- SEEK,
- CLAIM,
- INFO

SEEK are usually sent as multicast while CLAIM and INFO are sent as 
unicast.

Each packet consists of a common header structure:
```
0        7        15       23       31
+--------+--------+--------+--------+
|  TTL   | nonce1 | nonce2 | nonce3 |
+-----------------------------------+
| nonce4 |  type  |sender1 |sender2 |
+-----------------------------------+
|sender3 |sender4 |sender5 |sender6 |
+-----------------------------------+
|sender7 |sender8 |sender9 |sender10|
+-----------------------------------+
|sender11|sender12|sender13|sender14|
+-----------------------------------+
|sender15|sender16|
+-----------------+
```
TTL    - this is decremented whenever a multicast-packet is forwarded.
nonce  - this is a random number that is used to identify duplicate packets and drop them
type   - this is the packet-type, one of INTERCOM_SEEK, INTERCOM_CLAIM, INTERCOM_INFO
sender - ipv6-address of the sender of the packet

## SEEK
The seek operation is sent to determine where a client having a specific 
IP address is connected. This triggers local neighbor discovery 
mechanisms. SEEK-packets have the following structure:

addr contains the unknown ipv6-address.
```
0        7        15       23       31
+--------+--------+--------+--------+
|  TTL   | nonce1 | nonce2 | nonce3 |
+-----------------------------------+
| nonce4 |  type  | sender1| sender2|
+-----------------------------------+
|sender3 |sender4 |sender5 |sender6 |
+-----------------------------------+
|sender7 |sender8 |sender9 |sender10|
+-----------------------------------+
|sender11|sender12|sender13|sender14|
+-----------------------------------+
|sender15|sender16| addr1  | addr2  |
+-----------------------------------+
| addr3  | addr4  | addr5  | addr6  |
+-----------------------------------+
| addr7  | addr8  | addr9  | addr10 |
+-----------------------------------+
| addr11 | addr12 | addr13 | addr14 |
+-----------------------------------+
| addr15 | addr16 |
+-----------------+
```
## CLAIM
When a client connects to a node, this node sends a claim to the special 
node-client IP-address via unicast. So whichever node was the previous 
AP for this client will receive this message, drop all host-routes for 
this client, drop the node-client-IP and respond with an INFO-message
CLAIM-packets have the following structure:
```
0        7        15       23       31
+--------+--------+--------+--------+
|  TTL   | nonce1 | nonce2 | nonce3 |
+-----------------------------------+
| nonce4 |  type  | sender1| sender2|
+-----------------------------------+
|sender3 |sender4 |sender5 |sender6 |
+-----------------------------------+
|sender7 |sender8 |sender9 |sender10|
+-----------------------------------+
|sender11|sender12|sender13|sender14|
+-----------------------------------+
|sender15|sender16|  MAC1  |  MAC2  |
+-----------------------------------+
|  MAC3  |  MAC4  |  MAC5  |  MAC6  |
+-----------------------------------+
```
MAC - is the mac-address of the client being claimed.

## INFO
This packet contains all IP-addresses being in active use by a given client. It 
will be sent in response to CLAIM via unicast.
INFO-packets have the following structure:
```
0        7        15       23       31
+--------+--------+--------+--------+
|  TTL   | nonce1 | nonce2 | nonce3 |
+-----------------------------------+
| nonce4 |  type  | sender1| sender2|
+-----------------------------------+
|sender3 |sender4 |sender5 |sender6 |
+-----------------------------------+
|sender7 |sender8 |sender9 |sender10|
+-----------------------------------+
|sender11|sender12|sender13|sender14|
+-----------------------------------+
|sender15|sender16|relinq. |  MAC1  | 
+-----------------------------------+
|  MAC2  |  MAC3  |  MAC4  |  MAC5  |
+-----------------------------------+
|  MAC6  | #addr  |addr1_1 |addr1_2 |
+-----------------------------------+
|addr1_3 |addr1_4 |addr1_5 |addr1_6 |
+-----------------------------------+
|addr1_7 |addr1_8 |addr1_9 |addr1_10|
+-----------------------------------+
|addr1_11|addr1_12|addr1_13|addr1_14|
+-----------------------------------+
|addr1_15|addr1_16|addr2_1 |addr2_2 |
+-----------------------------------+
|addr2_3 |addr2_4 |addr2_5 |addr2_6 |
+-----------------------------------+
|addr2_7 |addr2_8 |addr2_9 |addr2_10|
+-----------------------------------+
|addr2_11|addr2_12|addr2_13|addr2_14|
+-----------------------------------+
|addr2_15|addr2_16|addr#_1 |addr#_2 |
+-----------------------------------+
|addr#_3 |addr#_4 |addr#_5 |addr#_6 |
+-----------------------------------+
|addr#_7 |addr#_8 |addr#_9 |addr#_10|
+-----------------------------------+
|addr#_11|addr#_12|addr#_13|addr#_14|
+-----------------------------------+
|addr#_15|addr#_16|
+-----------------+
```
MAC is the mac-address of the client
#addr is the amount of client-6ipv6-addresses in the packet. There is a 
compile-time flag having a default of 32
addr1-addr# are 1-n ipv6 addresses.
