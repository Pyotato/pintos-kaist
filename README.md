Brand new pintos for Operating Systems and Lab (CS330), KAIST, by Youngjin Kwon.

The manual is available at https://casys-kaist.github.io/pintos-kaist/.

# PROJECT2

- Topic : User Program
- Commencement : 06/02
- Dued : 06/11

# RESULTS

![Alt text](image.png)
![Alt text](image-1.png)
![Alt text](image-2.png)
![Alt text](image-3.png)
![Alt text](image-4.png)
![Alt text](image-5.png)
![Alt text](image-6.png)

# LIMITATIONS & ISSUES

- All test cases are stable except for `syn-read`
- Implementation for test case `syn-read` seems to be unstable in that certain tests pass and others it fails.
- Possible causes may be :
- Exception handling in function `sys_read` does not handle all the randomised allocated buffer sizes (see `/tests/filesys/base/syn-read.c` for how test cases are generated.)
- `sys_read` function does not adequately acquires/ releases locks, and therefore causes the kernel to panic.

```
Kernel PANIC at ../../threads/thread.c:432 in thread_exit(): assertion `list_empty(&thread_current()->children)' failed.

```

# IMPLEMENTATION REFERENCES

- [kaist gitbook](https://casys-kaist.github.io/pintos-kaist/)
- [stanford uni class slides](https://web.stanford.edu/class/cs140/projects/pintos/pintos_3.html#SEC32)
