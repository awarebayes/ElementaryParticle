#include "queue.h"
#include <sys/select.h>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <stdio.h>
#include <string.h>

#define MAX_QUEUES 10
#define MAX_FD_PER_QUEUE 1024

static fd_set readfds[MAX_QUEUES] = {0};
static fd_set writefds[MAX_QUEUES] = {0};
static int max_fd[MAX_QUEUES] = {0};
static int num_fds[MAX_QUEUES] = {0};
static pthread_mutex_t num_queues_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    int queue_id;
    int fd;
    enum queue_event_type type;
    void *data;
} fd_info;

static fd_info fd_array[MAX_QUEUES][MAX_FD_PER_QUEUE];

static int num_queues = 0; // Number of queues created

void print_fdarray(int queue)
{
    for (int i = 0; i < 10; i++)
    {
        fd_info fdf = fd_array[queue][i];
        printf("FDarray i %d - fd %d - type %d - data %p\n", i, fdf.fd, fdf.type, fdf.data);
    }
}

int queue_create(void)
{
    int new_queue_index = -1;

    pthread_mutex_lock(&num_queues_mutex);

    if (num_queues < MAX_QUEUES)
    {
        // Initialize the read and write sets for the new queue
        FD_ZERO(&readfds[num_queues]);
        FD_ZERO(&writefds[num_queues]);
        max_fd[num_queues] = -1;
        num_fds[num_queues] = 0;

        new_queue_index = num_queues;
        num_queues++;

        printf("Created queue %d\n", new_queue_index);
    }

    pthread_mutex_unlock(&num_queues_mutex);

    return num_queues - 1; // Return the index of the created queue
}

int queue_add_fd(int qfd, int fd, enum queue_event_type type, int shared, const void *data)
{

    if (qfd < 0 || qfd >= num_queues)
    {
        return -1; // Invalid queue index
    }

    if (num_fds[qfd] >= MAX_FD_PER_QUEUE)
    {
        return -1; // Maximum number of file descriptors reached for this queue
    }

    pthread_mutex_lock(&num_queues_mutex);

    if (fd > max_fd[qfd])
    {
        max_fd[qfd] = fd;
    }

    if (type == QUEUE_EVENT_IN)
    {
        FD_SET(fd, &readfds[qfd]);
    }
    else if (type == QUEUE_EVENT_OUT)
    {
        FD_SET(fd, &writefds[qfd]);
    }

    // You can store additional information about the file descriptor
    // and its event type as needed for your application.

    fd_info info;
    info.queue_id = qfd;
    info.fd = fd;
    info.type = type;
    info.data = (void *)data;
    fd_array[qfd][num_fds[qfd]] = info;

    num_fds[qfd]++;

    if (type == QUEUE_EVENT_IN)
    {
        printf("Added fd %d to queue %d with data to %p (%d) to read \n", fd, qfd, fd_array[qfd][num_fds[qfd] - 1].data, data);
    }

    if (type == QUEUE_EVENT_OUT)
    {
        printf("Added fd %d to queue %d with data to %p to write \n", fd, qfd, fd_array[qfd][num_fds[qfd] - 1].data);
    }

    pthread_mutex_unlock(&num_queues_mutex);

    return 0;
}

int queue_mod_fd(int qfd, int fd, enum queue_event_type type, const void *data)
{
    if (qfd < 0 || qfd >= num_queues)
    {
        return -1; // Invalid queue index
    }

    if (fd < 0 || fd > max_fd[qfd])
    {
        return -1; // Invalid file descriptor
    }

    if (type == QUEUE_EVENT_IN)
    {
        if (!FD_ISSET(fd, &readfds[qfd]))
        {
            FD_SET(fd, &readfds[qfd]);
        }
        printf("Modified fd %d to queue %d with data to %p to read \n", fd, qfd, data);
    }

    if (type == QUEUE_EVENT_OUT)
    {
        if (!FD_ISSET(fd, &writefds[qfd]))
        {
            FD_SET(fd, &writefds[qfd]);
        }
        printf("Modified fd %d to queue %d with data to %p to write \n", fd, qfd, data);
    }

    int exact_found = 0;
    for (int i = 0; i < num_fds[qfd]; i++)
    {
        if (fd_array[qfd][i].fd == fd && fd_array[qfd][i].type == type)
        {
            // Update the data pointer associated with the file descriptor
            fd_array[qfd][i].data = (void *)data;
            exact_found = 1;
            break;
        }
    }

    if (!exact_found)
    {
        fd_info info;
        info.queue_id = qfd;
        info.fd = fd;
        info.type = type;
        info.data = (void *)data;
        fd_array[qfd][num_fds[qfd]] = info;
        num_fds[qfd]++;
    }

    printf("Modified\n");
    return 0;
}

int queue_rem_fd(int qfd, int fd)
{
    if (qfd < 0 || qfd >= num_queues)
    {
        return -1; // Invalid queue index
    }

    if (fd < 0 || fd > max_fd[qfd])
    {
        return -1; // Invalid file descriptor
    }

    if (FD_ISSET(fd, &readfds[qfd]))
    {
        FD_CLR(fd, &readfds[qfd]);
    }

    if (FD_ISSET(fd, &writefds[qfd]))
    {
        FD_CLR(fd, &writefds[qfd]);
    }

    printf("Going to remove fd %d from queue %d\n", fd, qfd);

    // You may also remove the associated data, but in this example, we keep it intact.

    // Shift the remaining file descriptors to maintain a compact array (optional).
    for (int i = 0; i < num_fds[qfd]; i++)
    {
        if (fd_array[qfd][i].fd == fd)
        {
            memset(&fd_array[qfd][i], sizeof(fd_info), 0);
        }
    }

    printf("Removed fd %d from queue %d\n", fd, qfd);
    num_fds[qfd]--; // Watchme!

    return 0;
}

void log_fd_set(const fd_set *set, int q)
{
    printf("File Descriptors in the Set for q %d: ", q);
    for (int fd = 0; fd < FD_SETSIZE; fd++)
    {
        if (FD_ISSET(fd, set))
        {
            printf("%d ", fd);
        }
    }
    printf("\n");
}

ssize_t queue_wait(int qfd, queue_event *events, size_t event_len)
{
    printf("wait\n");
    if (qfd < 0 || qfd >= num_queues)
    {
        return -1; // Invalid queue index
    }

    if (event_len == 0)
    {
        return 0; // No events requested
    }

    fd_set readfds_copy;
    fd_set writefds_copy;
    FD_ZERO(&readfds_copy);
    FD_ZERO(&writefds_copy);
    memcpy(&readfds_copy, &readfds[qfd], sizeof(fd_set));
    memcpy(&writefds_copy, &writefds[qfd], sizeof(fd_set));

    printf("Read:");
    log_fd_set(&readfds_copy, qfd);
    printf("Writ:");
    log_fd_set(&writefds_copy, qfd);

    int maxfd = max_fd[qfd] + 1; // Maximum file descriptor

    printf("Waiting for max %d from queue %d maxfd %d\n", event_len, qfd, maxfd);

    ssize_t nready = select(maxfd, &readfds_copy, &writefds_copy, NULL, NULL);

    // printf("Got events %d from queue %d\n", nready, qfd);

    if (nready < 0)
    {

        printf("No one is ready: %d\n", nready);
        return -1; // Error
    }

    ssize_t events_found = 0;

    for (int fd = 0; fd <= max_fd[qfd] && events_found < event_len; fd++)
    {
        if (FD_ISSET(fd, &readfds_copy))
        {
            events[events_found].events = QUEUE_EVENT_IN;
            events[events_found].fd = fd;        // Populate the data structure
            events[events_found].queue_id = qfd; // Populate the data structure
            events_found++;
        }

        if (FD_ISSET(fd, &writefds_copy))
        {
            events[events_found].events = QUEUE_EVENT_OUT;
            events[events_found].fd = fd;        // Populate the data structure
            events[events_found].queue_id = qfd; // Populate the data structure
            events_found++;
        }
    }

    return events_found;
}

void *queue_event_get_data(const queue_event *event)
{

    pthread_mutex_lock(&num_queues_mutex);
    printf("Getting event data for fd %d from queue %d\n", event->fd, event->queue_id);
    print_fdarray(event->queue_id);
    printf("Queue ended\n");
    int q = event->queue_id;
    for (int i = 0; i < num_fds[q]; i++)
    {
        if (fd_array[q][i].fd == event->fd && fd_array[q][i].type == event->events)
        {
            printf("Getting event data for fd %d got %p\n", event->fd, fd_array[q][i].data);
            void *data = fd_array[q][i].data;
            pthread_mutex_unlock(&num_queues_mutex);
            return data;
        }
    }

    pthread_mutex_unlock(&num_queues_mutex);
    assert(0);

    return NULL; // Data not found
}

int queue_event_is_error(const queue_event *event)
{
    // In this implementation, you can consider any event type not equal to IN or OUT as an error.
    // Adjust this logic as needed based on your specific error event handling.

    int is_error = (event->events != QUEUE_EVENT_IN && event->events != QUEUE_EVENT_OUT);
    printf("Is error call: %d\n", is_error);

    return is_error;
}