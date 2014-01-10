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

int main(int argc, char** argv)
{
	unsigned c;

	int opt;
	int req_fb = 1;

	float max_w_fact = 2.0;
	float max_h_fact = 2.0;
	float min_w_fact = 0.2;
	float min_h_fact = 0.2;

	unsigned ow = 0;
	unsigned oh = 0;

	while ((opt = getopt(argc, argv, "f:")) != -1) {
		switch (opt) {
		case 'f':
			req_fb = atoi(optarg);
			break;
		default:
			printf("usage: -f <fbnum> -s\n");
			exit(EXIT_FAILURE);
		}
	}

	fb_init(req_fb, &fb_info);

	if (ow == 0 || ow > fb_info.di.xres)
		ow = fb_info.di.xres;

	if (oh == 0 || oh > fb_info.di.yres)
		oh = fb_info.di.yres;

	unsigned min_w = ow * min_w_fact;
	unsigned max_w = ow * max_w_fact;

	unsigned min_h = oh * min_h_fact;
	unsigned max_h = oh * max_h_fact;

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

		float vx = sin((float)c * M_PI / 180 + M_PI/4) / 2 + 0.5;
		float vy = sin((float)(c*1.1) * M_PI / 180 + M_PI/4) / 2 + 0.5;

		iw = min_w + (max_w - min_w) * vx;
		ih = min_h + (max_h - min_h) * vy;

		IOCTL1(fb_info.fd, FBIOGET_VSCREENINFO, var);
		var->xres = iw;
		var->yres = ih;
		IOCTL1(fb_info.fd, FBIOPUT_VSCREENINFO, var);

		char buf[256];
		sprintf(buf, "x %f", (float)iw / ow);
		fb_put_string2(&fb_info, 0, 0, buf, 0xffffff, 1);
		sprintf(buf, "y %f", (float)ih / oh);
		fb_put_string2(&fb_info, 0, 8, buf, 0xffffff, 1);

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
