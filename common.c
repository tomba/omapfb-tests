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

void fb_open(int fb_num, struct fb_info *fb_info, int reset)
{
	char str[64];
	int fd;
	struct fb_var_screeninfo *var = &fb_info->var;

	sprintf(str, "/dev/fb%d", fb_num);
	strcpy(fb_info->fb_name, str);

	fd = open(str, O_RDWR);

	ASSERT(fd >= 0);

	fb_info->fd = fd;

	IOCTL1(fd, FBIOGET_VSCREENINFO, var);
	IOCTL1(fd, FBIOGET_FSCREENINFO, &fb_info->fix);

	if (ioctl(fd, OMAPFB_GET_DISPLAY_INFO, &fb_info->di)) {
		printf("OMAPFB_GET_DISPLAY_INFO not supported, using var resolution\n");
		fb_info->di.xres = var->xres;
		fb_info->di.yres = var->yres;
	}

	if (ioctl(fd, OMAPFB_GET_UPDATE_MODE, &fb_info->update_mode)) {
		printf("OMAPFB_GET_UPDATE_MODE not supported, using auto update\n");
		fb_info->update_mode = OMAPFB_AUTO_UPDATE;
	}

	if (reset) {
		struct omapfb_mem_info mi;
		struct omapfb_plane_info pi;

		int bitspp = var->bits_per_pixel;

		if (bitspp == 0)
			bitspp = 32;

		IOCTL1(fd, OMAPFB_QUERY_PLANE, &pi);
		pi.enabled = 0;
		IOCTL1(fd, OMAPFB_SETUP_PLANE, &pi);

		FBCTL1(OMAPFB_QUERY_MEM, &mi);
		mi.size = fb_info->di.xres * fb_info->di.yres *
			bitspp / 8;
		FBCTL1(OMAPFB_SETUP_MEM, &mi);

		var->bits_per_pixel = bitspp;
		var->xres_virtual = var->xres = fb_info->di.xres;
		var->yres_virtual = var->yres = fb_info->di.yres;
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
	}

	printf("display %dx%d\n", fb_info->di.xres, fb_info->di.yres);
	printf("fb res %dx%d virtual %dx%d, line_len %d\n",
			var->xres, var->yres,
			var->xres_virtual, var->yres_virtual,
			fb_info->fix.line_length);
	printf("dim %dmm x %dmm\n", var->width, var->height);

	void* ptr = mmap(0,
			var->yres_virtual * fb_info->fix.line_length,
			PROT_WRITE | PROT_READ,
			MAP_SHARED, fd, 0);

	ASSERT(ptr != MAP_FAILED);

	fb_info->ptr = ptr;
}

void fb_update_window(int fd, short x, short y, short w, short h)
{
	struct omapfb_update_window uw;

	uw.x = x;
	uw.y = y;
	uw.width = w;
	uw.height = h;

	//printf("update %d,%d,%d,%d\n", x, y, w, h);
	IOCTL1(fd, OMAPFB_UPDATE_WINDOW, &uw);
}

void fb_sync_gfx(int fd)
{
	IOCTL0(fd, OMAPFB_SYNC_GFX);
}

static void fb_clear_area(struct fb_info *fb_info, int x, int y, int w, int h)
{
	int i = 0;
	int loc;
	char *fbuffer = (char *)fb_info->ptr;
	struct fb_var_screeninfo *var = &fb_info->var;
	struct fb_fix_screeninfo *fix = &fb_info->fix;

	for(i = 0; i < h; i++)
	{
		loc = (x + var->xoffset) * (var->bits_per_pixel / 8)
			+ (y + i + var->yoffset) * fix->line_length;
		memset(fbuffer + loc, 0, w * var->bits_per_pixel / 8);
	}
}

static void fb_put_char(struct fb_info *fb_info, int x, int y, char c,
		unsigned color)
{
	int i, j, bits, loc;
	unsigned short *p16;
	unsigned int *p32;
	struct fb_var_screeninfo *var = &fb_info->var;
	struct fb_fix_screeninfo *fix = &fb_info->fix;

	for(i = 0; i < 8; i++) {
		bits = fontdata_8x8[8 * c + i];
		for(j = 0; j < 8; j++) {
			loc = (x + j + var->xoffset) * (var->bits_per_pixel / 8)
				+ (y + i + var->yoffset) * fix->line_length;
			if(loc >= 0 && loc < fix->smem_len &&
					((bits >> (7 - j)) & 1)) {
				switch(var->bits_per_pixel) {
					case 16:
						p16 = fb_info->ptr + loc;
						*p16 = color;
						break;
					case 24:
					case 32:
						p32 = fb_info->ptr + loc;
						*p32 = color;
						break;
				}
			}
		}
	}
}

int fb_put_string(struct fb_info *fb_info, int x, int y, char *s, int maxlen,
		int color, int clear, int clearlen)
{
	int i;
	int w = 0;

	if(clear)
		fb_clear_area(fb_info, x, y, clearlen * 8, 8);

	for(i=0;i<strlen(s) && i < maxlen;i++) {
		fb_put_char(fb_info, (x + 8 * i), y, s[i], color);
		w += 8;
	}

	return w;
}

int fb_put_string2(struct fb_info *fb_info, int x, int y, char *s, int color,
	int clear)
{
	int i;
	int w = 0;
	int len = strlen(s);

	if(clear)
		fb_clear_area(fb_info, x, y, len * 8, 8);

	for(i=0; i < len; i++) {
		fb_put_char(fb_info, (x + 8 * i), y, s[i], color);
		w += 8;
	}

	return w;
}

void draw_pixel(struct fb_info *fb_info, int x, int y, unsigned color)
{
	void *fbmem;

	fbmem = fb_info->ptr;

	if (fb_info->var.bits_per_pixel == 16) {
		unsigned short c;
		unsigned r = (color >> 16) & 0xff;
		unsigned g = (color >> 8) & 0xff;
		unsigned b = (color >> 0) & 0xff;
		unsigned short *p;

		r = r * 32 / 256;
		g = g * 64 / 256;
		b = b * 32 / 256;

		c = (r << 11) | (g << 5) | (b << 0);

		fbmem += fb_info->fix.line_length * y;

		p = fbmem;

		p += x;

		*p = c;
	} else {
		unsigned int *p;

		fbmem += fb_info->fix.line_length * y;

		p = fbmem;

		p += x;

		*p = color;
	}
}

void draw_test_pattern(struct fb_info *fb_info)
{
	unsigned x, y;
	unsigned h = fb_info->var.yres_virtual;
	unsigned w = fb_info->var.xres_virtual;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (x < 20 && y < 20)
				draw_pixel(fb_info, x, y, 0xffffff);
			else if (x < 20 && (y > 20 && y < h - 20))
				draw_pixel(fb_info, x, y, 0xff);
			else if (y < 20 && (x > 20 && x < w - 20))
				draw_pixel(fb_info, x, y, 0xff00);
			else if (x > w - 20 && (y > 20 && y < h - 20))
				draw_pixel(fb_info, x, y, 0xff0000);
			else if (y > h - 20 && (x > 20 && x < w - 20))
				draw_pixel(fb_info, x, y, 0xffff00);
			else if (x == 20 || x == w - 20 ||
					y == 20 || y == h - 20)
				draw_pixel(fb_info, x, y, 0xffffff);
			else if (x == y || w - x == h - y)
				draw_pixel(fb_info, x, y, 0xff00ff);
			else if (w - x == y || x == h - y)
				draw_pixel(fb_info, x, y, 0x00ffff);
			else if (x > 20 && y > 20 && x < w - 20 && y < h - 20) {
				int t = x * 3 / w;
				unsigned r = 0, g = 0, b = 0;
				unsigned c;
				if (fb_info->var.bits_per_pixel == 16) {
					if (t == 0)
						b = (y % 32) * 256 / 32;
					else if (t == 1)
						g = (y % 64) * 256 / 64;
					else if (t == 2)
						r = (y % 32) * 256 / 32;
				} else {
					if (t == 0)
						b = (y % 256);
					else if (t == 1)
						g = (y % 256);
					else if (t == 2)
						r = (y % 256);
				}
				c = (r << 16) | (g << 8) | (b << 0);
				draw_pixel(fb_info, x, y, c);
			} else {
				draw_pixel(fb_info, x, y, 0);
			}
		}

	}

	char str[256];

	sprintf(str, "%s: %dx%d", fb_info->fb_name, w, h);

	fb_put_string2(fb_info, 20, 2, str, 0xffffff, 1);

	fb_put_string(fb_info, w / 3 * 2, 30, "RED", 3, 0xffffff, 1, 3);
	fb_put_string(fb_info, w / 3, 30, "GREEN", 5, 0xffffff, 1, 5);
	fb_put_string(fb_info, 20, 30, "BLUE", 4, 0xffffff, 1, 4);
}

