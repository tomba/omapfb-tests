#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <linux/fb.h>
#include <linux/omapfb.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define ASSERT(x) if (!(x)) \
	{ perror("assert(" __FILE__ ":" TOSTRING(__LINE__) "): "); exit(1); }
#define FBCTL0(ctl) if (ioctl(fd, ctl))\
	{ perror("fbctl0(" __FILE__ ":" TOSTRING(__LINE__) "): "); exit(1); }
#define FBCTL1(ctl, arg1) if (ioctl(fd, ctl, arg1))\
	{ perror("fbctl1(" __FILE__ ":" TOSTRING(__LINE__) "): "); exit(1); }

#define IOCTL0(fd, ctl) if (ioctl(fd, ctl))\
	{ perror("ioctl0(" __FILE__ ":" TOSTRING(__LINE__) "): "); exit(1); }
#define IOCTL1(fd, ctl, arg1) if (ioctl(fd, ctl, arg1))\
	{ perror("ioctl1(" __FILE__ ":" TOSTRING(__LINE__) "): "); exit(1); }

struct fb_info
{
	int fd;
	char fb_name[64];

	void *ptr;

	struct omapfb_display_info di;
	enum omapfb_update_mode update_mode;

	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	unsigned bytespp;
};

extern char fontdata_8x8[];

void fb_open(int fb_num, struct fb_info *fb_info, int reset);

void fb_init(int fb_num, struct fb_info *fb_info);
void fb_setup_mem(struct fb_info *fb_info,
		unsigned xres, unsigned yres,
		unsigned vxres, unsigned vyres);
void fb_mmap(struct fb_info *fb_info);

void fb_update_window(int fd, short x, short y, short w, short h);
void fb_sync_gfx(int fd);
int fb_put_string(const struct fb_info *fb_info, int x, int y, char *s,
		int maxlen, int color, int clear, int clearlen);
int fb_put_string2(const struct fb_info *fb_info, int x, int y, char *s,
		int color, int clear);
void draw_pixel(const struct fb_info *fb_info, int x, int y, unsigned color);
void draw_test_pattern(const struct fb_info *fb_info);
void draw_test_pattern2(const struct fb_info *fb_info);
void fb_clear_area(const struct fb_info *fb_info, int x, int y, int w, int h);

int zigzag(int min, int max, int c);
int parse_xtimesy(const char *str, unsigned *x, unsigned *y);

void get_time_now(struct timespec *ts);
uint64_t get_time_elapsed_us(const struct timespec *ts_start, const struct timespec *ts_end);

#endif
