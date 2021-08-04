#include <string.h>		/* memcpy */
#include <stdlib.h>		/* calloc */
#include "iov.h"

void iov_gather(void * dst, const struct iovec * src, unsigned long nr_segs)
{
  size_t offset = 0;
  for (unsigned int seg = 0; seg < nr_segs; seg++) {
    memcpy(dst + offset, src[seg].iov_base, src[seg].iov_len);
    offset += src[seg].iov_len;
  }
}

void * iov_alloc_gather(const struct iovec * src, unsigned long nr_segs, size_t * length)
{
  size_t src_len;
  void * dst;

  src_len = iov_length(src, nr_segs);
  dst = calloc(src_len, 1);
  if (dst == NULL) {
    return NULL;
  }

  iov_gather(dst, src, nr_segs);

  if (length != NULL) {
    *length = src_len;
  }
  return dst;
}

size_t iov_length(const struct iovec * iov, unsigned long nr_segs)
{
  size_t ret = 0;
  for (unsigned long seg = 0; seg < nr_segs; seg++) {
    ret += iov[seg].iov_len;
  }
  return ret;
}
