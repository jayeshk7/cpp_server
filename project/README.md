
I am using cpp-httplib
`This library uses 'blocking' socket I/O. If you are looking for a library with 'non-blocking' socket I/O, this is not the one that you want.`

So when we do read() or recv() - this will wait here until there is something to read. The CPU does nothing during this time. For non-blocking socket these operations will return immediately and you will have to keep checking (or use some polling mechanism to know there is data to read from this socket).

Even accept() can be made non blocking. This can be done by doing epoll_wait() on the sockfd to which server is connected to. if that socket is getting some data that means some client is trying to connect. If any other socket is getting data that means it is probably some socket which has already been accepted as a connection (accept() returns a new sockfd). So both read() and accept() are now non blocking



