#ifndef FILELOCK_H
#define FILELOCK_H

int lock_file(int fd, int lock_type);
int unlock_file(int fd);

#endif
