#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/omapfb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <math.h>

#include "common.h"

int main(int argc, char **argv)
{
	int fd;
	char str[64];
	struct fb_var_screeninfo var;

	int opt;
	int req_fb = 0;

	int x, y;

	while ((opt = getopt(argc, argv, "f:")) != -1) {
		switch (opt) {
		case 'f':
			req_fb = atoi(optarg);
			break;
		default:
			printf("usage: pan [-f <fbnum>] <x> <y>\n");
			exit(EXIT_FAILURE);
		}
	}

	if (argc - optind != 2) {
		printf("usage: pan [-f <fbnum>] <x> <y>\n");
		exit(EXIT_FAILURE);
	}

	sprintf(str, "/dev/fb%d", req_fb);
	fd = open(str, O_RDWR);

	x = atoi(argv[optind]);
	y = atoi(argv[optind + 1]);

	printf("pan to %d, %d\n", x, y);

	FBCTL1(FBIOGET_VSCREENINFO, &var);
	var.xoffset = x;
	var.yoffset = y;
	FBCTL1(FBIOPAN_DISPLAY, &var);
	FBCTL1(FBIOPUT_VSCREENINFO, &var);

	close(fd);

	return 0;
}

