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

void init_fb_info(int fb_num, struct fb_info *fb_info)
{
	char str[64];
	int fd;

	sprintf(str, "/dev/fb%d", fb_num);
	strcpy(fb_info->fb_name, str);

	fd = open(str, O_RDWR);

	ASSERT(fd >= 0);

	fb_info->fd = fd;

	IOCTL1(fd, FBIOGET_VSCREENINFO, &fb_info->var);
	IOCTL1(fd, FBIOGET_FSCREENINFO, &fb_info->fix);

	struct fb_var_screeninfo *var = &fb_info->var;

	if (ioctl(fd, OMAPFB_GET_DISPLAY_INFO, &fb_info->di)) {
		printf("OMAPFB_GET_DISPLAY_INFO not supported, using var resolution\n");
		fb_info->di.xres = var->xres;
		fb_info->di.yres = var->yres;
	}

	if (ioctl(fd, OMAPFB_GET_UPDATE_MODE, &fb_info->update_mode)) {
		printf("OMAPFB_GET_UPDATE_MODE not supported, using auto update\n");
		fb_info->update_mode = OMAPFB_AUTO_UPDATE;
	}

	printf("display %dx%d\n", fb_info->di.xres, fb_info->di.yres);
	printf("dim %dmm x %dmm\n", var->width, var->height);
}

void setup_fb_mem(struct fb_info *fb_info,
		unsigned xres, unsigned yres,
		unsigned vxres, unsigned vyres)
{
	struct fb_var_screeninfo *var = &fb_info->var;
	struct omapfb_mem_info mi;
	struct omapfb_plane_info pi;
	int fd = fb_info->fd;

	IOCTL1(fd, OMAPFB_QUERY_PLANE, &pi);
	if (pi.enabled) {
		pi.enabled = 0;
		IOCTL1(fd, OMAPFB_SETUP_PLANE, &pi);
	}

	int bitspp = var->bits_per_pixel;

	if (bitspp == 0)
		bitspp = 32;

	if (xres == 0)
		xres = fb_info->di.xres;
	if (yres == 0)
		yres = fb_info->di.yres;

	if (vxres == 0)
		vxres = fb_info->di.xres;
	if (vyres == 0)
		vyres = fb_info->di.yres;

	var->bits_per_pixel = bitspp;
	var->xres = xres;
	var->yres = yres;
	var->xres_virtual = vxres;
	var->yres_virtual = vyres;

	FBCTL1(OMAPFB_QUERY_MEM, &mi);
	mi.size = var->xres_virtual * var->yres_virtual *
		bitspp / 8;
	FBCTL1(OMAPFB_SETUP_MEM, &mi);

	FBCTL1(FBIOPUT_VSCREENINFO, &fb_info->var);

	pi.pos_x = 0;
	pi.pos_y = 0;
	pi.out_width = var->xres;
	pi.out_height = var->yres;
	pi.enabled = 1;
	FBCTL1(OMAPFB_SETUP_PLANE, &pi);

	IOCTL1(fd, FBIOGET_VSCREENINFO, var);
	IOCTL1(fd, FBIOGET_FSCREENINFO, &fb_info->fix);
	IOCTL1(fd, OMAPFB_GET_UPDATE_MODE, &fb_info->update_mode);

	printf("fb res %dx%d virtual %dx%d, line_len %d\n",
			var->xres, var->yres,
			var->xres_virtual, var->yres_virtual,
			fb_info->fix.line_length);

	void* ptr = mmap(0,
			var->yres_virtual * fb_info->fix.line_length,
			PROT_WRITE | PROT_READ,
			MAP_SHARED, fd, 0);

	ASSERT(ptr != MAP_FAILED);

	fb_info->ptr = ptr;
}

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

	init_fb_info(req_fb, &fb_info);

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

	setup_fb_mem(&fb_info, ow, oh, max_w, max_h);

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
