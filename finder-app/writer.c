#include "writer.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include <syslog.h>

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
				to be written was written to the file.\n");
		return 1;
	}

	return 0;

}

int main(int argc, char* argv[])
{
	openlog("writer",LOG_USER,LOG_USER);	
	
	if(argc != 3)
	{
		const char* usageErrStr = "Insufficient number of arguments provided."
						 " 2 are required.\n\n";

		const char* correctUsageStr ="USAGE: write $1 $2\n\n"
				"$1 = writefile (file which is to be overwritten)\n"
				"$2 = writetext (new contents of the file)\n\n";

		fprintf(stderr, "%s", usageErrStr);
		printf("%s", correctUsageStr);

		syslog(LOG_ERR, "%s", usageErrStr);
		syslog(LOG_INFO, "%s", correctUsageStr);

		closelog();

		return 1;
	}	
	
	closelog();

	return myWriter(argv[1], argv[2]);
}
