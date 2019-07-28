#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static void play_lottery (char *buffer) {
	int fd;
	unsigned int nrd = strlen(buffer);

	fd = open("/dev/lottery", O_RDWR);
	if (fd == -1)
		err(1, "open");

	write(fd, buffer, nrd);

	close(fd);
} 

int main( int argc, char *argv[] )  {

	if( argc == 2 ) {
	//   printf("The argument supplied is %s\n", argv[1]);
		play_lottery(argv[1]);
	
	}
	else if( argc > 2 ) {
		printf("Too many arguments supplied.\n");
	}
	else {
		printf("One argument expected.\n");
	}
}
