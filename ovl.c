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

void move_ovl(unsigned x, unsigned y, unsigned w, unsigned h,
		unsigned out_w, unsigned out_h,
		unsigned xoffset, unsigned yoffset)
{
	int fd = fb_info.fd;
	struct fb_var_screeninfo *var = &fb_info.var;
	struct omapfb_plane_info pi;

	//printf("overlay: %d,%d -> %d,%d (%dx%d) (vp: %d,%d)\n",
	//		x, y, x + w, w, h, y + h, xoffset, yoffset);

	/*
	 * disabling the ovl causes flicker, but otherwise we're not able
	 * to do the following changes "atomically".
	 */

	IOCTL1(fd, OMAPFB_QUERY_PLANE, &pi);
	pi.enabled = 0;
	IOCTL1(fd, OMAPFB_SETUP_PLANE, &pi);

	IOCTL1(fd, FBIOGET_VSCREENINFO, var);
	var->xres = w;
	var->yres = h;
	var->xoffset = xoffset;
	var->yoffset = yoffset;
	IOCTL1(fd, FBIOPUT_VSCREENINFO, var);

	IOCTL1(fd, OMAPFB_QUERY_PLANE, &pi);
	pi.out_width = out_w;
	pi.out_height = out_h;
	pi.pos_x = x;
	pi.pos_y = y;
	pi.enabled = 1;
	IOCTL1(fd, OMAPFB_SETUP_PLANE, &pi);
}

int main(int argc, char** argv)
{
	unsigned c;

	int opt;
	int req_fb = 0;
	int req_scaling = 0;

	while ((opt = getopt(argc, argv, "f:s")) != -1) {
		switch (opt) {
		case 'f':
			req_fb = atoi(optarg);
			break;
		case 's':
			req_scaling = 1;
			break;
		default:
			printf("usage: -f <fbnum> -s\n");
			exit(EXIT_FAILURE);
		}
	}

	fb_open(req_fb, &fb_info, 1);

	draw_test_pattern(&fb_info);

	c = 0;
	while (1) {
		unsigned x, y, w, h, xoffset, yoffset, out_w, out_h;

		out_w = (sin((float)c * M_PI / 180 - M_PI / 2) + 1.0) / 2.0 *
			(fb_info.di.xres - 8 - 1) + 8;

		out_h = (sin((float)c * M_PI / 180 *1.2 - M_PI / 2) + 1.0) / 2.0 *
			(fb_info.di.yres - 8 - 1) + 8;

		x = (sin((float)c * M_PI / 180 - M_PI / 2) + 1.0) / 2.0 *
			(fb_info.di.xres - out_w - 1);

		y = (sin((float)c * M_PI / 180 * 1.3 - M_PI / 2) + 1.0) / 2.0 *
			(fb_info.di.yres - out_h - 1);

		if (req_scaling) {
			w = out_w / 2;
			h = out_h / 2;
		} else {
			w = out_w;
			h = out_h;
		}

		xoffset = (cos((float)c * M_PI / 180 * 1.1 - M_PI / 2) + 1.0) / 2.0 *
			(fb_info.var.xres_virtual - w);

		yoffset = (sin((float)c * M_PI / 180 * 1.2 - M_PI / 2) + 1.0) / 2.0 *
			(fb_info.var.yres_virtual - h);

		move_ovl(x, y, w, h, out_w, out_h, xoffset, yoffset);

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
