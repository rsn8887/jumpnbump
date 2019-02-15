/*
 * gfx.c
 * Copyright (C) 1998 Brainchild Design - http://brainchilddesign.com/
 * 
 * Copyright (C) 2001 Chuck Mason <cemason@users.sourceforge.net>
 *
 * Copyright (C) 2002 Florian Schulze <crow@icculus.org>
 *
 * This file is part of Jump'n'Bump.
 *
 * Jump'n'Bump is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Jump'n'Bump is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "globals.h"
#include "filter.h"

#include <SDL_endian.h>

#ifdef _MSC_VER
    #include "jumpnbump32.xpm"
#elif __APPLE__
    #include "jumpnbump128.xpm"
#else
    #include "jumpnbump64.xpm"
#endif

#ifdef __SWITCH__
#include <switch.h>
void update_joycon_mode(); // call this once per frame to update split/dual joycon mode
static int single_joycons = 0; // are single Joycons being used right now?
int single_joycon_mode = 0; // is the user requesting singleJoyconMode?
#endif

#if defined(__SWITCH__) || defined(__PSP2__)
int keep_aspect = 1;
#endif

SDL_Surface *icon;

#if defined(__SWITCH__) || defined(__PSP2__)
int screen_width=400;
int screen_height=256;
int screen_pitch=400;
int scale_up=0;
#else
int screen_width=800;
int screen_height=512;
int screen_pitch=800;
int scale_up=1;
#endif

static SDL_Window *jnb_window = NULL;
static SDL_Renderer *jnb_renderer = NULL;
static SDL_PixelFormat *jnb_pixelformat = NULL;
static SDL_Texture *jnb_texture = NULL;
static SDL_PixelFormat* jnb_texture_pixel_format = NULL;
#if defined(__SWITCH__) || defined(__PSP2__)
static int fullscreen = 1;
#else
static int fullscreen = 1;
#endif
static int vinited = 0;
static void *screen_buffer[2];
static int drawing_enable = 0;
static void *background = NULL;
static int background_drawn;
static void *mask = NULL;
static int dirty_blocks[2][25][16];
static SDL_Rect current_render_size;

static SDL_Surface *load_xpm_from_array(char **xpm)
{
#define NEXT_TOKEN { \
	while ((*p != ' ') && (*p != '\t')) p++; \
	while ((*p == ' ') || (*p == '\t')) p++; }

	SDL_Surface *surface;
	char *p;
	int width;
	int height;
	int colors;
	int images;
	int color;
	int pal[256];
	int x,y;

	p = *xpm++;

	width = atoi(p);
	if (width <= 0)
		return NULL;
	NEXT_TOKEN;

	height = atoi(p);
	if (height <= 0)
		return NULL;
	NEXT_TOKEN;

	colors = atoi(p);
	if (colors <= 0)
		return NULL;
	NEXT_TOKEN;

	images = atoi(p);
	if (images <= 0)
		return NULL;

	surface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	if (!surface)
		return NULL;

	SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
	while (colors--) {
		p = *xpm++;

		color = *p++;
		NEXT_TOKEN;

		if (*p++ != 'c') {
			SDL_FreeSurface(surface);
			return NULL;
		}
		NEXT_TOKEN;

		if (*p == '#')
			pal[color] = strtoul(++p, NULL, 16) | 0xff000000;
		else
			pal[color] = 0;
	}

	y = 0;
	while (y < height) {
		int *pixels;

		p = *xpm++;

		pixels = (int *)&((char *)surface->pixels)[y++ * surface->pitch];
		x = 0;
		while (x < width) {
			Uint8 r,g,b,a;

			if (*p == '\0') {
				SDL_FreeSurface(surface);
				return NULL;
			}
			r = (pal[(int)*p] >> 16) & 0xff;
			b = (pal[(int)*p] & 0xff);
			g = (pal[(int)*p] >> 8) & 0xff;
			a = (pal[(int)*p] >> 24) & 0xff;
			pixels[x] = SDL_MapRGBA(surface->format, r, g, b, a);
			x++;
			p++;
		}
	}

	return surface;
}

unsigned char *get_vgaptr(int page, int x, int y)
{
	assert(drawing_enable==1);

	return (unsigned char *)screen_buffer[page] + (y*screen_pitch)+(x);
}


void set_scaling(int scale)
{
	if (scale==1) {
		screen_width=800;
		screen_height=512;
		scale_up=1;
		screen_pitch=screen_width;
	} else {
		screen_width=400;
		screen_height=256;
		scale_up=0;
		screen_pitch=screen_width;
	}
}

void open_screen(void)
{
	int lval = 0;

	lval = SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	if (lval < 0) {
		fprintf(stderr, "SDL ERROR (SDL_Init): %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
	SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft");

	reinit_screen();

	vinited = 1;

	screen_buffer[0]=malloc(screen_width*screen_height);
	screen_buffer[1]=malloc(screen_width*screen_height);
}

void reinit_screen(void)
{
	int flags = SDL_WINDOW_RESIZABLE;
	SDL_RendererInfo renderer_info;
	Uint32 texture_format;

	if (fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

	if (jnb_texture) {
		SDL_DestroyTexture(jnb_texture);
		jnb_texture = NULL;
	}

	if (jnb_renderer) {
		SDL_DestroyRenderer(jnb_renderer);
		jnb_renderer = NULL;
	}

	if (!jnb_window) {
		jnb_window = SDL_CreateWindow("Jump'n'Bump",
				SDL_WINDOWPOS_UNDEFINED,
				SDL_WINDOWPOS_UNDEFINED,
				screen_width,
				screen_height,
				flags);
		if (!jnb_window) {
			fprintf(stderr, "SDL ERROR (SDL_CreateWindow): %s\n", SDL_GetError());
			exit(EXIT_FAILURE);
		}
	}
	current_render_size.x = 0;
	current_render_size.y = 0;
	SDL_GetWindowSize(jnb_window, &current_render_size.w, &current_render_size.h);

	if (!jnb_renderer) {
		jnb_renderer = SDL_CreateRenderer(jnb_window, -1, SDL_RENDERER_ACCELERATED);
		if (!jnb_renderer) {
			fprintf(stderr, "SDL ERROR (SDL_CreateRenderer): %s\n", SDL_GetError());
			exit(EXIT_FAILURE);
		}

		if (SDL_GetRendererInfo(jnb_renderer, &renderer_info) == 0) {
			printf("Using SDL renderer %s\n", renderer_info.name);
		}
	}

	if (!jnb_pixelformat) {
		jnb_pixelformat = SDL_AllocFormat(SDL_PIXELFORMAT_INDEX8);
		if (jnb_pixelformat == NULL) {
			fprintf(stderr, "SDL ERROR (SDL_AllocFormat): %s\n", SDL_GetError());
			exit(EXIT_FAILURE);
		}

		jnb_pixelformat->palette = SDL_AllocPalette(256);
		if (jnb_pixelformat->palette == NULL) {
			fprintf(stderr, "SDL ERROR (SDL_AllocPalette): %s\n", SDL_GetError());
			exit(EXIT_FAILURE);
		}
	}

	jnb_texture = SDL_CreateTexture(jnb_renderer,
				SDL_PIXELFORMAT_UNKNOWN,
				SDL_TEXTUREACCESS_STREAMING,
				screen_width,
				screen_height);
	if (jnb_texture == NULL) {
		fprintf(stderr, "SDL ERROR (SDL_CreateTexture): %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if (SDL_QueryTexture(jnb_texture, &texture_format, NULL, NULL, NULL) != 0)
	{
		fprintf(stderr, "SDL ERROR (SDL_QueryTexture): %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	jnb_texture_pixel_format = SDL_AllocFormat(texture_format);
	if (!jnb_texture_pixel_format)
	{
		fprintf(stderr, "SDL ERROR (SDL_AllocFormat): %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	if(fullscreen)
		SDL_ShowCursor(0);
	else
		SDL_ShowCursor(1);

	icon=load_xpm_from_array(jumpnbump_xpm);
	if (icon==NULL) {
		printf("Couldn't load icon\n");
	} else {
		SDL_SetWindowIcon(jnb_window,icon);
	}

	memset(dirty_blocks, 1, sizeof(dirty_blocks));

	return;
}


void fs_toggle()
{
	Uint32 fullscreen_flags;

	if (!vinited) {
		fullscreen ^= 1;
		return;
	}

	fullscreen_flags = fullscreen ? 0: SDL_WINDOW_FULLSCREEN_DESKTOP;
	
	if (SDL_SetWindowFullscreen(jnb_window,fullscreen_flags) == 0)
		fullscreen ^= 1;

	if(fullscreen)
		SDL_ShowCursor(0);
	else
		SDL_ShowCursor(1);
}

void exit_fullscreen()
{
	if (fullscreen)
		fs_toggle();
}

void wait_vrt()
{
	return;
}


void clear_page(int page, int color)
{
	int i,j;
	unsigned char *buf = get_vgaptr(page, 0, 0);

	assert(drawing_enable==1);

	for (i=0; i<25; i++)
		for (j=0; j<16; j++)
			dirty_blocks[page][i][j] = 1;

	for (i=0; i<screen_height; i++)
		for (j=0; j<screen_width; j++)
			*buf++ = color;
}


void clear_lines(int page, int y, int count, int color)
{
	int i,j;

	assert(drawing_enable==1);

	if (scale_up) {
		count *= 2;
		y *= 2;
	}

	for (i=0; i<count; i++) {
		if ((i+y)<screen_height) {
			unsigned char *buf = get_vgaptr(page, 0, i+y);
			for (j=0; j<screen_width; j++)
				*buf++ = color;
		}
	}

	y /= screen_height / 16;
	count /= screen_height / 16;
    count++;
    while ((y + count) >= 16)
        count--;
	for (i=0; i<count; i++)
		for (j=0; j<25; j++)
			dirty_blocks[page][j][i+y] = 1;
}


int get_color(int color, char pal[768])
{
	assert(color<256);
	assert(pal);
	return SDL_MapRGB(jnb_pixelformat, (Uint8)(pal[color*3+0]<<2), (Uint8)(pal[color*3+1]<<2), (Uint8)(pal[color*3+2]<<2));
}


int get_pixel(int page, int x, int y)
{
	assert(drawing_enable==1);

	if (scale_up) {
		x *= 2;
		y *= 2;
	}

	assert(x<screen_width);
	assert(y<screen_height);

	return *(unsigned char *)get_vgaptr(page, x, y);
}


void set_pixel(int page, int x, int y, int color)
{
	assert(drawing_enable==1);

	if (scale_up) {
		x *= 2;
		y *= 2;
	}

	assert(x<screen_width);
	assert(y<screen_height);

	dirty_blocks[page][x / (screen_width / 25)][y / (screen_height / 16)] = 1;

	*(unsigned char *)get_vgaptr(page, x, y) = color;
}


void flippage(int page)
{
#ifdef __SWITCH__
    // split/combine joycons depending on user setting and handheld mode
    update_joycon_mode();
#endif

	int x,y,pitch;
	unsigned char *src;
	unsigned char *dest;
	int block_x, block_y;
	SDL_Color* colors;

	assert(drawing_enable==0);

	src=screen_buffer[page];

	colors = jnb_pixelformat->palette->colors;

	for (block_x = 0; block_x < 25; ++block_x) {
		for (block_y = 0; block_y < 16; ++block_y) {
			if (dirty_blocks[page][block_x][block_y])
			{
				int block_with = 1;
				int block_height = 1;
				SDL_Rect lock_area;
				int i, j;
				int expand_x, expand_y;

				expand_x = 1;
				expand_y = 1;
				while (expand_x || expand_y) {
					expand_x = expand_x && ((block_x + block_with) < 25);
					for (i = 0; i < block_height && expand_x; ++i) {
						expand_x = dirty_blocks[page][block_x + block_with][block_y + i];
					}
					if (expand_x) ++block_with;

					expand_y = expand_y && ((block_y + block_height) < 16);
					for (i = 0; i < block_with && expand_x; ++i) {
						expand_x = dirty_blocks[page][block_x + i][block_y + block_height];
					}
					if (expand_y) ++block_height;
				}

				lock_area.w = (screen_width / 25) * block_with;
				lock_area.h = (screen_height / 16) * block_height;
				lock_area.x = (screen_width / 25) * block_x;
				lock_area.y = (screen_height / 16) * block_y;
				SDL_LockTexture(jnb_texture, &lock_area, (void **)&dest, &pitch);
				for (x = 0; x < lock_area.w; ++x) {
					for (y = 0; y < lock_area.h; ++y) {
						SDL_Color color = colors[src[screen_pitch * (y + lock_area.y) + x + lock_area.x]];
						void* dest_ptr = dest + y * pitch + x * jnb_texture_pixel_format->BytesPerPixel;

						if (jnb_texture_pixel_format->BytesPerPixel == 2)
						{
							Uint16* dest_int_ptr = (Uint16*) dest_ptr;
							*dest_int_ptr = SDL_MapRGB(jnb_texture_pixel_format, color.r, color.g, color.b);
						}
						else if (jnb_texture_pixel_format->BytesPerPixel == 4)
						{
							Uint32* dest_int_ptr = (Uint32*) dest_ptr;
							*dest_int_ptr = SDL_MapRGB(jnb_texture_pixel_format, color.r, color.g, color.b);
						} else {
							printf("%d bytes per pixel not supported", (int) jnb_texture_pixel_format->BytesPerPixel);
							exit(1);
						}
					}
				}
				SDL_UnlockTexture(jnb_texture);

				for (i = 0; i < block_with; ++i)
					for (j = 0; j < block_height; ++j)
						dirty_blocks[page][block_x + i][block_y + j] = 0;
			}
		}
	}
#if defined(__SWITCH__) || defined(__PSP2__)
	int window_height = 0;
	int window_width = 0;
	SDL_GetWindowSize(jnb_window, &window_width, &window_height);
	if (keep_aspect == 1) {
		current_render_size.h = window_height;
		current_render_size.w = (window_height * JNB_WIDTH) / ((float) JNB_HEIGHT);
		current_render_size.x = (window_width - current_render_size.w) / 2;
		current_render_size.y = 0;
	} else if (keep_aspect == 2) {
		current_render_size.h = window_height;
		current_render_size.w = (window_height * 4) / ((float) 3);
		current_render_size.x = (window_width - current_render_size.w) / 2;
		current_render_size.y = 0;
	} else {
		current_render_size.w = window_width;
		current_render_size.h = window_height;
		current_render_size.x = 0;
		current_render_size.y = 0;
	}
#endif
	SDL_RenderClear(jnb_renderer);
	SDL_RenderCopy(jnb_renderer, jnb_texture, NULL, &current_render_size);
	SDL_RenderPresent(jnb_renderer);
}


void draw_begin(void)
{
	assert(drawing_enable==0);

	drawing_enable = 1;
	if (background_drawn == 0) {
		if (background) {
			put_block(0, 0, 0, JNB_WIDTH, JNB_HEIGHT, background);
			put_block(1, 0, 0, JNB_WIDTH, JNB_HEIGHT, background);
		} else {
			clear_page(0, 0);
			clear_page(1, 0);
		}
		background_drawn = 1;
	}
}


void draw_end(void)
{
	assert(drawing_enable==1);

	drawing_enable = 0;
}


void setpalette(int index, int count, char *palette)
{
	SDL_Color colors[256];
	int i;

	assert(drawing_enable==0);

	for (i = 0; i < count; i++) {
		colors[i+index].r = palette[i * 3 + 0] << 2;
		colors[i+index].g = palette[i * 3 + 1] << 2;
		colors[i+index].b = palette[i * 3 + 2] << 2;
	}
	SDL_SetPaletteColors(jnb_pixelformat->palette, &colors[index], index, count);
	memset(dirty_blocks, 1, sizeof(dirty_blocks));
}


void fillpalette(int red, int green, int blue)
{
	SDL_Color colors[256];
	int i;

	assert(drawing_enable==0);

	for (i = 0; i < 256; i++) {
		colors[i].r = red << 2;
		colors[i].g = green << 2;
		colors[i].b = blue << 2;
	}
	SDL_SetPaletteColors(jnb_pixelformat->palette, colors, 0, 256);
}


void get_block(int page, int x, int y, int width, int height, void *buffer)
{
	unsigned char *buffer_ptr, *vga_ptr;
	int h;

	assert(drawing_enable==1);

	if (scale_up) {
		x *= 2;
		y *= 2;
		width *= 2;
		height *= 2;
	}

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (y + height >= screen_height)
		height = screen_height - y;
	if (x + width >= screen_width)
		width = screen_width - x;
	if (width<=0)
		return;
	if(height<=0)
		return;

	vga_ptr = get_vgaptr(page, x, y);
	buffer_ptr = buffer;
	for (h = 0; h < height; h++) {
		memcpy(buffer_ptr, vga_ptr, width);
		vga_ptr += screen_pitch;
		buffer_ptr += width;
	}

}


void put_block(int page, int x, int y, int width, int height, void *buffer)
{
	int i, j, h;
	unsigned char *vga_ptr, *buffer_ptr;

	assert(drawing_enable==1);

	if (scale_up) {
		x *= 2;
		y *= 2;
		width *= 2;
		height *= 2;
	}

	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (y + height >= screen_height)
		height = screen_height - y;
	if (x + width >= screen_width)
		width = screen_width - x;
	if (width<=0)
		return;
	if(height<=0)
		return;

	vga_ptr = get_vgaptr(page, x, y);
	buffer_ptr = buffer;
	for (h = 0; h < height; h++) {
		memcpy(vga_ptr, buffer_ptr, width);
		vga_ptr += screen_pitch;
		buffer_ptr += width;
	}

	width /= screen_width / 25;
    width += 2;
	height /= screen_height / 16;
    height += 2;
	x /= screen_width / 25;
    y /= screen_height / 16;
    while ((x + width) >= 25)
        width--;
    while ((y + height) >= 16)
        height--;
    for (i=0; i < width; ++i)
        for (j=0; j < height; ++j)
			dirty_blocks[page][i + x][j + y] = 1;
}


void put_text(int page, int x, int y, char *text, int align)
{
	int c1;
	int t1;
	int width;
	int cur_x;
	int image;

	assert(drawing_enable==1);

	if (text == NULL || strlen(text) == 0)
		return;
	if (font_gobs.num_images == 0)
		return;

	width = 0;
	c1 = 0;
	while (text[c1] != 0) {
		t1 = text[c1];
		c1++;
		if (t1 == ' ') {
			width += 5;
			continue;
		}
		if (t1 >= 33 && t1 <= 34)
			image = t1 - 33;

		else if (t1 >= 39 && t1 <= 41)
			image = t1 - 37;

		else if (t1 >= 44 && t1 <= 59)
			image = t1 - 39;

		else if (t1 >= 64 && t1 <= 90)
			image = t1 - 43;

		else if (t1 >= 97 && t1 <= 122)
			image = t1 - 49;

		else if (t1 == '~')
			image = 74;

		else if (t1 == 0x84)
			image = 75;

		else if (t1 == 0x86)
			image = 76;

		else if (t1 == 0x8e)
			image = 77;

		else if (t1 == 0x8f)
			image = 78;

		else if (t1 == 0x94)
			image = 79;

		else if (t1 == 0x99)
			image = 80;

		else
			continue;
		width += pob_width(image, &font_gobs) + 1;
	}

	switch (align) {
	case 0:
		cur_x = x;
		break;
	case 1:
		cur_x = x - width;
		break;
	case 2:
		cur_x = x - width / 2;
		break;
	default:
		cur_x = 0;	/* this should cause error? -Chuck */
		break;
	}
	c1 = 0;

	while (text[c1] != 0) {
		t1 = text[c1];
		c1++;
		if (t1 == ' ') {
			cur_x += 5;
			continue;
		}
		if (t1 >= 33 && t1 <= 34)
			image = t1 - 33;

		else if (t1 >= 39 && t1 <= 41)
			image = t1 - 37;

		else if (t1 >= 44 && t1 <= 59)
			image = t1 - 39;

		else if (t1 >= 64 && t1 <= 90)
			image = t1 - 43;

		else if (t1 >= 97 && t1 <= 122)
			image = t1 - 49;

		else if (t1 == '~')
			image = 74;

		else if (t1 == 0x84)
			image = 75;

		else if (t1 == 0x86)
			image = 76;

		else if (t1 == 0x8e)
			image = 77;

		else if (t1 == 0x8f)
			image = 78;

		else if (t1 == 0x94)
			image = 79;

		else if (t1 == 0x99)
			image = 80;

		else
			continue;
		put_pob(page, cur_x, y, image, &font_gobs, 1);
		cur_x += pob_width(image, &font_gobs) + 1;
	}
}


void put_pob(int page, int x, int y, int image, gob_t *gob, int use_mask)
{
	int c1, c2;
	int pob_x, pob_y;
	int width, height;
	int draw_width, draw_height;
	int colour;
	int i, j;
	unsigned char *vga_ptr;
	unsigned char *pob_ptr;
	unsigned char *mask_ptr;

	assert(drawing_enable==1);
	assert(gob);
	assert(image>=0);
	assert(image<gob->num_images);

	if (scale_up) {
		x *= 2;
		y *= 2;
		width = draw_width = gob->width[image]*2;
		height = draw_height = gob->height[image]*2;
		x -= gob->hs_x[image]*2;
		y -= gob->hs_y[image]*2;
	} else {
		width = draw_width = gob->width[image];
		height = draw_height = gob->height[image];
		x -= gob->hs_x[image];
		y -= gob->hs_y[image];
	}

	if ((x + width) <= 0 || x >= screen_width)
		return;
	if ((y + height) <= 0 || y >= screen_height)
		return;

	pob_x = 0;
	pob_y = 0;
	if (x < 0) {
		pob_x -= x;
		draw_width += x;
		x = 0;
	}
	if ((x + width) > screen_width)
		draw_width -= x + width - screen_width;
	if (y < 0) {
		pob_y -= y;
		draw_height += y;
		y = 0;
	}
	if ((y + height) > screen_height)
		draw_height -= y + height - screen_height;

	vga_ptr = get_vgaptr(page, x, y);
	pob_ptr = ((unsigned char *)gob->data[image]) + ((pob_y * width) + pob_x);
	mask_ptr = ((unsigned char *)mask) + ((y * screen_pitch) + (x));
	for (c1 = 0; c1 < draw_height; c1++) {
		for (c2 = 0; c2 < draw_width; c2++) {
			colour = *mask_ptr;
			if (use_mask == 0 || (use_mask == 1 && colour == 0)) {
				colour = *pob_ptr;
				if (colour != 0) {
					*vga_ptr = colour;
				}
			}
			vga_ptr++;
			pob_ptr++;
			mask_ptr++;
		}
		pob_ptr += width - c2;
		vga_ptr += (screen_width - c2);
		mask_ptr += (screen_width - c2);
	}

	draw_width /= screen_width / 25;
    draw_width += 2;
	draw_height /= screen_height / 16;
    draw_height += 2;
	x /= screen_width / 25;
	y /= screen_height / 16;
    while ((x + draw_width) >= 25)
        draw_width--;
    while ((y + draw_height) >= 16)
        draw_height--;
    for (i=0; i < draw_width; ++i)
        for (j=0; j < draw_height; ++j)
			dirty_blocks[page][i + x][j + y] = 1;
}


int pob_width(int image, gob_t *gob)
{
	assert(gob);
	assert(image>=0);
	assert(image<gob->num_images);
	return gob->width[image];
}


int pob_height(int image, gob_t *gob)
{
	assert(gob);
	assert(image>=0);
	assert(image<gob->num_images);
	return gob->height[image];
}


int pob_hs_x(int image, gob_t *gob)
{
	assert(gob);
	assert(image>=0);
	assert(image<gob->num_images);
	return gob->hs_x[image];
}


int pob_hs_y(int image, gob_t *gob)
{
	assert(gob);
	assert(image>=0);
	assert(image<gob->num_images);
	return gob->hs_y[image];
}


int read_pcx(unsigned char * handle, void *buf, int buf_len, char *pal)
{
	unsigned char *buffer=buf;
	short c1;
	short a, b;
	long ofs1;
	if (buffer != 0) {
		handle += 128;
		ofs1 = 0;
		while (ofs1 < buf_len) {
			a = *(handle++);
			if ((a & 0xc0) == 0xc0) {
				b = *(handle++);
				a &= 0x3f;
				for (c1 = 0; c1 < a && ofs1 < buf_len; c1++)
					buffer[ofs1++] = (char) b;
			} else
				buffer[ofs1++] = (char) a;
		}
		if (pal != 0) {
			handle++;
			for (c1 = 0; c1 < 768; c1++)
				pal[c1] = *(handle++) /*fgetc(handle)*/ >> 2;
		}
	}
	return 0;
}


void register_background(char *pixels, char pal[768])
{
	if (background) {
		free(background);
		background = NULL;
	}
	background_drawn = 0;
	if (!pixels)
		return;
	assert(pal);
	if (scale_up) {
		background = malloc(screen_pitch*screen_height);
		assert(background);
		do_scale2x((unsigned char *)pixels, JNB_WIDTH, JNB_HEIGHT, (unsigned char *)background);
	} else {
		background = malloc(JNB_WIDTH*JNB_HEIGHT);
		assert(background);
		memcpy(background, pixels, JNB_WIDTH*JNB_HEIGHT);
	}
}

int register_gob(unsigned char *handle, gob_t *gob, int len)
{
	unsigned char *gob_data;
	int i;

	gob_data = malloc(len);
	memcpy(gob_data, handle, len);

	gob->num_images = (short)((gob_data[0]) + (gob_data[1] << 8));

	gob->width = malloc(gob->num_images*sizeof(int));
	gob->height = malloc(gob->num_images*sizeof(int));
	gob->hs_x = malloc(gob->num_images*sizeof(int));
	gob->hs_y = malloc(gob->num_images*sizeof(int));
	gob->data = malloc(gob->num_images*sizeof(void *));
	gob->orig_data = malloc(gob->num_images*sizeof(void *));
	for (i=0; i<gob->num_images; i++) {
		int image_size;
		int offset;

		offset = (gob_data[i*4+2]) + (gob_data[i*4+3] << 8) + (gob_data[i*4+4] << 16) + (gob_data[i*4+5] << 24);

		gob->width[i]  = (short)((gob_data[offset]) + (gob_data[offset+1] << 8)); offset += 2;
		gob->height[i] = (short)((gob_data[offset]) + (gob_data[offset+1] << 8)); offset += 2;
		gob->hs_x[i]   = (short)((gob_data[offset]) + (gob_data[offset+1] << 8)); offset += 2;
		gob->hs_y[i]   = (short)((gob_data[offset]) + (gob_data[offset+1] << 8)); offset += 2;

		image_size = gob->width[i] * gob->height[i];
		gob->orig_data[i] = malloc(image_size);
		memcpy(gob->orig_data[i], &gob_data[offset], image_size);
		if (scale_up) {
			image_size = gob->width[i] * gob->height[i] * 4;
			gob->data[i] = malloc(image_size);
			do_scale2x((unsigned char *)gob->orig_data[i], gob->width[i], gob->height[i], (unsigned char *)gob->data[i]);
		} else {
			gob->data[i] = (unsigned short *)gob->orig_data[i];
		}
	}
	free(gob_data);
	return 0;
}

void register_mask(void *pixels)
{
	if (mask) {
		free(mask);
		mask = NULL;
	}
	assert(pixels);
	if (scale_up) {
		mask = malloc(screen_pitch*screen_height);
		assert(mask);
		do_scale2x((unsigned char *)pixels, JNB_WIDTH, JNB_HEIGHT, (unsigned char *)mask);
	} else {
		mask = malloc(JNB_WIDTH*JNB_HEIGHT);
		assert(mask);
		memcpy(mask, pixels, JNB_WIDTH*JNB_HEIGHT);
	}
}

void on_resized(int width, int height)
{
	float aspect = ((float) width) / ((float) height);
	if (aspect > 1.3) {
		if (aspect < 1.9f) {
			current_render_size.w = width;
			current_render_size.h = height;
			current_render_size.x = 0;
			current_render_size.y = 0;
		} else {
			current_render_size.w = (float) (height * 16) / ((float) 9);
			current_render_size.h = height;
			current_render_size.x = (width - current_render_size.w) / 2;
			current_render_size.y = 0;
		}
	} else {
		current_render_size.w = width;
		current_render_size.h = (float) (width * 3) / ((float) 4);
		current_render_size.x = 0;
		current_render_size.y = (height - current_render_size.h) / 2;
	}
	SDL_RenderClear(jnb_renderer);
	SDL_RenderCopy(jnb_renderer, jnb_texture, NULL, &current_render_size);
	SDL_RenderPresent(jnb_renderer);
}

#ifdef __SWITCH__
void update_joycon_mode() {
	int handheld = hidGetHandheldMode();
	int coalesce_controllers = 0;
	int split_controllers = 0;
	if (!handheld) {
		if (single_joycon_mode) {
			if (!single_joycons) {
				split_controllers = 1;
				single_joycons = 1;
			}
		} else if (single_joycons) {
			coalesce_controllers = 1;
			single_joycons = 0;
		}
	} else {
		if (single_joycons) {
			coalesce_controllers = 1;
			single_joycons = 0;
		}
	}
	if (coalesce_controllers) {
		// find all left/right single JoyCon pairs and join them together
		for (int id = 0; id < 8; id++) {
			hidSetNpadJoyAssignmentModeDual((HidControllerID) id);
		}
		int last_right_id = 8;
		for (int id0 = 0; id0 < 8; id0++) {
			if (hidGetControllerType((HidControllerID) id0) & TYPE_JOYCON_LEFT) {
				for (int id1 = last_right_id - 1; id1 >= 0; id1--) {
					if (hidGetControllerType((HidControllerID) id1) & TYPE_JOYCON_RIGHT) {
						last_right_id = id1;
						// prevent missing player numbers
						if (id0 < id1) {
							hidMergeSingleJoyAsDualJoy((HidControllerID) id0, (HidControllerID) id1);
						} else if (id0 > id1) {
							hidMergeSingleJoyAsDualJoy((HidControllerID) id1, (HidControllerID) id0);
						}
						break;
					}
				}
			}
		}
	}
	if (split_controllers) {
		for (int id = 0; id < 8; id++) {
			hidSetNpadJoyAssignmentModeSingleByDefault((HidControllerID) id);
		}
		hidSetNpadJoyHoldType(HidJoyHoldType_Horizontal);
		hidScanInput();
	}
}
#endif
