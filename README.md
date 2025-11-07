## (toy 🧸) CPP server

### Installation steps for Ubuntu

You will require Postgres and libpq. To install these, do:

```
$ sudo apt install libpq-dev postgresql postgresql-contrib
```

I am using [cpp-httplib](https://github.com/yhirose/cpp-httplib) and as mentioned in their README - `This library uses 'blocking' socket I/O. If you are looking for a library with 'non-blocking' socket I/O, this is not the one that you want.`. See NOTES below to know more about this.

To compile the server, do:
`$ make server`
If this fails, it can happen if the directory where the Postgres headers and libpq are installed in your machine are different. In that case, get the output of these commands:
```
$ pg_config --includedir
Sample output ----> /usr/include/postgresql
$ pg_config --libdir
Sample output ----> /usr/lib/x86_64-linux-gnu
```
Assuming your output is same as the sample outputs shown above, you then substitute the path in `CPPFLAGS+=-I/<path> .... ` in the Makefile with `/usr/include/postgresql`. 
And the path in `CPPFLAGS+= ... -L/<path> ...` in the Makefile with `/usr/lib/x86_64-linux-gnu`. Now run `make server` again.

Reference - [Official libpq docs](https://www.postgresql.org/docs/current/libpq-build.html)

---

### Load Testing

There are two types of client request simulation programs:
* `read_client.cpp` emulates CPU-bound workload by making GET requests for keys which will be present in a kv-cache (which is an `std::unordered_map`)
* `write_client.cpp` emulated disk-bound workload by making POST requests to insert new keys into the database.

---

### NOTES

When we do a `read()` or `recv()` syscall - it waits until there is something to read. The CPU does nothing during this time. For non-blocking socket these operations will return immediately and you will have to keep checking (use some polling mechanism to know when there is data to read from this socket).

Even `accept()` can be made non blocking. This can be done by doing `epoll_wait()` on the sockfd to which server is connected to. if that socket is getting some data that means some client is trying to connect. If any other socket is getting data that means it is probably some socket which has already been accepted as a connection (`accept()` returns a new sockfd). So both `read()` and `accept()` are now non blocking.

---

### TODO

- [ ] Each api opening a new connection to DB is slow. for each conn, you will have to do tcp + auth. Implement connection pooling.
- [x] Look up RAII. Using mutex.lock()/unlock() can fail if the code within the lock throws an exception leading to lock never getting released => deadlock.
- [ ] Implement reader writer lock.
- [ ] Add proper access logging (to measure request service time) and error logging in server. Also add error handler and exception handler.
- [ ] Experiment with different threadpool sizes for server (default = 8) and max_queued_requests. Can take as user input.
- [ ] Add a /metrics API which returns all the required metrics after a load test is completed.

