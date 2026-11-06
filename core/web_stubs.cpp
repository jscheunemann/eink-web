// Browser build: system calls a web page cannot perform.
#include <sys/time.h>

// TrustedTime restores its clock floor with settimeofday(). The page clock is
// already real time and cannot be set, so accept and ignore the write.
extern "C" int settimeofday(const struct timeval *, const struct timezone *) { return 0; }
