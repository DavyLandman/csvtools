#ifndef HINTS_H
#define HINTS_H
#if _XOPEN_SOURCE >= 600 || _POSIX_C_SOURCE >= 200112L
	#include <fcntl.h>
	#define SEQUENTIAL_HINT(fd) if (posix_fadvise(fileno(fd), 0, 0, POSIX_FADV_SEQUENTIAL | POSIX_FADV_NOREUSE)) { ; }
#else
	#define SEQUENTIAL_HINT(fd) 
#endif

#endif
