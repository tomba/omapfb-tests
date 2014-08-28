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

#include "common.h"

static unsigned display_xres;
static unsigned display_yres;

static unsigned frame_xres;
static unsigned frame_yres;

static unsigned ovl_xres;
static unsigned ovl_yres;

struct frame_info
{
	void *addr;
	unsigned xres, yres;
	unsigned line_len;
	struct fb_info *fb_info;
};

static void clear_frame(struct frame_info *frame)
{
	int y;

	void *p = frame->addr;

	for (y = 0; y < frame->yres; ++y) {
		if (y == 10)
			memset(p, 0xff, frame->xres * frame->fb_info->bytespp);
		else if (y == frame->yres - 10)
			memset(p, 0xff, frame->xres * frame->fb_info->bytespp);
		else
			memset(p, 0, frame->xres * frame->fb_info->bytespp);
		p += frame->line_len;
	}
}

static void mess_frame(struct frame_info *frame)
{
	int x, y;

	for (y = 0; y < frame->yres; ++y) {
		unsigned int *lp32 = frame->addr + y * frame->line_len;
		unsigned short *lp16 = frame->addr + y * frame->line_len;

		for (x = 0; x < frame->xres; ++x) {

			if (frame->fb_info->bytespp == 2)
				lp16[x] = x*y;
			else
				lp32[x] = x*y;

		}
	}
}

static void draw_bar(struct frame_info *frame, int xpos, int width)
{
	int x, y;
	unsigned short c16;
	unsigned int c32;

	unsigned int colors32[] = {
		0xffffff,
		0xff0000,
		0xffffff,
		0x00ff00,
		0xffffff,
		0x0000ff,
		0xffffff,
		0xaaaaaa,
		0xffffff,
		0x777777,
		0xffffff,
		0x333333,
		0xffffff,
	};

	unsigned short colors16[] = {
		0xffff,
		0x001f,
		0xffff,
	};

	for (y = 0; y < frame->yres; ++y) {
		unsigned int *lp32 = frame->addr + y * frame->line_len;
		unsigned short *lp16 = frame->addr + y * frame->line_len;

		c16 = colors16[y / (frame->yres / (sizeof(colors16) / 2))];
		c32 = colors32[y / (frame->yres / (sizeof(colors32) / 4))];

		for (x = xpos; x < xpos+width; ++x) {
			if (frame->fb_info->bytespp == 2)
				lp16[x] = c16;
			else
				lp32[x] = c32;
		}
	}
}

static void update_window(struct fb_info *fb_info,
		unsigned width, unsigned height)
{
	struct omapfb_update_window upd;

	upd.x = 0;
	upd.y = 0;
	upd.width = width;
	upd.height = height;
	IOCTL1(fb_info->fd, OMAPFB_UPDATE_WINDOW, &upd);
}

static unsigned long min_pan_us, max_pan_us, sum_pan_us;
static struct timespec tv1;
static struct timespec ftv1;
static uint64_t draw_total_time;
static int num_draws;

static void init_perf()
{
	get_time_now(&ftv1);
	min_pan_us = 0xffffffff;
	max_pan_us = 0;
	sum_pan_us = 0;
}

static void perf_frame(int frame)
{
	const int num_frames = 100;
	uint64_t us;
	unsigned long ms;
	struct timespec ftv2;

	if (frame > 0 && frame % num_frames == 0) {
		get_time_now(&ftv2);

		us = get_time_elapsed_us(&ftv1, &ftv2);
		ms = us / 1000;

		float draw_avg = (float)draw_total_time / num_draws / 1000;

		printf("%d frames in %lu ms, %lu fps, "
				"pan min %lu, max %lu, avg %lu, draw avg %f ms\n",
				num_frames,
				ms, num_frames * 1000 / ms,
				min_pan_us, max_pan_us,
				sum_pan_us / num_frames,
				draw_avg);

		get_time_now(&ftv1);
		min_pan_us = 0xffffffff;
		max_pan_us = 0;
		sum_pan_us = 0;

		draw_total_time = 0;
		num_draws = 0;
	}
}

void perf_pan_start()
{
	get_time_now(&tv1);
}

void perf_pan_stop()
{
	unsigned long us;
	struct timespec tv2;

	get_time_now(&tv2);

	us = get_time_elapsed_us(&tv1, &tv2);

	if (us > max_pan_us)
		max_pan_us = us;
	if (us < min_pan_us)
		min_pan_us = us;
	sum_pan_us += us;

}


int main(int argc, char **argv)
{
	int fd;
	int frame;
	struct omapfb_mem_info mi;
	struct omapfb_plane_info pi;
	void *fb_base;
	char str[64];
	enum omapfb_update_mode update_mode;
	int manual;
	struct omapfb_caps caps;

	struct fb_info fb_info;
	struct fb_var_screeninfo *var = &fb_info.var;
	struct fb_fix_screeninfo *fix = &fb_info.fix;
	struct omapfb_display_info di;

	struct frame_info frame1, frame2;

	int opt;
	int req_nfrm = 10000;
	int req_fb = 0;
	int req_bitspp = 32;
	int req_yuv = 0;
	int req_rot = 0;
	int req_te = 1;
	int req_skipdraw = 0;

	while ((opt = getopt(argc, argv, "f:r:m:y:n:ts")) != -1) {
		switch (opt) {
		case 'f':
			req_fb = atoi(optarg);
			break;
		case 'r':
			req_rot = atoi(optarg);
			break;
		case 'm':
			req_bitspp = atoi(optarg);
			break;
		case 'y':
			req_yuv = atoi(optarg);
			break;
		case 't':
			req_te = 0;
			break;
		case 'n':
			req_nfrm = atoi(optarg);
			break;
		case 's':
			req_skipdraw = 1;
			break;
		default:
			printf("usage: ./db [-f <fbnum>] [-r <rot>] [-m <bitspp>] "
					"[-y <yuv>] [-n <num_of_frames>] [-t] [-s]\n");
			exit(EXIT_FAILURE);
		}
	}

	if (req_yuv != 0 && req_fb == 0) {
		printf("GFX overlay doesn't support YUV\n");
		return -1;
	}

	sprintf(str, "/dev/fb%d", req_fb);
	fd = open(str, O_RDWR);
	fb_info.fd = fd;

	FBCTL1(OMAPFB_GET_DISPLAY_INFO, &di);
	display_xres = di.xres;
	display_yres = di.yres;

	frame_xres = display_xres;
	frame_yres = display_yres;

	ovl_xres = display_xres;
	ovl_yres = display_yres;

	/* disable overlay */
	FBCTL1(OMAPFB_QUERY_PLANE, &pi);
	pi.enabled = 0;
	FBCTL1(OMAPFB_SETUP_PLANE, &pi);

	if (req_bitspp != 0) {
		fb_info.bytespp = req_bitspp / 8;
	} else {
		FBCTL1(FBIOGET_VSCREENINFO, var);
		fb_info.bytespp = var->bits_per_pixel / 8;
	}

	/* allocate memory */
	FBCTL1(OMAPFB_QUERY_MEM, &mi);
	mi.size = frame_xres * frame_yres *
		fb_info.bytespp * 2 * 2;
	FBCTL1(OMAPFB_SETUP_MEM, &mi);

	/* setup var info */
	FBCTL1(FBIOGET_VSCREENINFO, var);
	var->rotate = req_rot;
	if (var->rotate == 0 || var->rotate == 2) {
		var->xres = frame_xres;
		var->yres = frame_yres;
	} else {
		var->xres = frame_yres;
		var->yres = frame_xres;
	}
	var->xres_virtual = var->xres;
	var->yres_virtual = var->yres * 2;
	var->bits_per_pixel = fb_info.bytespp * 8;
	if (req_yuv == 1)
		var->nonstd = OMAPFB_COLOR_YUV422;
	else if (req_yuv == 2)
		var->nonstd = OMAPFB_COLOR_YUY422;
	else
		var->nonstd = 0;
	FBCTL1(FBIOPUT_VSCREENINFO, var);

	/* setup overlay */
	FBCTL1(FBIOGET_FSCREENINFO, fix);
	pi.out_width = ovl_xres;
	pi.out_height = ovl_yres;
	pi.enabled = 1;
	FBCTL1(OMAPFB_SETUP_PLANE, &pi);

	printf("mmap %u * %u = %u\n", fix->line_length, var->yres_virtual,
			fix->line_length * var->yres_virtual);
	fb_base = mmap(NULL, fix->line_length * var->yres_virtual,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			fd, 0);
	if (fb_base == MAP_FAILED) {
		perror("mmap: ");
		exit(1);
	}

	frame1.addr = fb_base;
	frame1.xres = var->xres;
	frame1.yres = var->yres;
	frame1.line_len = fix->line_length;
	frame1.fb_info = &fb_info;

	frame2 = frame1;
	frame2.addr = fb_base + frame1.yres * frame1.line_len;


	usleep(100000);

	FBCTL1(OMAPFB_GET_UPDATE_MODE, &update_mode);
	if (update_mode == OMAPFB_MANUAL_UPDATE) {
		printf("Manual update mode\n");
		manual = 1;
	} else {
		manual = 0;
	}

	FBCTL1(OMAPFB_GET_CAPS, &caps);
	if ((caps.ctrl & OMAPFB_CAPS_TEARSYNC)) {
		struct omapfb_tearsync_info tear;
		if (req_te)
			printf("Enabling TE\n");
		else
			printf("Disabling TE\n");
		tear.enabled = req_te;
		FBCTL1(OMAPFB_SET_TEARSYNC, &tear);
	}

	printf("entering mainloop\n");
	usleep(100000);

	frame = 0;

	init_perf();

	while (1) {
		struct frame_info *current_frame;

		perf_frame(frame);

		if (frame % 2 == 0) {
			current_frame = &frame2;
			var->yoffset = 0;
		} else {
			current_frame = &frame1;
			var->yoffset = var->yres;
		}

		frame++;
		if (frame >= req_nfrm)
			break;

		perf_pan_start();

		FBCTL1(FBIOPAN_DISPLAY, var);

		perf_pan_stop();

		if (manual) {
			FBCTL0(OMAPFB_SYNC_GFX);
			update_window(&fb_info, display_xres,
					display_yres);
		} else {
			FBCTL0(OMAPFB_WAITFORGO);
			//FBCTL0(OMAPFB_WAITFORVSYNC);
		}

		if (0)
		{
			struct frame_info *prev_frame;

			if (current_frame == &frame1)
				prev_frame = &frame2;
			else
				prev_frame = &frame1;

			FBCTL0(OMAPFB_SYNC_GFX);
			mess_frame(prev_frame);
		}

		if (!req_skipdraw) {
			const int bar_width = 40;
			const int speed = 8;
			int bar_xpos = (frame * speed) %
				(var->xres - bar_width);

			struct timespec ts1, ts2;

			get_time_now(&ts1);

			clear_frame(current_frame);

			draw_bar(current_frame, bar_xpos, bar_width);

			get_time_now(&ts2);

			draw_total_time += get_time_elapsed_us(&ts1, &ts2);
			num_draws++;
		}
	}

	close(fd);

	munmap(fb_base, mi.size);

	return 0;
}

