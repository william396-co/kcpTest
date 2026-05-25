# kcpTest

KCP test project with a custom UDP handshake and a small packet header used to distinguish handshake traffic from KCP payload traffic.

## Overview

This project uses UDP as the transport layer and KCP as the reliable protocol on top of it.

The main idea is:

- `Client` owns one UDP socket and one KCP session.
- `Server` owns one UDP listen socket and manages many logical `Connection` objects.
- Each server-side `Connection` stores one `conv` and one KCP control block.
- A custom packet header is used to tell handshake packets apart from real KCP data packets.

## Roles

### Client

The client is a single-peer endpoint:

- one `UdpSocket`
- one `ikcpcb*`
- one active `conv` after handshake succeeds

The client first sends a handshake request. After the server replies with an allocated `conv`, the client creates its KCP session and starts sending KCP data.

### Server

The server is a multi-client endpoint:

- one UDP listen socket
- a map of `conv -> Connection`
- one logical `Connection` per client session

The server receives all UDP packets on the same socket, decodes the packet header, and routes the packet by type and `conv`.

## Packet Format

The protocol header is defined in [src/util.h](D:/git-workspace/kcpTest/src/util.h).

```cpp
enum PacketType : uint8_t {
    PKT_HANDSHAKE_REQ = 1,
    PKT_HANDSHAKE_ACK = 2,
    PKT_KCP_DATA      = 3,
};

struct PacketHeader {
    uint32_t magic;
    uint32_t conv;
    uint8_t  type;
};
```

Packet meaning:

- `PKT_HANDSHAKE_REQ`: client asks the server to allocate a new `conv`
- `PKT_HANDSHAKE_ACK`: server replies with the allocated `conv`
- `PKT_KCP_DATA`: actual KCP segment payload

Associated helpers:

- `encode_packet(...)`
- `decode_packet(...)`

These helpers wrap outgoing data and parse incoming UDP data before the code decides whether the packet is handshake traffic or KCP traffic.

## Handshake Flow

Before KCP can be used, the client and server must agree on a `conv`.

```text
Client                                           Server
------                                           ------
send_shakehand()                                recv_work()
    |                                                |
    |-- PKT_HANDSHAKE_REQ, conv = 0 ---------------->|
    |                                                | decode packet
    |                                                | alloc_conv()
    |                                                | create Connection(conv)
    |<------------------ PKT_HANDSHAKE_ACK ----------|
    |                    conv = allocated_conv       |
    |                                                |
recv_shakehand(conv)                                |
ikcp_create(conv, socket)                           |
```

Handshake result:

- client learns the server-assigned `conv`
- server creates a `Connection` for that `conv`
- both sides are ready to exchange `PKT_KCP_DATA`

## Data Flow

### Client to Server

Application data goes through KCP first, then is wrapped into a UDP packet with a header.

```text
App data
  |
  v
Client::send(...)
  |
  v
ikcp_send(...)
  |
  v
KCP output callback
  |
  v
encode_packet(PKT_KCP_DATA, conv, kcp_bytes)
  |
  v
UDP send
  |
  v
Server::recv_work()
  |
  v
decode_packet(...)
  |
  v
find Connection by conv
  |
  v
Connection::recv(...)
  |
  v
ikcp_input(...)
  |
  v
ikcp_recv(...)
  |
  v
Application data
```

### Server to Client

The reverse direction is symmetric.

```text
Server-side app / Connection::send(...)
  |
  v
ikcp_send(...)
  |
  v
KCP output callback
  |
  v
encode_packet(PKT_KCP_DATA, conv, kcp_bytes)
  |
  v
UDP send to saved client ip:port
  |
  v
Client::recv_work()
  |
  v
decode_packet(...)
  |
  v
ikcp_input(...)
  |
  v
ikcp_recv(...)
  |
  v
Application data
```

## Connection Model

The client and server are intentionally different.

### Client connection model

```text
Client
 |- UdpSocket
 `- ikcp session
```

The client talks to exactly one remote endpoint and uses exactly one `conv`.

### Server connection model

```text
Server
 |- UdpSocket listen
 `- map<conv, Connection*>
    |- Connection(conv=1001)
    |- Connection(conv=1002)
    `- Connection(conv=1003)
```

Each `Connection` represents one logical KCP session:

- remote client address
- assigned `conv`
- one `ikcpcb*`

The server receives all packets on the shared UDP socket and dispatches them by decoded header.

## Server Routing Rules

The server should process packets like this:

1. Call `decode_packet(...)`
2. Check `magic`
3. Switch on `type`

Routing logic:

- `PKT_HANDSHAKE_REQ`
  Allocate a new `conv`, create a `Connection`, and send `PKT_HANDSHAKE_ACK`.

- `PKT_HANDSHAKE_ACK`
  Usually handled only by the client.

- `PKT_KCP_DATA`
  Use `conv` to find the matching `Connection`, then pass the payload into `ikcp_input()`.

## Important Design Point

Handshake traffic and real KCP traffic should never be distinguished by string content such as `"CONNECT"`.

They should be distinguished by:

- `magic`
- `type`
- `conv`

That keeps the protocol explicit, debuggable, and extendable.

## Files

Main files involved in the protocol:

- [client.cpp](D:/git-workspace/kcpTest/client.cpp)
- [server.cpp](D:/git-workspace/kcpTest/server.cpp)
- [client.h](D:/git-workspace/kcpTest/client.h)
- [server.h](D:/git-workspace/kcpTest/server.h)
- [src/connection.h](D:/git-workspace/kcpTest/src/connection.h)
- [src/connection.cpp](D:/git-workspace/kcpTest/src/connection.cpp)
- [src/util.h](D:/git-workspace/kcpTest/src/util.h)
- [src/udpsocket.h](D:/git-workspace/kcpTest/src/udpsocket.h)
- [src/udpsocket.cpp](D:/git-workspace/kcpTest/src/udpsocket.cpp)
