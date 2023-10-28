#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>

#ifdef __linux__
#ifdef EPOLLFL
#include <sys/epoll.h>
typedef struct epoll_event queue_event;
#else
#include <sys/epoll.h>
struct my_epoll_event
{
	void *ptr;
	int queue_id;
	int fd;
	uint32_t events;   /* Epoll events */
};

typedef struct my_epoll_event queue_event;
#endif
#else
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

typedef struct kevent queue_event;
#endif

enum queue_event_type
{
	QUEUE_EVENT_IN,
	QUEUE_EVENT_OUT,
};

int queue_create(void);
int queue_add_fd(int qfd, int fd, enum queue_event_type, int shared, const void *data);
int queue_mod_fd(int qfd, int fd, enum queue_event_type, const void *data);
int queue_rem_fd(int qfd, int);
ssize_t queue_wait(int, queue_event *, size_t);

void *queue_event_get_data(const queue_event *);
int queue_event_is_error(const queue_event *e);

#endif /* QUEUE_H */
