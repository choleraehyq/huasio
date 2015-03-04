#include "huasio.h"
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
#include <unistd.h>
#include <sys/epoll.h>

namespace huasio {
	
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
			if (epoll_ctl(epfd, EPOLL_CTL_ADD, task.fd, &ev) == -1) {
				printf("%d\n", errno);
				errexit("epoll_ctl error");
			}
			this->cur_event++;
		}
	}
	void threadPool::event_loop(int &epfd) {
		struct epoll_event evlist[MAX];
		int ready = epoll_wait(epfd, evlist, MAX, CIRCLE);
		if (ready == -1) {
			errexit("epoll_wait");
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
			if (epoll_ctl(epfd, EPOLL_CTL_DEL, evlist[i].data.fd, NULL) == -1) {
				errexit("epoll_ctl del error");
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
				int epfd = epoll_create(MAX);
				if (epfd < 0) { 
					errexit("epoll_create in threadPool() error");
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
