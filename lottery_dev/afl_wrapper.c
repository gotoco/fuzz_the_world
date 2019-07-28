#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioccom.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

static void play_lottery (char *buffer, unsigned long size) {
        int fd;

        fd = open("/dev/lottery", O_RDWR);
        if (fd == -1) {
                fprintf(stderr, "File not found!");
                exit(1);
        }
        write(fd, buffer, size);

        close(fd);
} 

void load_hook(unsigned int argc, char**argv)
{
        return;
}

//int main(int argc, char *argv[])
int run(unsigned int argc, char** argv)
{
        if (argc < 2) {
            fprintf(stderr, "Missing input file to mount.\n");
            return -1;
        }
        if (argc >= 2)
        { 
                FILE *f = fopen(argv[1], "rb");
                if (f == NULL) {
                        printf("File not found!!");
                        exit(1);
                }
                fseek(f, 0, SEEK_END);
                unsigned long fsize = ftell(f);
                fseek(f, 0, SEEK_SET);  /* same as rewind(f); */

                unsigned char *buffer = malloc(fsize);
                fread(buffer, 1, fsize, f);

                play_lottery(buffer, fsize);

                fclose(f);

                free(buffer);
        }

        return 0;
}

