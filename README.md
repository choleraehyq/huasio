# huio
A toy asychronized-io and non-blocking io library.

It can successfully run on Ubuntu14.10 compiled by g++4.9.1.
This library is based on epoll and threadpool. The multi-thread part is implemented by C++11. 

Because of epoll, this library isn't support file operation.

##API:

void as_init(int)  

You should call this before any other operations.

std::future<int> as_reg(task_info info, Callback_t callback = \[\](){})

This function is used to register IO event to the threadpool. Callback function will be also registered to the threadpool, and after the IO event completed the callback function will be executed automatically like continuation. And this function will return a std::future<int> representing the number of IO bytes.

int as_left(void)

This function will return the imcomplete IO events.

void as_wait(void)

This function will block until all the registered IO event completed.
