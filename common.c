#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <stdint.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <linux/fb.h>
#include <linux/omapfb.h>

#include "common.h"

void fb_init(int fb_num, struct fb_info *fb_info)
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

void fb_setup_mem(struct fb_info *fb_info,
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

	printf("fb res %dx%d virtual %dx%d, line_len %d\n",
			var->xres, var->yres,
			var->xres_virtual, var->yres_virtual,
			fb_info->fix.line_length);
}

void fb_mmap(struct fb_info *fb_info)
{
	struct fb_var_screeninfo *var = &fb_info->var;
	int fd = fb_info->fd;

	void* ptr = mmap(NULL,
			var->yres_virtual * fb_info->fix.line_length,
			PROT_WRITE | PROT_READ,
			MAP_SHARED, fd, 0);

	ASSERT(ptr != MAP_FAILED);

	fb_info->ptr = ptr;
}

void fb_open(int fb_num, struct fb_info *fb_info, int reset)
{
	fb_init(fb_num, fb_info);

	if (reset)
		fb_setup_mem(fb_info, 0, 0, 0, 0);

	fb_mmap(fb_info);
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

void fb_clear_area(const struct fb_info *fb_info, int x, int y, int w, int h)
{
	int i = 0;
	int loc;
	char *fbuffer = (char *)fb_info->ptr;
	const struct fb_var_screeninfo *var = &fb_info->var;
	const struct fb_fix_screeninfo *fix = &fb_info->fix;

	for(i = 0; i < h; i++)
	{
		loc = (x + var->xoffset) * (var->bits_per_pixel / 8)
			+ (y + i + var->yoffset) * fix->line_length;
		memset(fbuffer + loc, 0, w * var->bits_per_pixel / 8);
	}
}

static void fb_put_char(const struct fb_info *fb_info, int x, int y, char c,
		unsigned color)
{
	int i, j, bits, loc;
	unsigned short *p16;
	unsigned int *p32;
	const struct fb_var_screeninfo *var = &fb_info->var;
	const struct fb_fix_screeninfo *fix = &fb_info->fix;

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

int fb_put_string(const struct fb_info *fb_info, int x, int y, char *s,
		int maxlen, int color, int clear, int clearlen)
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

int fb_put_string2(const struct fb_info *fb_info, int x, int y, char *s,
		int color, int clear)
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

void draw_pixel(const struct fb_info *fb_info, int x, int y, unsigned color)
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

void draw_test_pattern(const struct fb_info *fb_info)
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

	fb_put_string2(fb_info, 0, 0, str, 0xffffff, 1);

	fb_put_string(fb_info, w / 3 * 2, 30, "RED", 3, 0xffffff, 1, 3);
	fb_put_string(fb_info, w / 3, 30, "GREEN", 5, 0xffffff, 1, 5);
	fb_put_string(fb_info, 20, 30, "BLUE", 4, 0xffffff, 1, 4);
}

void draw_test_pattern2(const struct fb_info *fb_info)
{
	unsigned x, y;
	unsigned h = fb_info->var.yres_virtual;
	unsigned w = fb_info->var.xres_virtual;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (x == 0 || x == w - 1 || y == 0 || y == h - 1)
				draw_pixel(fb_info, x, y, 0xffffff);
			else
				draw_pixel(fb_info, x, y, 0);
		}
	}
}

/* zigzag between [min, max] */
int zigzag(int min, int max, int c)
{
	int d = max - min;

	if (d == 0)
		return min;

	int div = c / d;
	int rem = c % d;

	if (div % 2 == 0)
		return min + rem;
	else
		return max - rem;
}

int parse_xtimesy(const char *str, unsigned *x, unsigned *y)
{
	if (sscanf(str, "%dx%d", x, y) != 2)
		return -EINVAL;
	return 0;
}

static struct timespec timespec_diff(const struct timespec *start,
		const struct timespec *end)
{
	struct timespec temp;
	if ((end->tv_nsec - start->tv_nsec) < 0) {
		temp.tv_sec = end->tv_sec - start->tv_sec - 1;
		temp.tv_nsec = 1000000000 + end->tv_nsec - start->tv_nsec;
	} else {
		temp.tv_sec = end->tv_sec - start->tv_sec;
		temp.tv_nsec = end->tv_nsec - start->tv_nsec;
	}
	return temp;
}

void get_time_now(struct timespec *ts)
{
	clock_gettime(CLOCK_MONOTONIC, ts);
}

uint64_t get_time_elapsed_us(const struct timespec *ts_start, const struct timespec *ts_end)
{
	struct timespec res;
	uint64_t usecs;

	res = timespec_diff(ts_start, ts_end);
	usecs = res.tv_nsec / 1000 + ((uint64_t)res.tv_sec) * 1000 * 1000;

	return usecs;
}
