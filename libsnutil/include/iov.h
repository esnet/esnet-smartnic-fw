#include <sys/uio.h>		/* struct iovec */

extern void iov_gather(void * dst, const struct iovec * src, unsigned long nr_segs);
extern void * iov_alloc_gather(const struct iovec * src, unsigned long nr_segs, size_t * length);
extern size_t iov_length(const struct iovec * iov, unsigned long nr_segs);
