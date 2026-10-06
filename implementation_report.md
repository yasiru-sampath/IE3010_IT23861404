# IE3010 Network Programming - Implementation Report
## NetMessenger: Multi-Client Chat and File-Sharing Platform over TCP/IP

**Registration Number:** IT23861404  
**Port:** 7404  
**NID:** 8614  
**Server:** server_1404.c  
**Client:** client_1404.c  
**Makefile:** Makefile_1404  
**Log:** netmsg_IT23861404.log  
**Storage:** ./storage/IT23861404/  

## 1. Introduction

NetMessenger is a TCP/IP client-server chat and file-sharing application implemented in C using the BSD socket API. The server accepts multiple clients concurrently. Registered clients can broadcast messages, send private messages, use rooms, list users/rooms, and transfer files through the server.

## 2. Personalisation Calculation

Numeric registration part: **23861404**.

- Last four digits = 1404.
- Listening port = 6000 + 1404 = **7404**.
- Digits 3-6 = 8614, therefore **NID:8614**.
- Server source = **server_1404.c**.
- Client source = **client_1404.c**.
- Makefile = **Makefile_1404**.
- Log = **netmsg_IT23861404.log**.
- Storage root = **./storage/IT23861404/**.
- Submission ZIP = **IE3010_IT23861404.zip**.

## 3. Architecture

The architecture consists of one TCP server and multiple TCP clients. The server listens on port 7404 and creates a dedicated POSIX thread for each connected client. Shared user and room state is protected by a mutex.

**Architecture diagram to insert:**

Client A ─┐
Client B ─┼── TCP/7404 ──> NetMessenger Server ──> personalised storage/log
Client C ─┘

### Why threads?

A thread-per-client model is straightforward for this assignment. Each client can block waiting for TCP data without stopping other clients. A mutex protects the shared client list and room list.

## 4. Protocol Implementation

The implementation supports REGISTER, LIST, BCAST, PMSG, JOIN, LEAVE, ROOMS, RMSG, SENDFILE and QUIT. Text commands are newline terminated. The server appends `NID:8614` to its OK and ERR responses.

For SENDFILE, the server reads exactly the declared file size from the TCP stream and stores a copy below the personalised storage directory.

### Receiver-side file framing assumption

The assignment explicitly defines the sender-to-server SENDFILE command and raw bytes, but it does not specify a receiver-side notification format. This implementation therefore uses:

`FILE <sender> <filename> <filesize>\n`

followed immediately by exactly `<filesize>` raw bytes. This allows the receiving client to identify the file and read the exact byte count.

## 5. Functional Features

### Registration and presence
A client must first register a unique username. Duplicate usernames receive `ERR 001 USERNAME_TAKEN NID:8614`.

### Broadcast
BCAST sends a message to all other connected clients.

### Private messaging
PMSG targets one registered username. An unknown user produces `ERR 002 USER_NOT_FOUND`.

### Rooms
JOIN creates or joins a room. LEAVE removes the client. RMSG delivers the message only to room members.

### File sharing
Files are received by the server, stored under the sender's personalised directory and forwarded to the requested user or room.

### Disconnect handling
The server removes disconnected clients from the client list and all rooms. It is designed to survive unexpected client termination without crashing.

## 6. Error Handling

Invalid commands, missing registration, invalid users/rooms, oversized files and file-storage failures return structured ERR responses. SIGPIPE is ignored so a disconnected receiving socket does not terminate the whole server process.

## 7. Testing Plan

| Test | Expected result | Actual result |
|---|---|---|
| Start server | Server listens on TCP 7404 | Fill after test |
| Register unique user | REGISTERED response with NID:8614 | Fill after test |
| Duplicate username | USERNAME_TAKEN | Fill after test |
| LIST | All connected users returned | Fill after test |
| BCAST | Other clients receive message | Fill after test |
| PMSG | Only target receives message | Fill after test |
| JOIN/LEAVE | Room membership changes | Fill after test |
| RMSG | Room members receive message | Fill after test |
| ROOMS | Current rooms returned | Fill after test |
| SENDFILE | File stored and forwarded unchanged | Fill after test |
| Invalid user | USER_NOT_FOUND | Fill after test |
| Invalid room | ROOM_NOT_FOUND | Fill after test |
| QUIT | BYE response and clean close | Fill after test |
| Kill client process | Server removes client and continues | Fill after test |

## 8. Evidence/Screenshots Required

Insert genuine screenshots from your own machine for:
1. Server starting on port 7404.
2. `ss -tlnp` showing port 7404.
3. Two or more clients connected.
4. REGISTER response showing NID:8614.
5. Broadcast message.
6. Private message.
7. Room creation/join/message/leave.
8. Successful file transfer.
9. Personalised storage directory containing the file.
10. Log file containing timestamped events.
11. An invalid command/error response.
12. Unexpected client disconnect.

## 9. Conclusion

The implementation provides the required multi-client TCP chat and file-sharing functionality while applying the registration-number personalisation requirements. Before submission, all functionality must be compiled and tested on the department's Linux environment, and genuine screenshots and test results must be inserted.
