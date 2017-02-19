#include <types.h>
#include <syscall.h>

ssize_t
sys_write(int fd, const void *buf, size_t buflen)
{
    (void)fd;
    (void)buf;
    (void)buflen;

    return 0;
}
