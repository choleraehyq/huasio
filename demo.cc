#include "huasio.h"

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <future>

int main() {
	int a[10] = {1, 2, 3, 4};
	huasio::task_info task;
	int fd = STDOUT_FILENO;
	printf("%d\n", fd);
	task.fd = fd;
	task.num = 1;
	task.size = 4;
	task.buf = static_cast<void *>(a);
	task.flag = huasio::event::WRITE;
	huasio::as_init(2);
	std::future<int> &&ans = huasio::as_reg(task, 
			[&ans, a]() {
				printf("%d %d\n", ans.get(), a);
			});
	printf("%d\n", ans.get());
	return 0;
}
