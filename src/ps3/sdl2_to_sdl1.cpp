#ifdef USE_SDL2
#include <SDL.h>
#include <SDL_ttf.h>
#include "ps3_compat.h"
#include "ps3_paths.h"
#include "ps3_log.h"
#include "ps3_diag.h"
#include <sdl2_to_sdl1.h>
#include <math.h>
#include <stdlib.h>

static SDL_Window* window = NULL;
/* The emulator surface is presented every frame. Creating and destroying a
 * texture for it on every frame makes the RSX driver allocate (and free) GPU
 * memory constantly - and PSL1GHT's DestroyTexture() releases memory that the
 * GPU may still be reading, which can wedge the flip and freeze the app. Keep
 * the textures alive and only update their contents. */
typedef struct {
	SDL_Texture *tex;
	int w;
	int h;
	Uint32 fmt;
} Ps3TexCache;

static Ps3TexCache s_tex_cache[2];
static SDL_Texture* prescaled = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* s_mask_texture = NULL;
static int s_mask_type = -1;
static int s_mask_w = 0;
static int s_mask_h = 0;
static SDL_Surface* s_scale2x_surf = NULL;
static int prescaled_width = 320;
static int prescaled_height = 240;
static int surface_width = 320;
static int surface_height = 240;

static int currently_docked = -1;

static int x_offset;
static int y_offset;
static int scaled_height;
static int scaled_width;
static int display_width;
static int display_height;

extern int mainMenu_shader;
extern int mainMenu_displayedLines;
extern int mainMenu_displayHires;
extern int visibleAreaWidth;
extern int displaying_menu;
extern int mainMenu_screenOffsetX;
extern int mainMenu_screenOffsetY;
extern int mainMenu_autoCrop;
extern int presetModeId;

extern void update_pad_mode(void);

#define MIN(a,b) ((a) < (b) ? (a) : (b))

typedef struct {
    int active_left;
    int active_top;
    int active_right;
    int active_bottom;
    int stable_left;
    int stable_top;
    int stable_right;
    int stable_bottom;
    int shrink_hold_frames;
    int last_surface_w;
    int last_surface_h;
} Ps3CropState;

static Ps3CropState s_crop = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

void ps3_reset_autocrop(void) {
    s_crop.last_surface_w = 0;
    s_crop.last_surface_h = 0;
    s_crop.shrink_hold_frames = 0;
}

static inline bool is_border_pixel(Uint16 pix, Uint16 border_col) {
    if (pix == border_col || pix == 0) return true;
    Uint32 r = (pix >> 11) & 0x1F;
    Uint32 g = (pix >> 5) & 0x3F;
    Uint32 b = pix & 0x1F;
    return (r <= 1 && g <= 2 && b <= 1);
}

static void auto_crop_detect(SDL_Surface *surface, SDL_Rect *out_src_rect) {
    if (!surface || !surface->pixels) {
        out_src_rect->x = 0;
        out_src_rect->y = 0;
        out_src_rect->w = surface ? surface->w : 320;
        out_src_rect->h = surface ? surface->h : 240;
        return;
    }

    int w = surface->w;
    int h = surface->h;

    if (mainMenu_autoCrop == 0) {
        out_src_rect->x = 0;
        out_src_rect->y = 0;
        out_src_rect->w = w;
        out_src_rect->h = h;
        return;
    }

    if (s_crop.last_surface_w != w || s_crop.last_surface_h != h) {
        s_crop.last_surface_w = w;
        s_crop.last_surface_h = h;
        s_crop.stable_left = 0;
        s_crop.stable_top = 0;
        s_crop.stable_right = w - 1;
        s_crop.stable_bottom = h - 1;
        s_crop.shrink_hold_frames = 0;
    }

    Uint16 *pixels = (Uint16 *)surface->pixels;
    int pitch_words = surface->pitch / (int)sizeof(Uint16);
    Uint16 border_color = pixels[0];

    int top_line = 0;
    int max_top_scan = (h * 2) / 5;
    for (int y = 0; y < max_top_scan; y++) {
        int non_border = 0;
        Uint16 *row = pixels + y * pitch_words;
        for (int x = 4; x < w - 4; x += 2) {
            if (!is_border_pixel(row[x], border_color)) {
                non_border++;
                if (non_border >= 3) break;
            }
        }
        if (non_border >= 3) {
            top_line = y;
            break;
        }
    }

    int bottom_line = h - 1;
    int min_bottom_scan = (h * 3) / 5;
    for (int y = h - 1; y >= min_bottom_scan; y--) {
        int non_border = 0;
        Uint16 *row = pixels + y * pitch_words;
        for (int x = 4; x < w - 4; x += 2) {
            if (!is_border_pixel(row[x], border_color)) {
                non_border++;
                if (non_border >= 3) break;
            }
        }
        if (non_border >= 3) {
            bottom_line = y;
            break;
        }
    }

    int left_col = 0;
    int max_left_scan = (w * 2) / 5;
    for (int x = 0; x < max_left_scan; x++) {
        int non_border = 0;
        for (int y = top_line; y <= bottom_line; y += 4) {
            Uint16 pix = pixels[y * pitch_words + x];
            if (!is_border_pixel(pix, border_color)) {
                non_border++;
                if (non_border >= 2) break;
            }
        }
        if (non_border >= 2) {
            left_col = x;
            break;
        }
    }

    int right_col = w - 1;
    int min_right_scan = (w * 3) / 5;
    for (int x = w - 1; x >= min_right_scan; x--) {
        int non_border = 0;
        for (int y = top_line; y <= bottom_line; y += 4) {
            Uint16 pix = pixels[y * pitch_words + x];
            if (!is_border_pixel(pix, border_color)) {
                non_border++;
                if (non_border >= 2) break;
            }
        }
        if (non_border >= 2) {
            right_col = x;
            break;
        }
    }

    if (top_line < bottom_line && left_col < right_col) {
        int min_w = (w * 7) / 10;
        int min_h = (h * 7) / 10;

        int det_w = right_col - left_col + 1;
        int det_h = bottom_line - top_line + 1;

        if (det_w < min_w) {
            int pad = (min_w - det_w) / 2;
            left_col -= pad;
            right_col += pad;
        }
        if (det_h < min_h) {
            int pad = (min_h - det_h) / 2;
            top_line -= pad;
            bottom_line += pad;
        }

        if (left_col < 0) left_col = 0;
        if (right_col >= w) right_col = w - 1;
        if (top_line < 0) top_line = 0;
        if (bottom_line >= h) bottom_line = h - 1;

        if (left_col < s_crop.stable_left) s_crop.stable_left = left_col;
        if (right_col > s_crop.stable_right) s_crop.stable_right = right_col;
        if (top_line < s_crop.stable_top) s_crop.stable_top = top_line;
        if (bottom_line > s_crop.stable_bottom) s_crop.stable_bottom = bottom_line;

        bool is_smaller = (left_col > s_crop.stable_left ||
                           right_col < s_crop.stable_right ||
                           top_line > s_crop.stable_top ||
                           bottom_line < s_crop.stable_bottom);

        if (is_smaller) {
            s_crop.shrink_hold_frames++;
            if (s_crop.shrink_hold_frames > 60) {
                if (s_crop.stable_left < left_col) s_crop.stable_left++;
                if (s_crop.stable_right > right_col) s_crop.stable_right--;
                if (s_crop.stable_top < top_line) s_crop.stable_top++;
                if (s_crop.stable_bottom > bottom_line) s_crop.stable_bottom--;
            }
        } else {
            s_crop.shrink_hold_frames = 0;
        }
    }

    if (mainMenu_autoCrop == 1) {
        out_src_rect->x = s_crop.stable_left;
        out_src_rect->y = s_crop.stable_top;
        out_src_rect->w = s_crop.stable_right - s_crop.stable_left + 1;
        out_src_rect->h = s_crop.stable_bottom - s_crop.stable_top + 1;
    } else if (mainMenu_autoCrop == 2) {
        out_src_rect->x = 0;
        out_src_rect->y = s_crop.stable_top;
        out_src_rect->w = w;
        out_src_rect->h = s_crop.stable_bottom - s_crop.stable_top + 1;
    } else if (mainMenu_autoCrop == 3) {
        out_src_rect->x = s_crop.stable_left;
        out_src_rect->y = 0;
        out_src_rect->w = s_crop.stable_right - s_crop.stable_left + 1;
        out_src_rect->h = h;
    } else {
        out_src_rect->x = 0;
        out_src_rect->y = 0;
        out_src_rect->w = w;
        out_src_rect->h = h;
    }

    if (out_src_rect->w <= 0) out_src_rect->w = w;
    if (out_src_rect->h <= 0) out_src_rect->h = h;
}

int isDocked() {
	switch (appletGetOperationMode()) {
		case AppletOperationMode_Handheld:
			return 0;
			break;
		case AppletOperationMode_Console:
			return 1;
			break;
		default:
			return 0;
	}
}

void updateResolution() {
	if (!window || !renderer)
		return;
	int docked = isDocked();
	if ((docked && !currently_docked) || (!docked && currently_docked)) {
		if (docked) {
			display_width = 1920;
			display_height = 1080;
			currently_docked = 1;
		} else {
			display_width = 1280;
			display_height = 720;
			currently_docked = 0;
		}
		SDL_RenderClear(renderer);
		SDL_RenderPresent(renderer);
		SDL_RenderClear(renderer);
		SDL_RenderPresent(renderer);
		SDL_RenderClear(renderer);
		SDL_RenderPresent(renderer);
		SDL_SetWindowSize(window, display_width, display_height);
		SDL_SetVideoModeScaling(0, 0, 0, 0);
	}
}

SDL_Surface *SDL_SetVideoMode(int w, int h, int bpp, int flags) {
	if (currently_docked == -1) {
		if (isDocked()) {
			display_width = 1920;
			display_height = 1080;
			currently_docked = 1;
		} else {
			display_width = 1280;
			display_height = 720;
			currently_docked = 0;
		}
	}

	if (!window) {
		static const Uint32 window_flags[] = { 0, SDL_WINDOW_SHOWN };
		for (size_t i = 0; i < sizeof(window_flags) / sizeof(window_flags[0]); i++) {
			window = SDL_CreateWindow("uae4all2", 0, 0, display_width, display_height, window_flags[i]);
			ps3_write_log("[PS3] CreateWindow flags=0x%x -> %s\n", window_flags[i], window ? "ok" : SDL_GetError());
			if (window)
				break;
		}
	}
	if (window && !renderer) {
		static const Uint32 renderer_flags[] = {
			SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE,
			SDL_RENDERER_ACCELERATED,
			SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE | SDL_RENDERER_PRESENTVSYNC,
			SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE,
			SDL_RENDERER_SOFTWARE,
			0
		};
		for (size_t i = 0; i < sizeof(renderer_flags) / sizeof(renderer_flags[0]); i++) {
			renderer = SDL_CreateRenderer(window, -1, renderer_flags[i]);
			ps3_write_log("[PS3] CreateRenderer flags=0x%x -> %s\n", renderer_flags[i], renderer ? "ok" : SDL_GetError());
			if (renderer)
				break;
		}
	}
	if (window) {
		int window_w = 0;
		int window_h = 0;
		SDL_GetWindowSize(window, &window_w, &window_h);
		if (window_w > 0 && window_h > 0) {
			display_width = window_w;
			display_height = window_h;
		}
	}
	if (!window || !renderer) {
		ps3_write_log("[PS3] SetVideoMode abort: window=%p renderer=%p error=%s\n",
		              (void *)window, (void *)renderer, SDL_GetError());
		return NULL;
	}

	{
		SDL_RendererInfo info;
		extern int emulating;
		if (!emulating) {
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);
			SDL_RenderPresent(renderer);
		}
		if (SDL_GetRendererInfo(renderer, &info) == 0)
			ps3_write_log("[PS3] renderer=%s flags=0x%x driver=%s\n", info.name, info.flags,
			              SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "?");
	}

	SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 16, SDL_PIXELFORMAT_RGB565);
	surface_width = w;
	surface_height = h;
	ps3_write_log("[PS3] SetVideoMode %dx%d bpp=%d surface=%p display=%dx%d\n",
	              w, h, bpp, (void *)surface, display_width, display_height);
	return surface;
}

#ifdef USE_GLES2_SHADER
static GLuint s_lottes_prog = 0;
static GLint s_u_tex = -1;
static GLint s_u_tex_size = -1;
static GLint s_u_in_size = -1;
static GLint s_u_src_off = -1;
static GLint s_u_out_size = -1;
static GLint s_a_pos = -1;
static GLint s_a_texcoord = -1;
static GLuint s_lottes_tex = 0;
static int s_lottes_tex_w = 0;
static int s_lottes_tex_h = 0;

static const char *s_lottes_vs =
"attribute vec2 a_pos;\n"
"attribute vec2 a_texcoord;\n"
"varying vec2 v_texcoord;\n"
"void main() {\n"
"    v_texcoord = a_texcoord;\n"
"    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
"}\n";

static const char *s_lottes_fs =
"precision mediump float;\n"
"uniform sampler2D s_texture;\n"
"uniform vec2 u_texture_size;\n"
"uniform vec2 u_input_size;\n"
"uniform vec2 u_src_offset;\n"
"uniform vec2 u_output_size;\n"
"varying vec2 v_texcoord;\n"
"#define hardScan -8.0\n"
"#define hardPix -3.0\n"
"#define warpX 0.031\n"
"#define warpY 0.041\n"
"#define maskDark 0.5\n"
"#define maskLight 1.5\n"
"vec2 Warp(vec2 pos) {\n"
"    pos = pos * 2.0 - 1.0;\n"
"    pos *= vec2(1.0 + (pos.y * pos.y) * warpX, 1.0 + (pos.x * pos.x) * warpY);\n"
"    return pos * 0.5 + 0.5;\n"
"}\n"
"float ToLinear1(float c) {\n"
"    return (c <= 0.04045) ? c / 12.92 : pow(max(0.0, (c + 0.055) / 1.055), 2.4);\n"
"}\n"
"vec3 ToLinear(vec3 c) {\n"
"    return vec3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b));\n"
"}\n"
"float ToSrgb1(float c) {\n"
"    return (c < 0.0031308) ? c * 12.92 : 1.055 * pow(max(0.0, c), 0.41666) - 0.055;\n"
"}\n"
"vec3 ToSrgb(vec3 c) {\n"
"    return vec3(ToSrgb1(c.r), ToSrgb1(c.g), ToSrgb1(c.b));\n"
"}\n"
"vec3 Fetch(vec2 pos, vec2 off) {\n"
"    vec2 p = floor(pos * u_input_size) + off;\n"
"    if (p.x < 0.0 || p.x >= u_input_size.x || p.y < 0.0 || p.y >= u_input_size.y) return vec3(0.0);\n"
"    vec2 uv = (u_src_offset + p + vec2(0.5, 0.5)) / u_texture_size;\n"
"    return ToLinear(texture2D(s_texture, uv).rgb);\n"
"}\n"
"vec2 Dist(vec2 pos) {\n"
"    vec2 p = pos * u_input_size;\n"
"    return -((p - floor(p)) - vec2(0.5, 0.5));\n"
"}\n"
"float Gaus(float pos, float scale) {\n"
"    return exp2(scale * pos * pos);\n"
"}\n"
"vec3 Horz3(vec2 pos, float off) {\n"
"    vec3 b = Fetch(pos, vec2(-1.0, off));\n"
"    vec3 c = Fetch(pos, vec2( 0.0, off));\n"
"    vec3 d = Fetch(pos, vec2( 1.0, off));\n"
"    float dst = Dist(pos).x;\n"
"    float scale = hardPix;\n"
"    float wb = Gaus(dst - 1.0, scale);\n"
"    float wc = Gaus(dst + 0.0, scale);\n"
"    float wd = Gaus(dst + 1.0, scale);\n"
"    return (b * wb + c * wc + d * wd) / (wb + wc + wd);\n"
"}\n"
"vec3 Horz5(vec2 pos, float off) {\n"
"    vec3 a = Fetch(pos, vec2(-2.0, off));\n"
"    vec3 b = Fetch(pos, vec2(-1.0, off));\n"
"    vec3 c = Fetch(pos, vec2( 0.0, off));\n"
"    vec3 d = Fetch(pos, vec2( 1.0, off));\n"
"    vec3 e = Fetch(pos, vec2( 2.0, off));\n"
"    float dst = Dist(pos).x;\n"
"    float scale = hardPix;\n"
"    float wa = Gaus(dst - 2.0, scale);\n"
"    float wb = Gaus(dst - 1.0, scale);\n"
"    float wc = Gaus(dst + 0.0, scale);\n"
"    float wd = Gaus(dst + 1.0, scale);\n"
"    float we = Gaus(dst + 2.0, scale);\n"
"    return (a * wa + b * wb + c * wc + d * wd + e * we) / (wa + wb + wc + wd + we);\n"
"}\n"
"float Scan(vec2 pos, float off) {\n"
"    float dst = Dist(pos).y;\n"
"    return Gaus(dst + off, hardScan);\n"
"}\n"
"vec3 Tri(vec2 pos) {\n"
"    vec3 a = Horz3(pos, -1.0);\n"
"    vec3 b = Horz5(pos,  0.0);\n"
"    vec3 c = Horz3(pos,  1.0);\n"
"    float wa = Scan(pos, -1.0);\n"
"    float wb = Scan(pos,  0.0);\n"
"    float wc = Scan(pos,  1.0);\n"
"    return a * wa + b * wb + c * wc;\n"
"}\n"
"vec3 Mask(vec2 pos) {\n"
"    vec3 mask = vec3(maskDark, maskDark, maskDark);\n"
"    pos.x += pos.y * 3.0;\n"
"    pos.x = fract(pos.x * 0.166666666);\n"
"    if (pos.x < 0.333) mask.r = maskLight;\n"
"    else if (pos.x < 0.666) mask.g = maskLight;\n"
"    else mask.b = maskLight;\n"
"    return mask;\n"
"}\n"
"void main() {\n"
"    vec2 pos = Warp(v_texcoord);\n"
"    if (pos.x < 0.0 || pos.x > 1.0 || pos.y < 0.0 || pos.y > 1.0) {\n"
"        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
"        return;\n"
"    }\n"
"    vec3 col = Tri(pos);\n"
"    col *= Mask(gl_FragCoord.xy * 1.000001);\n"
"    gl_FragColor = vec4(ToSrgb(col), 1.0);\n"
"}\n";

static GLuint s_crisis_prog = 0;
static GLint s_crisis_u_tex = -1;
static GLint s_crisis_u_tex_size = -1;
static GLint s_crisis_u_in_size = -1;
static GLint s_crisis_u_src_off = -1;
static GLint s_crisis_u_out_size = -1;
static GLint s_crisis_a_pos = -1;
static GLint s_crisis_a_texcoord = -1;

static const char *s_crisis_fs =
"precision mediump float;\n"
"uniform sampler2D s_texture;\n"
"uniform vec2 u_texture_size;\n"
"uniform vec2 u_input_size;\n"
"uniform vec2 u_src_offset;\n"
"uniform vec2 u_output_size;\n"
"varying vec2 v_texcoord;\n"
"vec2 Warp(vec2 pos) {\n"
"    pos = pos * 2.0 - 1.0;\n"
"    float r2 = dot(pos, pos);\n"
"    pos *= 1.0 + vec2(0.035, 0.045) * r2;\n"
"    return pos * 0.5 + 0.5;\n"
"}\n"
"vec3 SampleCRT(vec2 uv, float shift) {\n"
"    float px = 1.0 / u_texture_size.x;\n"
"    float left = texture2D(s_texture, uv - vec2(shift * px, 0.0)).r;\n"
"    float mid = texture2D(s_texture, uv).g;\n"
"    float right = texture2D(s_texture, uv + vec2(shift * px, 0.0)).b;\n"
"    return vec3(left, mid, right);\n"
"}\n"
"void main() {\n"
"    vec2 pos = Warp(v_texcoord);\n"
"    if (pos.x < 0.0 || pos.x > 1.0 || pos.y < 0.0 || pos.y > 1.0) {\n"
"        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
"        return;\n"
"    }\n"
"    vec2 texel = (u_src_offset + pos * u_input_size + vec2(0.5, 0.5)) / u_texture_size;\n"
"    vec3 color = SampleCRT(texel, 0.75);\n"
"    float scan = 0.80 + 0.20 * (0.5 + 0.5 * cos(pos.y * u_input_size.y * 3.14159265));\n"
"    float grille = 0.94 + 0.06 * (0.5 + 0.5 * sin(gl_FragCoord.x * 3.14159265));\n"
"    vec2 curve = v_texcoord * 2.0 - 1.0;\n"
"    float vignette = 1.0 - 0.18 * dot(curve, curve);\n"
"    color *= scan * grille * max(0.72, vignette);\n"
"    gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);\n"
"}\n";

static bool ps3_init_lottes_gl(void) {
	if (s_lottes_prog != 0) return true;
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &s_lottes_vs, NULL);
	glCompileShader(vs);
	GLint ok = 0;
	glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		glDeleteShader(vs);
		return false;
	}
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &s_lottes_fs, NULL);
	glCompileShader(fs);
	glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		glDeleteShader(vs);
		glDeleteShader(fs);
		return false;
	}
	s_lottes_prog = glCreateProgram();
	glAttachShader(s_lottes_prog, vs);
	glAttachShader(s_lottes_prog, fs);
	glLinkProgram(s_lottes_prog);
	glDeleteShader(vs);
	glDeleteShader(fs);
	glGetProgramiv(s_lottes_prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		glDeleteProgram(s_lottes_prog);
		s_lottes_prog = 0;
		return false;
	}
	s_u_tex = glGetUniformLocation(s_lottes_prog, "s_texture");
	s_u_tex_size = glGetUniformLocation(s_lottes_prog, "u_texture_size");
	s_u_in_size = glGetUniformLocation(s_lottes_prog, "u_input_size");
	s_u_src_off = glGetUniformLocation(s_lottes_prog, "u_src_offset");
	s_u_out_size = glGetUniformLocation(s_lottes_prog, "u_output_size");
	s_a_pos = glGetAttribLocation(s_lottes_prog, "a_pos");
	s_a_texcoord = glGetAttribLocation(s_lottes_prog, "a_texcoord");
	return true;
}

static bool ps3_init_crisis_gl(void) {
	if (s_crisis_prog != 0) return true;
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &s_lottes_vs, NULL);
	glCompileShader(vs);
	GLint ok = 0;
	glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		glDeleteShader(vs);
		return false;
	}
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &s_crisis_fs, NULL);
	glCompileShader(fs);
	glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		glDeleteShader(vs);
		glDeleteShader(fs);
		return false;
	}
	s_crisis_prog = glCreateProgram();
	glAttachShader(s_crisis_prog, vs);
	glAttachShader(s_crisis_prog, fs);
	glLinkProgram(s_crisis_prog);
	glDeleteShader(vs);
	glDeleteShader(fs);
	glGetProgramiv(s_crisis_prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		glDeleteProgram(s_crisis_prog);
		s_crisis_prog = 0;
		return false;
	}
	s_crisis_u_tex = glGetUniformLocation(s_crisis_prog, "s_texture");
	s_crisis_u_tex_size = glGetUniformLocation(s_crisis_prog, "u_texture_size");
	s_crisis_u_in_size = glGetUniformLocation(s_crisis_prog, "u_input_size");
	s_crisis_u_src_off = glGetUniformLocation(s_crisis_prog, "u_src_offset");
	s_crisis_u_out_size = glGetUniformLocation(s_crisis_prog, "u_output_size");
	s_crisis_a_pos = glGetAttribLocation(s_crisis_prog, "a_pos");
	s_crisis_a_texcoord = glGetAttribLocation(s_crisis_prog, "a_texcoord");
	return true;
}

static void ps3_render_lottes_gl(SDL_Surface *surface, const SDL_Rect *src_rect, const SDL_Rect *dst_rect, int sw, int sh, int crisis) {
	if (!surface || !surface->pixels || !(crisis ? ps3_init_crisis_gl() : ps3_init_lottes_gl())) return;
	GLuint crt_prog = crisis ? s_crisis_prog : s_lottes_prog;
	GLint crt_u_tex = crisis ? s_crisis_u_tex : s_u_tex;
	GLint crt_u_tex_size = crisis ? s_crisis_u_tex_size : s_u_tex_size;
	GLint crt_u_in_size = crisis ? s_crisis_u_in_size : s_u_in_size;
	GLint crt_u_src_off = crisis ? s_crisis_u_src_off : s_u_src_off;
	GLint crt_u_out_size = crisis ? s_crisis_u_out_size : s_u_out_size;
	GLint crt_a_pos = crisis ? s_crisis_a_pos : s_a_pos;
	GLint crt_a_texcoord = crisis ? s_crisis_a_texcoord : s_a_texcoord;

	GLint prev_prog = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &prev_prog);
	GLint prev_vbo = 0;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_vbo);
	GLint prev_active_tex = 0;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active_tex);
	GLint prev_tex = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
	GLint prev_fbo = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
	GLint prev_vp[4] = {0, 0, sw, sh};
	glGetIntegerv(GL_VIEWPORT, prev_vp);
	GLboolean prev_scissor = glIsEnabled(GL_SCISSOR_TEST);
	GLboolean prev_blend = glIsEnabled(GL_BLEND);
	GLboolean prev_depth = glIsEnabled(GL_DEPTH_TEST);
	GLboolean prev_cull = glIsEnabled(GL_CULL_FACE);

	if (!s_lottes_tex) {
		glGenTextures(1, &s_lottes_tex);
		glBindTexture(GL_TEXTURE_2D, s_lottes_tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		s_lottes_tex_w = 0;
		s_lottes_tex_h = 0;
	} else {
		glBindTexture(GL_TEXTURE_2D, s_lottes_tex);
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	if (s_lottes_tex_w != surface->w || s_lottes_tex_h != surface->h) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, surface->w, surface->h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, surface->pixels);
		s_lottes_tex_w = surface->w;
		s_lottes_tex_h = surface->h;
	} else {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surface->w, surface->h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, surface->pixels);
	}

	int vp_x = dst_rect->x;
	int vp_y = sh - (dst_rect->y + dst_rect->h);
	int vp_w = dst_rect->w;
	int vp_h = dst_rect->h;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	glViewport(0, 0, sw, sh);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glViewport(vp_x, vp_y, vp_w, vp_h);
	glUseProgram(crt_prog);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, s_lottes_tex);
	glUniform1i(crt_u_tex, 0);
	glUniform2f(crt_u_tex_size, (float)surface->w, (float)surface->h);
	glUniform2f(crt_u_in_size, (float)src_rect->w, (float)src_rect->h);
	glUniform2f(crt_u_src_off, (float)src_rect->x, (float)src_rect->y);
	glUniform2f(crt_u_out_size, (float)vp_w, (float)vp_h);

	float quad_data[16] = {
		-1.0f, -1.0f,  0.0f, 1.0f,
		 1.0f, -1.0f,  1.0f, 1.0f,
		-1.0f,  1.0f,  0.0f, 0.0f,
		 1.0f,  1.0f,  1.0f, 0.0f
	};

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if (crt_a_pos >= 0) {
		glEnableVertexAttribArray((GLuint)crt_a_pos);
		glVertexAttribPointer((GLuint)crt_a_pos, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), &quad_data[0]);
	}
	if (crt_a_texcoord >= 0) {
		glEnableVertexAttribArray((GLuint)crt_a_texcoord);
		glVertexAttribPointer((GLuint)crt_a_texcoord, 2, GL_FLOAT, GL_FALSE, 4 * (GLsizei)sizeof(float), &quad_data[2]);
	}

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	if (crt_a_pos >= 0 && crt_a_pos > 1) glDisableVertexAttribArray((GLuint)crt_a_pos);
	if (crt_a_texcoord >= 0 && crt_a_texcoord > 1) glDisableVertexAttribArray((GLuint)crt_a_texcoord);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glUseProgram(prev_prog);
	glBindBuffer(GL_ARRAY_BUFFER, prev_vbo);
	glActiveTexture(prev_active_tex);
	glBindTexture(GL_TEXTURE_2D, prev_tex);
	glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
	glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
	if (prev_scissor) glEnable(GL_SCISSOR_TEST);
	if (prev_blend) glEnable(GL_BLEND);
	if (prev_depth) glEnable(GL_DEPTH_TEST);
	if (prev_cull) glEnable(GL_CULL_FACE);
}

#endif

static bool ps3_init_lottes_gl(void) { return false; }
static bool ps3_init_crisis_gl(void) { return false; }
static void ps3_render_lottes_gl(SDL_Surface *surface, const SDL_Rect *src_rect, const SDL_Rect *dst_rect, int sw, int sh, int crisis) {
	(void)sw; (void)sh; (void)crisis;
	if (!renderer || !surface || !surface->pixels)
		return;
	// Persistent fallback texture (shaders 4/9 without GL): creating +
	// destroying a texture every frame wedges the RSX flip. Reuse it.
	static SDL_Texture *s_fallback_tex = NULL;
	static int s_fallback_w = 0;
	static int s_fallback_h = 0;
	if (!s_fallback_tex || s_fallback_w != surface->w || s_fallback_h != surface->h) {
		if (s_fallback_tex) { SDL_DestroyTexture(s_fallback_tex); s_fallback_tex = NULL; }
		s_fallback_tex = SDL_CreateTexture(renderer, surface->format->format, SDL_TEXTUREACCESS_STREAMING, surface->w, surface->h);
		if (!s_fallback_tex)
			s_fallback_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, surface->w, surface->h);
		if (!s_fallback_tex)
			return;
		s_fallback_w = surface->w;
		s_fallback_h = surface->h;
	}
	SDL_UpdateTexture(s_fallback_tex, NULL, surface->pixels, surface->pitch);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, s_fallback_tex, src_rect, dst_rect);
}
static void ps3_update_crt_mask(int shader_type, int w, int h, int base_h) {
	if (!renderer || w <= 0 || h <= 0) return;
	if (s_mask_texture && s_mask_type == shader_type && s_mask_w == w && s_mask_h == h) return;
	if (s_mask_texture) {
		SDL_DestroyTexture(s_mask_texture);
		s_mask_texture = NULL;
	}
	s_mask_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, w, h);
	if (!s_mask_texture) return;
	SDL_SetTextureBlendMode(s_mask_texture, SDL_BLENDMODE_MOD);

	Uint32 *pixels = (Uint32 *)malloc((size_t)w * (size_t)h * sizeof(Uint32));
	if (!pixels) return;

	int line_h = (base_h > 0) ? (h / base_h) : 2;
	if (line_h < 1) line_h = 1;

	if (shader_type == 4) {
		float center_x = (float)w * 0.5f;
		float center_y = (float)h * 0.5f;
		float max_dist_sq = center_x * center_x + center_y * center_y;
		if (max_dist_sq < 1.0f) max_dist_sq = 1.0f;
		for (int y = 0; y < h; y++) {
			float v = ((float)(y % line_h) + 0.5f) / (float)line_h;
			float dy = fabsf(v - 0.5f) * 2.0f;
			float scan = 0.40f + 0.60f * expf(-dy * dy * 2.5f);
			float ny = (float)y - center_y;
			for (int x = 0; x < w; x++) {
				float nx = (float)x - center_x;
				float d_sq = (nx * nx + ny * ny) / max_dist_sq;
				float vig = 1.0f - 0.15f * d_sq;
				if (vig < 0.75f) vig = 0.75f;
				int triad = x % 3;
				float r = (triad == 0) ? 1.0f : 0.65f;
				float g = (triad == 1) ? 1.0f : 0.65f;
				float b = (triad == 2) ? 1.0f : 0.65f;
			Uint32 cr = (Uint32)(fminf(255.0f, 255.0f * r * scan * vig));
			Uint32 cg = (Uint32)(fminf(255.0f, 255.0f * g * scan * vig));
			Uint32 cb = (Uint32)(fminf(255.0f, 255.0f * b * scan * vig));
			// SDL_PIXELFORMAT_RGBA8888: R=bits24 G=bits16 B=bits8 A=bits0
			pixels[y * w + x] = (cr << 24) | (cg << 16) | (cb << 8) | 0xFF;
			}
		}
	} else if (shader_type == 5) {
		for (int y = 0; y < h; y++) {
			float v = ((float)(y % line_h) + 0.5f) / (float)line_h;
			float dy = fabsf(v - 0.5f) * 2.0f;
			float scan = 0.60f + 0.40f * (1.0f - dy * dy);
			for (int x = 0; x < w; x++) {
			int triad = x % 2;
			float mask = (triad == 0) ? 1.0f : 0.85f;
			Uint32 c = (Uint32)(fminf(255.0f, 255.0f * mask * scan));
			// SDL_PIXELFORMAT_RGBA8888: R=bits24 G=bits16 B=bits8 A=bits0
			pixels[y * w + x] = (c << 24) | (c << 16) | (c << 8) | 0xFF;
			}
		}
	} else if (shader_type == 6) {
		for (int y = 0; y < h; y++) {
			int pos = y % line_h;
			Uint32 c = (pos >= (line_h / 2)) ? 130 : 255;
			// SDL_PIXELFORMAT_RGBA8888: R=bits24 G=bits16 B=bits8 A=bits0
			Uint32 val = (c << 24) | (c << 16) | (c << 8) | 0xFF;
			for (int x = 0; x < w; x++) {
				pixels[y * w + x] = val;
			}
		}
	}
	SDL_UpdateTexture(s_mask_texture, NULL, pixels, w * (int)sizeof(Uint32));
	free(pixels);
	s_mask_type = shader_type;
	s_mask_w = w;
	s_mask_h = h;
}

static void scale2x_rgb565(const Uint16 *src, int src_pitch, Uint16 *dst, int dst_pitch, int w, int h) {
	src_pitch /= (int)sizeof(Uint16);
	dst_pitch /= (int)sizeof(Uint16);
	for (int y = 0; y < h; y++) {
		int ym1 = (y > 0) ? y - 1 : 0;
		int yp1 = (y < h - 1) ? y + 1 : h - 1;
		const Uint16 *row_mid = src + y * src_pitch;
		const Uint16 *row_top = src + ym1 * src_pitch;
		const Uint16 *row_bot = src + yp1 * src_pitch;
		Uint16 *out0 = dst + (y * 2) * dst_pitch;
		Uint16 *out1 = dst + (y * 2 + 1) * dst_pitch;
		for (int x = 0; x < w; x++) {
			int xm1 = (x > 0) ? x - 1 : 0;
			int xp1 = (x < w - 1) ? x + 1 : w - 1;
			Uint16 B = row_top[x];
			Uint16 D = row_mid[xm1];
			Uint16 E = row_mid[x];
			Uint16 F = row_mid[xp1];
			Uint16 H = row_bot[x];

			Uint16 E0 = (D == B && B != F && D != H) ? D : E;
			Uint16 E1 = (B == F && B != D && F != H) ? F : E;
			Uint16 E2 = (D == H && D != B && H != F) ? D : E;
			Uint16 E3 = (H == F && D != H && B != F) ? F : E;

			out0[x * 2]     = E0;
			out0[x * 2 + 1] = E1;
			out1[x * 2]     = E2;
			out1[x * 2 + 1] = E3;
		}
	}
}

void SDL_SetVideoModeScaling(int x, int y, float sw, float sh) {
	if (!renderer)
		return;
	if (!displaying_menu) {
		int aspect_variant = presetModeId % 10;
		bool is_fullscreen = (aspect_variant == 7);
		bool is_five_four = (aspect_variant == 8);
		bool is_sixteen_ten = (aspect_variant == 9);

		if (mainMenu_shader == 8 || (mainMenu_shader == 0 && mainMenu_autoCrop == 0)) {
			int screen_width = visibleAreaWidth;
			int screen_height = mainMenu_displayHires ? (2 * mainMenu_displayedLines) : mainMenu_displayedLines;
			if (screen_height <= 0) screen_height = 240;
			if (screen_width <= 0) screen_width = 320;
			int scale_factor = MIN(display_height / screen_height, display_width / screen_width);
			if (scale_factor < 1) scale_factor = 1;
			scaled_height = scale_factor * screen_height;
			scaled_width = scale_factor * screen_width;
		} else if (is_fullscreen) {
			scaled_width = display_width;
			scaled_height = display_height;
		} else if (is_five_four) {
			scaled_height = display_height;
			scaled_width = (display_height * 5) / 4;
		} else if (is_sixteen_ten) {
			scaled_height = display_height;
			scaled_width = (display_height * 16) / 10;
		} else {
			scaled_height = display_height;
			scaled_width = (display_height * 4) / 3;
		}
		x_offset = (display_width - scaled_width) / 2;
		y_offset = (display_height - scaled_height) / 2;
		if (mainMenu_shader == 1 || mainMenu_shader == 4 || mainMenu_shader == 5 || mainMenu_shader == 6) {
			int base_w = surface_width > 0 ? surface_width : 320;
			int base_h = surface_height > 0 ? surface_height : 240;
			int prescale_factor_x = scaled_width / base_w;
			int prescale_factor_y = scaled_height / base_h;
			if (prescale_factor_x < 1) prescale_factor_x = 1;
			if (prescale_factor_y < 1) prescale_factor_y = 1;
			prescaled_width = prescale_factor_x * base_w;
			prescaled_height = prescale_factor_y * base_h;
		}
	} else {
		scaled_width = display_width;
		scaled_height = display_height;
		x_offset = 0;
		y_offset = 0;
	}
	if (prescaled) {
		SDL_DestroyTexture(prescaled);
		prescaled = NULL;
	}
	if (!displaying_menu && (mainMenu_shader == 1 || mainMenu_shader == 4 || mainMenu_shader == 5 || mainMenu_shader == 6)) {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		prescaled = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, prescaled_width, prescaled_height);
	}
}

void SDL_SetVideoModeBilinear(int value) {
	if (value && (mainMenu_shader != 3) && (mainMenu_shader != 8) && (mainMenu_shader != 0))
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	else
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
}

void SDL_SetVideoModeSync(int value) {
	if (1)
		SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
	else
		SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
}

void ps3_get_game_viewport(int *x, int *y, int *w, int *h) {
	int eff_off_x = 0;
	int eff_off_y = 0;
	if (!displaying_menu) {
		eff_off_x = mainMenu_screenOffsetX;
		eff_off_y = mainMenu_screenOffsetY;
		if (isDocked() && display_width > 0 && display_height > 0) {
			eff_off_x = (mainMenu_screenOffsetX * display_width) / 1280;
			eff_off_y = (mainMenu_screenOffsetY * display_height) / 720;
		}
	}
	if (x) *x = x_offset + eff_off_x;
	if (y) *y = y_offset + eff_off_y;
	if (w) *w = scaled_width;
	if (h) *h = scaled_height;
}

static TTF_Font *s_osd_font = NULL;
static char s_osd_text[128] = {0};
static Uint32 s_osd_start_time = 0;
static Uint32 s_osd_duration = 0;
int ps3_fast_forward = 0;
int ps3_turbo_toggle = 0;

static SDL_Texture *s_turbo_tex = NULL;
static int s_turbo_w = 0, s_turbo_h = 0;
static SDL_Texture *s_osd_tex = NULL;
static char s_osd_cached_text[128] = {0};
static int s_osd_cached_w = 0, s_osd_cached_h = 0;

static SDL_Surface *s_osd_box = NULL;

void ps3_osd_show(const char *text, int duration_ms)
{
	if (!text || !text[0]) {
		s_osd_text[0] = '\0';
		return;
	}
	snprintf(s_osd_text, sizeof(s_osd_text), "%s", text);
	s_osd_start_time = SDL_GetTicks();
	s_osd_duration = (Uint32)(duration_ms > 0 ? duration_ms : 1500);
}

static void ps3_draw_osd_surface(SDL_Surface *surface)
{
	if (!surface)
		return;
	if (s_osd_text[0] == '\0')
		return;
	if (SDL_GetTicks() - s_osd_start_time >= s_osd_duration) {
		s_osd_text[0] = '\0';
		return;
	}
	if (!s_osd_font) {
		TTF_Init();
		const char *osd_font_paths[4] = {
			"./data/font.ttf", "./font.ttf",
			"/dev_hdd0/uae4all/data/font.ttf",
			"/dev_hdd0/game/UAEH00001/USRDIR/data/font.ttf"
		};
		for (int i = 0; i < 4 && !s_osd_font; i++)
			s_osd_font = TTF_OpenFont(osd_font_paths[i], 26);
	}
	if (!s_osd_font)
		return;
	if (strcmp(s_osd_text, s_osd_cached_text) != 0) {
		SDL_Color osd_col = { 255, 255, 255, 255 };
		SDL_Color osd_bg = { 16, 16, 16, 255 };
		SDL_Surface *osd_new = TTF_RenderUTF8_Shaded(s_osd_font, s_osd_text, osd_col, osd_bg);
		if (!osd_new)
			return;
		SDL_Surface *osd_conv = SDL_ConvertSurface(osd_new, surface->format, 0);
		SDL_FreeSurface(osd_new);
		if (!osd_conv)
			return;
		if (s_osd_box)
			SDL_FreeSurface(s_osd_box);
		s_osd_box = osd_conv;
		s_osd_cached_w = s_osd_box->w;
		s_osd_cached_h = s_osd_box->h;
		snprintf(s_osd_cached_text, sizeof(s_osd_cached_text), "%s", s_osd_text);
	}
	if (!s_osd_box)
		return;
	int osd_x = (surface->w - s_osd_cached_w) / 2;
	int osd_y = surface->h - s_osd_cached_h - 4;
	if (osd_x < 2) osd_x = 2;
	if (osd_y < 2) osd_y = 2;
	SDL_Rect osd_rect = { osd_x, osd_y, s_osd_cached_w, s_osd_cached_h };
	SDL_BlitSurface(s_osd_box, NULL, surface, &osd_rect);
}

static void ps3_render_osd_overlays(SDL_Renderer *rend, int sw, int sh)
{
	(void)rend;
	(void)sw;
	(void)sh;
	return;


	if (!s_osd_font) {
		TTF_Init();
		s_osd_font = TTF_OpenFont("./data/font.ttf", 20);
		if (!s_osd_font) s_osd_font = TTF_OpenFont("./font.ttf", 20);
		if (!s_osd_font) s_osd_font = TTF_OpenFont("/dev_hdd0/uae4all/data/font.ttf", 20);
	}

	if (ps3_fast_forward) {
		SDL_Rect ff_rect = { sw - 160, 20, 140, 34 };
		SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(rend, 229, 37, 33, 230);
		SDL_RenderFillRect(rend, &ff_rect);
		SDL_SetRenderDrawColor(rend, 255, 255, 255, 255);
		SDL_RenderDrawRect(rend, &ff_rect);

		if (s_osd_font) {
			if (!s_turbo_tex) {
				SDL_Color col = { 255, 255, 255, 255 };
				SDL_Surface *ts = TTF_RenderUTF8_Blended(s_osd_font, ">> TURBO >>", col);
				if (ts) {
					s_turbo_tex = SDL_CreateTextureFromSurface(rend, ts);
					s_turbo_w = ts->w;
					s_turbo_h = ts->h;
					SDL_FreeSurface(ts);
				}
			}
			if (s_turbo_tex) {
				SDL_Rect tr = { ff_rect.x + (ff_rect.w - s_turbo_w) / 2, ff_rect.y + (ff_rect.h - s_turbo_h) / 2, s_turbo_w, s_turbo_h };
				SDL_RenderCopy(rend, s_turbo_tex, NULL, &tr);
			}
		}
	}

	if (s_osd_text[0] != '\0') {
		Uint32 now = SDL_GetTicks();
		if (now - s_osd_start_time < s_osd_duration) {
			if (s_osd_font) {
				if (!s_osd_tex || strcmp(s_osd_cached_text, s_osd_text) != 0) {
					if (s_osd_tex) {
						SDL_DestroyTexture(s_osd_tex);
						s_osd_tex = NULL;
					}
					SDL_Color col = { 255, 255, 255, 255 };
					SDL_Surface *ts = TTF_RenderUTF8_Blended(s_osd_font, s_osd_text, col);
					if (ts) {
						s_osd_tex = SDL_CreateTextureFromSurface(rend, ts);
						s_osd_cached_w = ts->w;
						s_osd_cached_h = ts->h;
						strncpy(s_osd_cached_text, s_osd_text, sizeof(s_osd_cached_text) - 1);
						s_osd_cached_text[sizeof(s_osd_cached_text) - 1] = '\0';
						SDL_FreeSurface(ts);
					}
				}
			}

			int tw = s_osd_cached_w ? s_osd_cached_w : 200;
			int th = s_osd_cached_h ? s_osd_cached_h : 24;
			int box_w = tw + 36;
			int box_h = th + 14;
			int box_x = (sw - box_w) / 2;
			int box_y = 20;

			SDL_Rect box_rect = { box_x, box_y, box_w, box_h };
			SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
			SDL_SetRenderDrawColor(rend, 18, 24, 38, 230);
			SDL_RenderFillRect(rend, &box_rect);
			SDL_SetRenderDrawColor(rend, 0, 136, 204, 255);
			SDL_RenderDrawRect(rend, &box_rect);

			if (s_osd_tex) {
				SDL_Rect tr = { box_x + 18, box_y + 7, s_osd_cached_w, s_osd_cached_h };
				SDL_RenderCopy(rend, s_osd_tex, NULL, &tr);
			}
		} else {
			if (s_osd_tex) {
				SDL_DestroyTexture(s_osd_tex);
				s_osd_tex = NULL;
			}
			s_osd_cached_text[0] = '\0';
			s_osd_text[0] = '\0';
		}
	} else if (s_osd_tex) {
		SDL_DestroyTexture(s_osd_tex);
		s_osd_tex = NULL;
		s_osd_cached_text[0] = '\0';
	}
}

static SDL_Texture *ps3_texture_from_surface(int slot, SDL_Surface *surface) {
	Ps3TexCache *cache;
	Uint32 fmt;

	if (!renderer || !surface || !surface->pixels)
		return NULL;

	cache = &s_tex_cache[slot & 1];
	fmt = surface->format->format;

	if (!cache->tex || cache->w != surface->w || cache->h != surface->h || cache->fmt != fmt) {
		if (cache->tex) {
			SDL_DestroyTexture(cache->tex);
			cache->tex = NULL;
		}
		cache->tex = SDL_CreateTexture(renderer, fmt, SDL_TEXTUREACCESS_STREAMING, surface->w, surface->h);
		if (!cache->tex) {
			cache->tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, surface->w, surface->h);
		}
		if (!cache->tex) {
			ps3_write_log("[PS3] ps3_texture_from_surface: create failed (%dx%d fmt=0x%x): %s\n",
			              surface->w, surface->h, (unsigned)fmt, SDL_GetError());
			return NULL;
		}
		cache->w = surface->w;
		cache->h = surface->h;
		cache->fmt = fmt;
		ps3_write_log("[PS3] persistent surface texture slot %d: %p (%dx%d fmt=0x%x)\n",
		              slot & 1, (void *)cache->tex, cache->w, cache->h, (unsigned)fmt);
	}

	SDL_UpdateTexture(cache->tex, NULL, surface->pixels, surface->pitch);
	return cache->tex;
}

static void ps3_present_now(const char *what) {
	Uint32 present_start = SDL_GetTicks();

	ps3_diag_checkpoint(what);
	SDL_RenderPresent(renderer);
	ps3_diag_present_time(SDL_GetTicks() - present_start);
	ps3_diag_checkpoint("flip:done");
}

void SDL_Flip(SDL_Surface *surface) {
	ps3_diag_frame();
	ps3_diag_checkpoint("flip:begin");
	update_pad_mode();
	if (surface && renderer && window) {
		updateResolution();

		SDL_Rect src_rect;
		if (!displaying_menu && mainMenu_autoCrop != 0) {
			auto_crop_detect(surface, &src_rect);
		} else {
			src_rect.x = 0;
			src_rect.y = 0;
			src_rect.w = surface->w;
			src_rect.h = surface->h;
		}

		int eff_off_x = 0;
		int eff_off_y = 0;
		if (!displaying_menu) {
			eff_off_x = mainMenu_screenOffsetX;
			eff_off_y = mainMenu_screenOffsetY;
			if (isDocked() && display_width > 0 && display_height > 0) {
				eff_off_x = (mainMenu_screenOffsetX * display_width) / 1280;
				eff_off_y = (mainMenu_screenOffsetY * display_height) / 720;
			}
		}

		SDL_Rect dst_rect = { x_offset + eff_off_x, y_offset + eff_off_y, scaled_width, scaled_height };

		if (!displaying_menu)
			ps3_draw_osd_surface(surface);

		if (displaying_menu) {
			static SDL_Texture *s_menu_tex = NULL;
			static int s_menu_w = 0;
			static int s_menu_h = 0;
			if (!s_menu_tex || s_menu_w != surface->w || s_menu_h != surface->h) {
				if (s_menu_tex) SDL_DestroyTexture(s_menu_tex);
				SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
				s_menu_tex = SDL_CreateTexture(renderer, surface->format->format, SDL_TEXTUREACCESS_STREAMING, surface->w, surface->h);
				if (!s_menu_tex) {
					s_menu_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, surface->w, surface->h);
				}
				s_menu_w = surface->w;
				s_menu_h = surface->h;
				ps3_write_log("[PS3] SDL_Flip menu: created persistent streaming tex=%p (%dx%d fmt=0x%x)\n",
				              (void*)s_menu_tex, s_menu_w, s_menu_h, surface->format->format);
			}
			if (s_menu_tex && surface->pixels) {
				SDL_UpdateTexture(s_menu_tex, NULL, surface->pixels, surface->pitch);
			}
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);
			if (s_menu_tex) {
				SDL_RenderCopy(renderer, s_menu_tex, NULL, &dst_rect);
			}
			ps3_render_osd_overlays(renderer, display_width, display_height);
			ps3_present_now("flip:present(menu)");
			return;
		}

		if (!displaying_menu && (mainMenu_shader == 4 || mainMenu_shader == 9)) {
			ps3_render_lottes_gl(surface, &src_rect, &dst_rect, display_width, display_height, mainMenu_shader == 9);
			ps3_render_osd_overlays(renderer, display_width, display_height);
			ps3_present_now("flip:present(shader)");
		} else if (mainMenu_shader == 1 || mainMenu_shader == 5 || mainMenu_shader == 6) {
			SDL_Texture *tex;

			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
			tex = ps3_texture_from_surface(0, surface);

			int prescaled_ok = (tex != NULL && prescaled != NULL && SDL_SetRenderTarget(renderer, prescaled) == 0);
			if (prescaled_ok) {
				SDL_Rect dst_rect_prescale = { 0, 0, prescaled_width, prescaled_height };
				SDL_RenderCopy(renderer, tex, displaying_menu ? NULL : &src_rect, &dst_rect_prescale);

				if (!displaying_menu && (mainMenu_shader == 5 || mainMenu_shader == 6)) {
					int base_lines = mainMenu_displayHires ? (2 * mainMenu_displayedLines) : mainMenu_displayedLines;
					if (base_lines <= 0) base_lines = 240;
					ps3_update_crt_mask(mainMenu_shader, prescaled_width, prescaled_height, base_lines);
					if (s_mask_texture) {
						SDL_RenderCopy(renderer, s_mask_texture, NULL, &dst_rect_prescale);
					}
				}

				SDL_SetRenderTarget(renderer, NULL);
			}
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);
			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
			if (tex)
				SDL_RenderCopy(renderer, prescaled_ok ? prescaled : tex, prescaled_ok ? NULL : &src_rect, &dst_rect);
			ps3_render_osd_overlays(renderer, display_width, display_height);
			ps3_present_now("flip:present(prescale)");
		} else if (mainMenu_shader == 7 && surface && !displaying_menu) {
			int target_w = surface->w * 2;
			int target_h = surface->h * 2;
			if (!s_scale2x_surf || s_scale2x_surf->w != target_w || s_scale2x_surf->h != target_h) {
				if (s_scale2x_surf) SDL_FreeSurface(s_scale2x_surf);
				s_scale2x_surf = SDL_CreateRGBSurfaceWithFormat(0, target_w, target_h, 16, SDL_PIXELFORMAT_RGB565);
			}
			if (s_scale2x_surf && surface->pixels && s_scale2x_surf->pixels) {
				scale2x_rgb565((const Uint16 *)surface->pixels, surface->pitch,
				               (Uint16 *)s_scale2x_surf->pixels, s_scale2x_surf->pitch,
				               surface->w, surface->h);
				SDL_Rect scaled_src = { src_rect.x * 2, src_rect.y * 2, src_rect.w * 2, src_rect.h * 2 };
				SDL_Texture *scaled_tex;

				SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
				scaled_tex = ps3_texture_from_surface(1, s_scale2x_surf);
				SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
				SDL_RenderClear(renderer);
				if (scaled_tex)
					SDL_RenderCopy(renderer, scaled_tex, &scaled_src, &dst_rect);
				ps3_render_osd_overlays(renderer, display_width, display_height);
				ps3_present_now("flip:present(scale2x)");
			}
		} else {
			SDL_Texture *tex;

			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, (mainMenu_shader == 2) ? "linear" : "nearest");
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);
			tex = ps3_texture_from_surface(0, surface);
			if (tex)
				SDL_RenderCopy(renderer, tex, displaying_menu ? NULL : &src_rect, &dst_rect);
			ps3_render_osd_overlays(renderer, display_width, display_height);
			ps3_present_now("flip:present(game)");
		}
	}
}

SDL_Surface *ps3_load_bmp(const char *file)
{
	if (!file) return NULL;
	SDL_Surface *s = SDL_LoadBMP_RW(SDL_RWFromFile(file, "rb"), 1);
	if (!s) {
		char romfs_path[512];
		if (strncmp(file, "./data/", 7) == 0) {
			snprintf(romfs_path, sizeof(romfs_path), "%s/data/%s", ps3_base_dir(), file + 7);
			s = SDL_LoadBMP_RW(SDL_RWFromFile(romfs_path, "rb"), 1);
		} else if (strncmp(file, "data/", 5) == 0) {
			snprintf(romfs_path, sizeof(romfs_path), "%s/data/%s", ps3_base_dir(), file + 5);
			s = SDL_LoadBMP_RW(SDL_RWFromFile(romfs_path, "rb"), 1);
		}
	}
	return s;
}
#endif