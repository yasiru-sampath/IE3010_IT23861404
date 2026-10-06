# IE3010 NetMessenger - IT23861404

## Personalisation
- Registration number: IT23861404
- Numeric part: 23861404
- Last four digits: 1404
- TCP port: 6000 + 1404 = 7404
- NID: digits 3-6 of 23861404 = 8614
- Server source: server_1404.c
- Client source: client_1404.c
- Makefile: Makefile_1404
- Log: netmsg_IT23861404.log
- Storage: ./storage/IT23861404/<sender>/<filename>
- ZIP: IE3010_IT23861404.zip

## Build
```bash
make -f Makefile_1404
```

## Run server
```bash
./server_1404
```

## Run clients (separate terminals)
```bash
./client_1404 127.0.0.1 alice
./client_1404 127.0.0.1 bob
```

## Commands
REGISTER, LIST, BCAST, PMSG, JOIN, LEAVE, ROOMS, RMSG, SENDFILE, QUIT.

## File transfer
The sender command is:
`SENDFILE <target> <filename> <filesize>`
The server stores the original file under the personalised storage path and forwards a FILE header followed by exactly the declared raw bytes to the receiving client. The client saves it as `received_<filename>`.

## Note
The assignment specifies the sender-to-server SENDFILE framing but does not explicitly define a receiver-side file notification framing. This implementation uses `FILE <sender> <filename> <filesize>\n` followed immediately by raw bytes as the receiver-side framing so the client can reconstruct the complete file. This should be described as an implementation assumption in the report.
