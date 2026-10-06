# Structured Reflection - IE3010

## AI tools and stages
I used an AI assistant mainly during the planning and implementation stages of the NetMessenger project. It helped me interpret the assignment specification, calculate the personalised port and NID values, draft socket-programming structures, and identify areas that needed testing. I also used it to explain concepts such as TCP sockets, threads, message framing and file transfer.

## What AI did well and where it was misleading
The AI was useful for producing an initial structure quickly and for explaining standard C socket APIs. It also helped me notice that the assignment requires exact line framing and that file transfers must read exactly the declared number of bytes. However, AI-generated code cannot automatically be assumed to be correct. In particular, the receiver-side file-transfer framing is not fully specified by the brief, so I treated that part as an implementation assumption and must verify it through testing.

## What I changed, added or rejected
I checked the personalised values for my registration number, IT23861404. The last four digits are 1404, giving port 7404, and digits 3-6 of the numeric part are 8614, giving NID:8614. I also need to compile the code, test multiple simultaneous clients, test malformed commands and disconnects, and verify that transferred files are byte-for-byte unchanged. Any code that I cannot explain will be reviewed and rewritten before submission.

## What I learned
The project improved my understanding of TCP client-server communication, especially the difference between a socket connection and an application-level protocol. I learned that TCP is a byte stream, so one recv() call does not necessarily correspond to one complete command. Therefore, the server needs explicit newline framing for text commands and an exact byte count for file data. I also learned how shared server state such as users and rooms must be protected when multiple client threads access it concurrently. Finally, I learned that error handling and graceful disconnects are important parts of a network application, not optional extras.
