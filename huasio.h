#ifndef _HUASIO_H
#define _HUASIO_H

#include <functional>
#include <future>
#include <cstddef>

namespace huasio {
	const int MAX = 100;
	const int CIRCLE = 10; // ms
	enum class event {
		READ,
		WRITE,
		EXIT
	};
	struct task_info_s {
		int fd;
		size_t num;
		size_t size;
		void *buf;
		event flag;
	};
	using task_info = struct task_info_s;
	using Callback_t = std::function<void()>;

	void as_init(int);
	std::future<int> as_reg(task_info info,
			Callback_t callback = [](){});
	int as_left(void);
	void as_wait(void);

}
#endif
