#include "writer.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>

#include <stdio.h>
#include <string.h>

extern int errno;

int myWriter(const char *writefile, const char *writestr)
{
	int fd;

	fd = creat(writefile, S_IRWXU | S_IRGRP | S_IROTH);

	if(fd < 0)
	{
		fprintf(stderr, "Error opening/creating file: %s\n", strerror(errno));
		return 1;
	}

	int writestr_len = strlen(writestr);
	ssize_t bytes_wrote = 0;

	bytes_wrote = write(fd, writestr, writestr_len);

	if(bytes_wrote < 0)
	{
		fprintf(stderr, "Error writing to file: %s\n", strerror(errno));
		return 1;
	}
	else if(bytes_wrote != writestr_len)
	{
		fprintf(stderr, "Error writing to file: Not all of the string \
				to be written was written to the file./n");
		return 1;
	}

	return 0;

}

int main()
{


	return 0;
}
