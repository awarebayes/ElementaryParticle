#include "queue_impl.h"
#ifdef EPOLLFL
#include "queue.c"
#else
#include "queue_select.c"
#endif