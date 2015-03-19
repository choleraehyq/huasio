#include "huio.h"

#include <cstdio>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <future>
#include <string>

int main() {
	std::string a = "test\n";
	huio::task_info task;
	int fd = STDOUT_FILENO;
	printf("%d\n", fd);
	task.fd = fd;
	task.num = 1;
	task.size = 7;
	task.buf = static_cast<void *>(const_cast<char *>(a.c_str()));
	task.flag = huio::event::WRITE;
	huio::as_init(2);
	std::future<int> &&ans = huio::as_reg(task, 
			[&a]() {
				std::cout << 2 << a;
			});
	printf("%d\n", ans.get());
	huio::as_wait();
	return 0;
}
