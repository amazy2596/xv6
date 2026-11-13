#include "kernel/types.h"
#include "user/user.h"

int main()
{
	printf("time: %d\n", uptime());
	exit(1);
}