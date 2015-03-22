#include "huio.h"
#include "err.h"

#include <cstddef>
#include <cerrno>
#include <cstdint>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <future>
#include <vector>
#include <unordered_map>
#include <queue>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/epoll.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace huio {
	
	class threadPool {
		private:
			class blockingQueue {
			private:
				std::condition_variable add_cv, del_cv;
				std::mutex add_mtx, del_mtx;
				std::queue<task_info> queue;
				uint32_t size, limits;
			public:
				blockingQueue(void):size(0),
									limits(MAX) 
				{}
				blockingQueue(const blockingQueue &) = delete;
				blockingQueue &operator= (const blockingQueue &) = delete;
				~blockingQueue(void);
				void push(task_info &&task);
				task_info pop(void);
				int empty(void);
			};
			
		public:
			threadPool(int);
			threadPool(const threadPool &) = delete;
			threadPool &operator= (const threadPool &) = delete;
			~threadPool(void);
			void reg(task_info &&task, Callback_t &&cb);
			int rest(void);
			std::unordered_map<int, std::promise<int>> ret_pool;
		private:
			blockingQueue tasks;
			std::vector<std::thread> pool;
			std::unordered_map<int, Callback_t> cb_pool;
			std::unordered_map<int, int> io_nbytes;
			std::unordered_map<int, void *> bufmap;
			int nthreads;
			std::atomic<int> cur_event;
			std::atomic<bool> isStop;
			void event_loop(int &epfd);
			void add_event_if_exist(int &epfd);
	};

	void threadPool::blockingQueue::push(task_info &&task) {
		std::unique_lock<std::mutex> lock(this->add_mtx);
		this->add_cv.wait(lock, [this](){ 
				return !(this->queue.size() == this->limits);
		});
		this->queue.push(std::forward<task_info>(task));
			this->del_cv.notify_one();
	}

	task_info threadPool::blockingQueue::pop(void) {
		std::unique_lock<std::mutex> lock(this->add_mtx);
		this->del_cv.wait(lock, [this](){
				return !(this->queue.empty());
		});
		task_info tmp = queue.front();
		queue.pop();
		this->add_cv.notify_one();
		return tmp;
	}

	int threadPool::blockingQueue::empty(void) {
		std::unique_lock<std::mutex> lock(this->del_mtx);
		return this->queue.empty();
	}
	threadPool::blockingQueue::~blockingQueue(void) {
		std::unique_lock<std::mutex> lock(this->del_mtx);
		while ( !this->queue.empty() ) {
			this->queue.pop();
		}
	}

	void threadPool::add_event_if_exist(int &epfd) {
		if (!(this->tasks).empty()) {
			struct epoll_event ev;
			task_info &&task = this->tasks.pop();
			switch (task.flag) {
				case event::READ: 
					ev.events = EPOLLIN | EPOLLPRI;
					break;
				case event::WRITE: 
					ev.events = EPOLLOUT;
					break;	
			}
		
			ev.data.fd = task.fd;
			this->io_nbytes[task.fd] = task.num * task.size;
			this->bufmap[task.fd] = task.buf;
			if (::epoll_ctl(epfd, EPOLL_CTL_ADD, task.fd, &ev) == -1) {
				printf("%d\n", errno);
				errexit("::epoll_ctl error");
			}
			this->cur_event++;
		}
	}

	void threadPool::event_loop(int &epfd) {
		struct epoll_event evlist[MAX];
		int ready = ::epoll_wait(epfd, evlist, MAX, CIRCLE);
		if (ready == -1) {
			errexit("::epoll_wait");
		}
		for (int i = 0; i < ready; i++) {
			int retval;
			if ((evlist[i].events & EPOLLIN) || 
				(evlist[i].events & EPOLLPRI)) {
				if ((retval = read(evlist[i].data.fd, this->bufmap[evlist[i].data.fd], this->io_nbytes[evlist[i].data.fd])) == -1) {
					errexit("read");
				}
				
			}
			else if ((evlist[i].events & EPOLLOUT)) {
				if ((retval = write(evlist[i].data.fd, this->bufmap[evlist[i].data.fd], this->io_nbytes[evlist[i].data.fd])) == -1) {
					errexit("write");
				}
			}
			if (::epoll_ctl(epfd, EPOLL_CTL_DEL, evlist[i].data.fd, NULL) == -1) {
				errexit("::epoll_ctl del error");
			}
			this->ret_pool[evlist[i].data.fd].set_value(retval);
			this->cb_pool[evlist[i].data.fd]();
			this->cur_event--;
		}	
	}
	threadPool::threadPool(int num)
		:nthreads(num),cur_event(0),isStop(false) {
		for (int i = 0; i < num; i++) {
			pool.emplace_back([this]() {
				int epfd = ::epoll_create(MAX);
				if (epfd < 0) { 
					errexit("::epoll_create in threadPool() error");
				}
				while (!isStop.load()) {
					this->add_event_if_exist(epfd);
					this->event_loop(epfd);
				}
			});
		}
	}

	void threadPool::reg(task_info &&task, 
			Callback_t &&cb) {
		this->cb_pool[task.fd] = std::forward<Callback_t>(cb);
		this->tasks.push(std::forward<task_info>(task));
	}

	int threadPool::rest(void) {
		return this->cur_event.load();
	}

	threadPool::~threadPool() {
		this->isStop.store(true);
		for (auto &worker : this->pool) {
			worker.join();
		}
	}

	std::shared_ptr<threadPool> _pool;

	void as_init(int nthreads) {
		_pool = std::make_shared<threadPool>(nthreads);
	}

	std::future<int> as_reg(task_info task, 
			Callback_t cb) {
		std::promise<int> retval;
		_pool->ret_pool[task.fd] = std::move(retval);
		_pool->reg(std::move(task), std::move(cb));
		return _pool->ret_pool[task.fd].get_future();
	}

	int as_left(void) {
		return _pool->rest();
	}

	void as_wait(void) {
		_pool.reset();
	}
}

namespace huio {

	void nb_setfd(int fd) {
		int flags = fcntl(fd, F_GETFL);
		assert(flags == 0);
		flags |= O_NONBLOCK;
		int err = fcntl(fd, F_SETFL, flags);
		assert(err==0);
	}

	ssize_t nb_read(int fd, buffer &buf, int *savedErrno) {
		return buf.read(fd, savedErrno);
	}

	ssize_t nb_write(int fd, buffer &buf, int *savedErrno) {
		return buf.write(fd, savedErrno);
	}

	ssize_t buffer::read(int fd, int *savedErrno) {
		char extrabuf[64 * 1024];
		struct iovec vec[2];
		const size_t writable = writableBytes();
		vec[0].iov_base = begin() + writeIndex;
		vec[0].iov_len = writable;
		vec[1].iov_base = extrabuf;
		vec[1].iov_len = 64 * 1024;
		ssize_t n = ::readv(fd, vec, 2);
		if (n < 0) {
			*savedErrno = errno;
		}
		else if (static_cast<size_t>(n) <= writable) {
			hasWritten(n);
		}
		else {
			writeIndex = data.size();
			append(extrabuf, n - writable);
		}
		return n;
	}

	ssize_t buffer::write(int fd, int *savedErrno) {
		ssize_t n = ::write(fd, begin(), readableBytes());
		if (n < 0) {
			*savedErrno = errno;
		}
		else if (static_cast<size_t>(n) <= readableBytes()) {
			hasRead(n);
		}
		return n;
	}
	
	struct sockaddr_in getLocalAddr(int fd) {
		struct sockaddr_in localAddr;
		::memset(&localAddr, 0, sizeof(localAddr));
		socklen_t addrlen = static_cast<socklen_t>(sizeof(localAddr));
		if (::getsockname(fd, reinterpret_cast<struct sockaddr *>(&localAddr), &addrlen) < 0) {
			errexit("getLocalAddr");
		}
		return localAddr;
	}

	struct sockaddr_in getRemoteAddr(int fd) {
		struct sockaddr_in remoteAddr;
		::memset(&remoteAddr, 0, sizeof(remoteAddr));
		socklen_t addrlen = static_cast<socklen_t>(sizeof(remoteAddr));
		if (::getpeername(fd, reinterpret_cast<struct sockaddr *>(&remoteAddr), &addrlen) < 0) {
			errexit("getRemoteAddr");
		}
		return remoteAddr;
	}

	bool isSelfConnection(int fd) {
		struct sockaddr_in localAddr = getLocalAddr(fd);
		struct sockaddr_in remoteAddr = getRemoteAddr(fd);
		return localAddr.sin_port == remoteAddr.sin_port 
			&& localAddr.sin_addr.s_addr == remoteAddr.sin_addr.s_addr;
	}

	int nb_connect(int fd, const struct sockaddr *addr, socklen_t len) {
		if (::connect(fd, addr, len) == 0) {
			return 0;
		}
		else if (errno != EINPROGRESS || errno != EINTR) {
				::close(fd);
				return -1;
		}
		if (isSelfConnection(fd)) {
			::close(fd);
			return -1;
		}
		fd_set wset, rset;
		FD_ZERO(&wset);
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		FD_SET(fd, &wset);
		int n;
		if ((n == ::select(fd+1, &rset, &wset, NULL, NULL)) == 0) {
			::close(fd);
			errno = ETIMEDOUT;
			return -1;
		}
		else if (n == -1) {
			::close(fd);
			errexit("select in nb_connect");
		}
		int optval;
		if (FD_ISSET(fd, &rset) || FD_ISSET(fd, &wset)) {
			len = sizeof(optval);
			if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, &len) < 0) {
				::close(fd);
				errexit("getsockopt");
			}
		}
		else {
			::close(fd);
			errexit("fd not set");
		}
		if (optval) {
			::close(fd);
			return -1;
		}
		return 0;
	}
}
