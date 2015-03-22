#ifndef _HUIO_H
#define _HUIO_H

#include <functional>
#include <future>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <cassert>
#include <sys/socket.h>

namespace huio {
	const int MAX = 100;
	const int CIRCLE = 10; // ms
	enum class event {
		READ,
		WRITE,
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

	class buffer {
	private:
		std::vector<char> data;
		int writeIndex;
		int readIndex;
		size_t writableBytes() const {
			return data.size() - writeIndex;
		}
		size_t readableBytes() const {
			return writeIndex - readIndex;
		}
		char *begin() {
			return &*data.begin();
		}
		char *beginWrite() {
			return begin() + writeIndex;
		}
		void hasWritten(size_t len) {
			assert(len <= writableBytes());
			writeIndex += len;
		}
		void hasRead(size_t len) {
			assert(len <= readableBytes());
			readIndex += len;
		}
		void ensureWritableBytes(size_t len) {
			if (writableBytes() < len) {
				makeSpace(len);
			}
			assert(writableBytes() >= len);
		}
		void makeSpace(size_t len) {
			size_t readable = readableBytes();
			if (readIndex >= readable &&
					data.size() - readable >= len) {
				std::copy(begin() + readIndex,
						  begin() + writeIndex,
						  begin());
				readIndex = 0;
				writeIndex = readIndex + readable;
			}
			else {
				data.resize(data.size() + len);
			}
		}
		void append(const char *extra, size_t len) {
			ensureWritableBytes(len);
			std::copy(extra, extra+len, beginWrite());
			hasWritten(len);
		}
	public:
		explicit buffer(size_t initsize = 1024)
			: data(initsize),
			readIndex(0),
			writeIndex(0) {
		}
		//implicit copy-ctor, move-ctor, dtor and assignment is ok.
		ssize_t read(int fd, int *savedErrno);
		ssize_t write(int fd, int *savedErrno);
	};

	void nb_setfd(int fd);
	ssize_t nb_read(int fd, buffer &buf, int *savedErrno);
	ssize_t nb_write(int fd, buffer &buf, int *savedErrno);
	int nb_connect(int fd, const struct sockaddr *addr, socklen_t len);
}
#endif
