# huio
A toy asychronized-io and non-blocking io library.

The asychronized part can successfully run on Ubuntu14.10 compiled by g++4.9.1  and the non-blocking part is not very reliable now.

The asychronized part of this library is based on epoll and threadpool. The multi-thread part is implemented by C++11. Because of epoll, the asychronized part isn't support file operation.

##API:

void as_init(int)  

You should call this before any other operations.

std::future<int> as_reg(task_info info, Callback_t callback = \[\](){})

This function is used to register IO event to the threadpool. Callback function will be also registered to the threadpool, and after the IO event completed the callback function will be executed automatically like continuation. And this function will return a std::future<int> representing the number of IO bytes.

int as_left(void)

This function will return the imcomplete IO events.

void as_wait(void)

This function will block until all the registered IO event completed.

void nb_setfd(int fd)

Set the socket fd non-block.

ssize_t nb_read(int fd,	buffer& buf, int \*savedErrno)

The non-blocking read operation. You should initialize a buf which store the data first, the type of buf is buffer. The savedErrno is the errno of the read syscall, user should ensure the space which this pointer points to is available. This operation will try to read all readable data from the given fd.

ssize_t nb_write(int fd, buffer& buf, int \*savedErrno)

The non-blocking write operation. This operation will try to write all writable data in buf to fd.

int nb_connect(int fd, const struct sockaddr \*addr, socklen_t len)

The non-blocking version of connect(). It's usage is the same as connect(). When the operation succeeds it will return 0, otherwise -1. 
