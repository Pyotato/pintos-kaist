#include <syscall.h>
/**
 *
 * The kernel must put the arguments for the initial function on the register before it allows the user program to begin executing.
 * The arguments are passed in the same way as the normal calling convention.
 *
 *
 */
int main(int, char *[]);
void _start(int argc, char *argv[]);

void _start(int argc, char *argv[])
{
	exit(main(argc, argv));
}
