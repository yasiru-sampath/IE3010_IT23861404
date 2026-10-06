# IE3010 Design Diary — IT23861404

## Week 1 — Requirements and personalisation
I reviewed the NetMessenger specification and calculated the personalised values from registration number IT23861404. The numeric part is 23861404, so the last four digits are 1404 and the TCP port is 7404. Digits 3–6 are 8614, giving NID:8614. I created the required source, Makefile, log and storage naming convention.

## Week 2 — Server architecture
I selected a thread-per-client concurrency model because the assignment requires at least five simultaneous clients and each client can block while waiting for TCP input. Shared client and room state is protected with a pthread mutex. Logging has a separate mutex so concurrent threads do not corrupt the log file.

## Week 3 — Protocol implementation
I implemented the line-based commands REGISTER, LIST, BCAST, PMSG, JOIN, LEAVE, ROOMS, RMSG and QUIT. Every server response is generated through a common response function so the NID:8614 tag is consistently appended.

## Week 4 — File transfer
I implemented SENDFILE using the exact declared byte count. The server stores a copy in ./storage/IT23861404/<sender>/<filename> and forwards a FILE header followed by the exact raw bytes to the receiver. The receiver saves the file as received_<filename>.

## Week 5 — Error handling and disconnects
I added validation for registration, rooms, users, commands, file size and filenames. SIGPIPE is ignored so an unexpected peer disconnect does not terminate the server. Client removal also removes the client from every room.

## Testing and obstacles
The main networking issue considered was TCP stream framing: one recv() call is not guaranteed to return one complete command. The implementation therefore reads text commands byte-by-byte until newline and reads file payloads using an exact byte count. The file transfer test used 2066 bytes and the received SHA-256 matched the source file.

## Final preparation
I prepared the README, implementation report, prompt log, reflection and test summary. Before submission, I must run the program on the department Linux machine, capture genuine screenshots, create the required GitHub history with meaningful incremental commits, and verify that all evidence matches IT23861404.
