#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <math.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <linux/fb.h>
#include <linux/omapfb.h>

#include "common.h"

static struct fb_info fb_info;

static void usage()
{
	printf("usage: -f <fbnum> -s\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
	unsigned c;

	int opt;
	int req_fb = 1;

	unsigned ow = 0;
	unsigned max_w = 0;
	unsigned min_w = 0;

	unsigned oh = 0;
	unsigned max_h = 0;
	unsigned min_h = 0;

	int quiet = 0;

	while ((opt = getopt(argc, argv, "f:s:l:h:q")) != -1) {
		switch (opt) {
		case 'f':
			req_fb = atoi(optarg);
			break;
		case 's':
			if (parse_xtimesy(optarg, &ow, &oh) < 0)
				usage();
			break;
		case 'l':
			if (parse_xtimesy(optarg, &min_w, &min_h) < 0)
				usage();
			break;
		case 'h':
			if (parse_xtimesy(optarg, &max_w, &max_h) < 0)
				usage();
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			usage();
		}
	}

	fb_init(req_fb, &fb_info);

	if (ow == 0 || ow > fb_info.di.xres)
		ow = fb_info.di.xres;

	if (oh == 0 || oh > fb_info.di.yres)
		oh = fb_info.di.yres;

	if (min_w == 0)
		min_w = ow / 2;
	if (max_w == 0)
		max_w = ow * 2;

	if (min_h == 0)
		min_h = oh / 2;
	if (max_h == 0)
		max_h = oh * 2;

	if (max_w > 2048)
		max_w = 2048;

	if (max_h > 2048)
		max_h = 2048;

	fb_setup_mem(&fb_info, ow, oh, max_w, max_h);

	fb_mmap(&fb_info);

	draw_test_pattern(&fb_info);

	c = 0;
	while (1) {
		unsigned iw, ih;
		struct fb_var_screeninfo *var = &fb_info.var;

		iw = zigzag(min_w, max_w, c);
		ih = zigzag(min_h, max_h, c);

		IOCTL1(fb_info.fd, FBIOGET_VSCREENINFO, var);
		var->xres = iw;
		var->yres = ih;
		IOCTL1(fb_info.fd, FBIOPUT_VSCREENINFO, var);

		if (!quiet)
			printf("%f, %f -- %d x %d -> %d x %d\n",
					(float)iw / ow, (float)ih / oh,
					iw, ih,
					ow, oh);

		if (fb_info.update_mode == OMAPFB_MANUAL_UPDATE) {
			fb_update_window(fb_info.fd, 0, 0,
					fb_info.di.xres, fb_info.di.yres);
			fb_sync_gfx(fb_info.fd);
		} else {
			ioctl(fb_info.fd, OMAPFB_WAITFORGO);
		}

		c++;
	}

	return 0;
}
