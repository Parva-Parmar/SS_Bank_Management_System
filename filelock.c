#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

int lock_file(int fd, int lock_type) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = lock_type;  // F_WRLCK or F_RDLCK
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;  // Lock entire file
    return fcntl(fd, F_SETLKW, &lock);
}

int unlock_file(int fd) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    return fcntl(fd, F_SETLK, &lock);
}
