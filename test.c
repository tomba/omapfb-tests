#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <linux/fb.h>
#include <linux/omapfb.h>

#include "common.h"

static struct fb_info fb_info;

int main(int argc, char** argv)
{
	int opt;
	int req_fb = 0;
	int req_reset = 0;

	while ((opt = getopt(argc, argv, "f:r")) != -1) {
		switch (opt) {
		case 'f':
			req_fb = atoi(optarg);
			break;
		case 'r':
			req_reset = 1;
			break;
		default:
			exit(EXIT_FAILURE);
		}
	}

	fb_open(req_fb, &fb_info, req_reset);

	draw_test_pattern(&fb_info);

	if (fb_info.update_mode == OMAPFB_MANUAL_UPDATE) {
		fb_update_window(fb_info.fd, 0, 0,
				fb_info.di.xres, fb_info.di.yres);
		fb_sync_gfx(fb_info.fd);
	}

	return 0;
}
