#include "huasio.h"

#include <cstdio>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <future>
#include <string>

int main() {
	std::string a = "test\n";
	huasio::task_info task;
	int fd = STDOUT_FILENO;
	printf("%d\n", fd);
	task.fd = fd;
	task.num = 1;
	task.size = 7;
	task.buf = static_cast<void *>(const_cast<char *>(a.c_str()));
	task.flag = huasio::event::WRITE;
	huasio::as_init(2);
	std::future<int> &&ans = huasio::as_reg(task, 
			[&a]() {
				std::cout << 2 << a;
			});
	printf("%d\n", ans.get());
	huasio::as_wait();
	return 0;
}
