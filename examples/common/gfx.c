#include "sokol_gfx.h"
#include "sokol_app.h"
#include "sokol_debugtext.h"
#include "sokol_gl.h"
#include "sokol_audio.h"
#include "sokol_glue.h"
#include "gfx.h"
#include "shaders.glsl.h"
#include <assert.h>
#include <stdlib.h> // malloc/free
#include <stdalign.h>

#define _GFX_DEF(v,def) (v?v:def)

typedef struct {
    bool valid;
    gfx_border_t border;
    struct {
        sg_image img;       // framebuffer texture, RGBA8 or R8 if paletted
        sg_image pal_img;   // optional color palette texture
        gfx_dim_t size;
        bool paletted;
    } fb;
    struct {
        gfx_rect_t view;
        gfx_dim_t pixel_aspect;
        sg_image img;
        sg_buffer vbuf;
        sg_pipeline pip;
        sg_pass pass;
        sg_pass_action pass_action;
    } offscreen;
    struct {
        sg_buffer vbuf;
        sg_pipeline pip;
        sg_pass_action pass_action;
        bool portrait;
    } display;
    struct {
        sg_image img;
        sgl_pipeline pip;
        gfx_dim_t size;
    } icon;
    int flash_success_count;
    int flash_error_count;

    uint32_t palette_buffer[256];
    alignas(64) uint8_t pixel_buffer[GFX_MAX_FB_WIDTH * GFX_MAX_FB_HEIGHT * 4];
    void (*draw_extra_cb)(void);
} gfx_state_t;
static gfx_state_t state;

static const float gfx_verts[] = {
    0.0f, 0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f
};
static const float gfx_verts_rot[] = {
    0.0f, 0.0f, 1.0f, 0.0f,
    1.0f, 0.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 0.0f, 1.0f
};
static const float gfx_verts_flipped[] = {
    0.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    1.0f, 1.0f, 1.0f, 0.0f
};
static const float gfx_verts_flipped_rot[] = {
    0.0f, 0.0f, 1.0f, 1.0f,
    1.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 0.0f, 0.0f
};

// a bit-packed speaker-off icon
static const struct {
    int width;
    int height;
    int stride;
    uint8_t pixels[350];
} speaker_icon = {
    .width = 50,
    .height = 50,
    .stride = 7,
    .pixels = {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x80,0x00,0x00,0x00,0x00,0x00,0x00,
        0xC0,0x01,0x00,0x00,0x00,0x00,0x00,
        0xE0,0x03,0x40,0x00,0x00,0x00,0x00,
        0xF0,0x07,0x60,0x00,0x80,0x03,0x00,
        0xE0,0x0F,0x70,0x00,0xC0,0x07,0x00,
        0xC0,0x1F,0x78,0x00,0xE0,0x07,0x00,
        0x80,0x3F,0x78,0x00,0xE0,0x0F,0x00,
        0x00,0x7F,0x78,0x00,0xC0,0x1F,0x00,
        0x00,0xFE,0x70,0x00,0x80,0x1F,0x00,
        0x00,0xFC,0x61,0x00,0x8E,0x3F,0x00,
        0x00,0xF8,0x43,0x00,0x1F,0x3F,0x00,
        0x00,0xF0,0x07,0x80,0x1F,0x7E,0x00,
        0x00,0xF0,0x0F,0x80,0x3F,0x7E,0x00,
        0x00,0xF8,0x1F,0x00,0x3F,0x7C,0x00,
        0xF0,0xFF,0x3F,0x00,0x7E,0xFC,0x00,
        0xF8,0xFF,0x7F,0x00,0x7E,0xFC,0x00,
        0xFC,0x7F,0xFE,0x00,0xFC,0xF8,0x00,
        0xFC,0x3F,0xFC,0x01,0xFC,0xF8,0x00,
        0xFC,0x1F,0xFC,0x03,0xF8,0xF8,0x00,
        0x7C,0x00,0xFC,0x07,0xF8,0xF8,0x00,
        0x7C,0x00,0xFC,0x0F,0xF8,0xF8,0x00,
        0x7C,0x00,0xFC,0x1F,0xF8,0xF8,0x00,
        0x7C,0x00,0xFC,0x3F,0xF8,0xF8,0x00,
        0xFC,0x1F,0x7C,0x7F,0xF8,0xF8,0x00,
        0xFC,0x3F,0x7C,0xFE,0xF0,0xF8,0x00,
        0xFC,0x7F,0x7C,0xFC,0xE1,0xF8,0x00,
        0xF8,0xFF,0x7C,0xF8,0x03,0xFC,0x00,
        0xE0,0xFF,0x7D,0xF0,0x07,0xFC,0x00,
        0x00,0xF8,0x7F,0xE0,0x0F,0x7C,0x00,
        0x00,0xF0,0x7F,0xC0,0x1F,0x7C,0x00,
        0x00,0xE0,0x7F,0x80,0x3F,0x7C,0x00,
        0x00,0xC0,0x7F,0x00,0x7F,0x38,0x00,
        0x00,0x80,0x7F,0x00,0xFE,0x30,0x00,
        0x00,0x00,0x7F,0x00,0xFC,0x01,0x00,
        0x00,0x00,0x7E,0x00,0xF8,0x03,0x00,
        0x00,0x00,0x7C,0x00,0xF0,0x07,0x00,
        0x00,0x00,0x78,0x00,0xE0,0x0F,0x00,
        0x00,0x00,0x70,0x00,0xC0,0x1F,0x00,
        0x00,0x00,0x60,0x00,0x80,0x3F,0x00,
        0x00,0x00,0x40,0x00,0x00,0x1F,0x00,
        0x00,0x00,0x00,0x00,0x00,0x0E,0x00,
        0x00,0x00,0x00,0x00,0x00,0x04,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    }
};

void gfx_flash_success(void) {
    assert(state.valid);
    state.flash_success_count = 20;
}

void gfx_flash_error(void) {
    assert(state.valid);
    state.flash_error_count = 20;
}

void* gfx_framebuffer_ptr(void) {
    assert(state.valid);
    return state.pixel_buffer;
}

size_t gfx_framebuffer_size(void) {
    assert(state.valid);
    return sizeof(state.pixel_buffer);
}

// this function will be called at init time and when the emulator framebuffer size changes
static void gfx_init_images_and_pass(void) {
    // destroy previous resources (if exist)
    sg_destroy_image(state.fb.img);
    sg_destroy_image(state.offscreen.img);
    sg_destroy_pass(state.offscreen.pass);

    // a texture with the emulator's raw pixel data
    state.fb.img = sg_make_image(&(sg_image_desc){
        .width = state.fb.size.width,
        .height = state.fb.size.height,
        .pixel_format = state.fb.paletted ? SG_PIXELFORMAT_R8 : SG_PIXELFORMAT_RGBA8,
        .usage = SG_USAGE_STREAM,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });

    // 2x-upscaling render target textures and passes
    state.offscreen.img = sg_make_image(&(sg_image_desc){
        .render_target = true,
        .width = 2 * state.offscreen.view.width,
        .height = 2 * state.offscreen.view.height,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });
    state.offscreen.pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = state.offscreen.img
    });
}

void gfx_init(const gfx_desc_t* desc) {
    sg_setup(&(sg_desc){
        .buffer_pool_size = 32,
        .image_pool_size = 128,
        .shader_pool_size = 16,
        .pipeline_pool_size = 16,
        .context_pool_size = 2,
        .context = sapp_sgcontext()
    });
    sgl_setup(&(sgl_desc_t){
        .max_vertices = 16,
        .max_commands = 16,
        .context_pool_size = 1,
        .pipeline_pool_size = 16,
    });
    sdtx_setup(&(sdtx_desc_t){
        .context_pool_size = 1,
        .fonts[0] = sdtx_font_z1013(),
        .fonts[1] = sdtx_font_kc853()
    });

    state.valid = true;
    state.border = desc->border;
    state.display.portrait = desc->portrait;
    state.draw_extra_cb = desc->draw_extra_cb;
    state.fb.size =  (gfx_dim_t){0};
    state.fb.paletted = 0 != desc->palette.ptr;

    if (state.fb.paletted) {
        assert((desc->palette.ptr == 0) || (desc->palette.ptr && (desc->palette.size > 0) && (desc->palette.size < sizeof(state.palette_buffer))));
        memcpy(state.palette_buffer, desc->palette.ptr, desc->palette.size);
        state.fb.pal_img = sg_make_image(&(sg_image_desc){
            .width = 256,
            .height = 1,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = SG_FILTER_NEAREST,
            .mag_filter = SG_FILTER_NEAREST,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
            .data = {
                .subimage[0][0] = { .ptr = state.palette_buffer, .size = sizeof(state.palette_buffer) },
            }
        });
    }

    state.offscreen.pixel_aspect.width = _GFX_DEF(desc->pixel_aspect.width, 1);
    state.offscreen.pixel_aspect.height = _GFX_DEF(desc->pixel_aspect.height, 1);
    state.offscreen.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_DONTCARE }
    };
    state.offscreen.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(gfx_verts)
    });

    sg_shader shd = {0};
    if (state.fb.paletted) {
        shd = sg_make_shader(offscreen_pal_shader_desc(sg_query_backend()));
    }
    else {
        shd = sg_make_shader(offscreen_shader_desc(sg_query_backend()));
    }
    state.offscreen.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout = {
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT2,
                [1].format = SG_VERTEXFORMAT_FLOAT2
            }
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .depth.pixel_format = SG_PIXELFORMAT_NONE
    });

    state.display.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.05f, 0.05f, 0.05f, 1.0f } }
    };
    state.display.vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = {
            .ptr = sg_query_features().origin_top_left ?
                   (state.display.portrait ? gfx_verts_rot : gfx_verts) :
                   (state.display.portrait ? gfx_verts_flipped_rot : gfx_verts_flipped),
            .size = sizeof(gfx_verts)
        }
    });

    state.display.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(display_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [0].format = SG_VERTEXFORMAT_FLOAT2,
                [1].format = SG_VERTEXFORMAT_FLOAT2
            }
        },
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP
    });

    // create an unpacked speaker icon image and sokol-gl pipeline
    {
        // textures must be 2^n for WebGL
        state.icon.size.width = speaker_icon.width;
        state.icon.size.height = speaker_icon.height;
        const size_t pixel_data_size = state.icon.size.width * state.icon.size.height * sizeof(uint32_t);
        uint32_t* pixels = malloc(pixel_data_size);
        assert(pixels);
        memset(pixels, 0, pixel_data_size);
        const uint8_t* src = speaker_icon.pixels;
        uint32_t* dst = pixels;
        for (int y = 0; y < state.icon.size.height; y++) {
            uint8_t bits = 0;
            dst = pixels + (y * state.icon.size.width);
            for (int x = 0; x < state.icon.size.width; x++) {
                if ((x & 7) == 0) {
                    bits = *src++;
                }
                if (bits & 1) {
                    *dst++ = 0xFFFFFFFF;
                }
                else {
                    *dst++ = 0x00FFFFFF;
                }
                bits >>= 1;
            }
        }
        assert(src == speaker_icon.pixels + speaker_icon.stride * speaker_icon.height);
        assert(dst <= pixels + (state.icon.size.width * state.icon.size.height));
        state.icon.img = sg_make_image(&(sg_image_desc){
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .width = state.icon.size.width,
            .height = state.icon.size.height,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
            .data.subimage[0][0] = { .ptr=pixels, .size=pixel_data_size }
        });
        free(pixels);

        // sokol-gl pipeline for alpha-blended rendering
        state.icon.pip = sgl_make_pipeline(&(sg_pipeline_desc){
            .colors[0] = {
                .write_mask = SG_COLORMASK_RGB,
                .blend = {
                    .enabled = true,
                    .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
                    .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                }
            }
        });
    }
}

/* apply a viewport rectangle to preserve the emulator's aspect ratio,
   and for 'portrait' orientations, keep the emulator display at the
   top, to make room at the bottom for mobile virtual keyboard
*/
static void apply_viewport(gfx_dim_t canvas) {
    float cw = (float) (canvas.width - state.border.left - state.border.right);
    if (cw < 1.0f) {
        cw = 1.0f;
    }
    float ch = (float) (canvas.height - state.border.top - state.border.bottom);
    if (ch < 1.0f) {
        ch = 1.0f;
    }
    const float canvas_aspect = (float)cw / (float)ch;
    const gfx_rect_t view = state.offscreen.view;
    const gfx_dim_t aspect = state.offscreen.pixel_aspect;
    const float emu_aspect = (float)(view.width * aspect.width) / (float)(view.height * aspect.height);
    float vp_x, vp_y, vp_w, vp_h;
    if (emu_aspect < canvas_aspect) {
        vp_y = (float)state.border.top;
        vp_h = ch;
        vp_w = (ch * emu_aspect);
        vp_x = state.border.left + (cw - vp_w) / 2;
    }
    else {
        vp_x = (float)state.border.left;
        vp_w = cw;
        vp_h = (cw / emu_aspect);
        vp_y = (float)state.border.top;
    }
    sg_apply_viewportf(vp_x, vp_y, vp_w, vp_h, true);
}

void gfx_draw(const gfx_draw_t* args) {
    assert(state.valid);
    assert(args);
    assert((args->fb.width > 0) && (args->fb.height > 0));
    assert((args->view.width > 0) && (args->view.height > 0));
    const gfx_dim_t display = { .width = sapp_width(), .height = sapp_height() };

    state.offscreen.view = args->view;

    // check if emulator framebuffer size has changed, need to create new backing texture
    if ((args->fb.width != state.fb.size.width) || (args->fb.height != state.fb.size.height)) {
        state.fb.size = args->fb;
        gfx_init_images_and_pass();
    }

    // if audio is off, draw speaker icon via sokol-gl
    if (saudio_suspended()) {
        const float x0 = display.width - (float)state.icon.size.width - 10.0f;
        const float x1 = x0 + (float)state.icon.size.width;
        const float y0 = 10.0f;
        const float y1 = y0 + (float)state.icon.size.height;
        const float alpha = (sapp_frame_count() & 0x20) ? 0.0f : 1.0f;
        sgl_defaults();
        sgl_enable_texture();
        sgl_texture(state.icon.img);
        sgl_matrix_mode_projection();
        sgl_ortho(0.0f, (float)display.width, (float)display.height, 0.0f, -1.0f, +1.0f);
        sgl_c4f(1.0f, 1.0f, 1.0f, alpha);
        sgl_load_pipeline(state.icon.pip);
        sgl_begin_quads();
        sgl_v2f_t2f(x0, y0, 0.0f, 0.0f);
        sgl_v2f_t2f(x1, y0, 1.0f, 0.0f);
        sgl_v2f_t2f(x1, y1, 1.0f, 1.0f);
        sgl_v2f_t2f(x0, y1, 0.0f, 1.0f);
        sgl_end();
    }

    // copy emulator pixel data into emulator framebuffer texture
    const size_t bytes_per_pixel = state.fb.paletted ? 1 : 4;
    sg_update_image(state.fb.img, &(sg_image_data){
        .subimage[0][0] = {
            .ptr = state.pixel_buffer,
            .size = state.fb.size.width * state.fb.size.height * bytes_per_pixel,
        }
    });

    // upscale the original framebuffer 2x with nearest filtering
    sg_begin_pass(state.offscreen.pass, &state.offscreen.pass_action);
    sg_apply_pipeline(state.offscreen.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.offscreen.vbuf,
        .fs_images[SLOT_fb_tex] = state.fb.img,
        .fs_images[SLOT_pal_tex] = state.fb.pal_img,
    });
    const offscreen_vs_params_t vs_params = {
        .uv_offset = {
            (float)state.offscreen.view.x / (float)state.fb.size.width,
            (float)state.offscreen.view.y / (float)state.fb.size.height,
        },
        .uv_scale = {
            (float)state.offscreen.view.width / (float)state.fb.size.width,
            (float)state.offscreen.view.height / (float)state.fb.size.height
        }
    };
    sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_offscreen_vs_params, &SG_RANGE(vs_params));
    sg_draw(0, 4, 1);
    sg_end_pass();

    // tint the clear color red or green if flash feedback is requested
    if (state.flash_error_count > 0) {
        state.flash_error_count--;
        state.display.pass_action.colors[0].value.r = 0.7f;
    }
    else if (state.flash_success_count > 0) {
        state.flash_success_count--;
        state.display.pass_action.colors[0].value.g = 0.7f;
    }
    else {
        state.display.pass_action.colors[0].value.r = 0.05f;
        state.display.pass_action.colors[0].value.g = 0.05f;
    }

    // draw the final pass with linear filtering
    sg_begin_default_pass(&state.display.pass_action, display.width, display.height);
    apply_viewport(display);
    sg_apply_pipeline(state.display.pip);
    sg_apply_bindings(&(sg_bindings){
        .vertex_buffers[0] = state.display.vbuf,
        .fs_images[SLOT_tex] = state.offscreen.img,
    });
    sg_draw(0, 4, 1);
    sg_apply_viewport(0, 0, display.width, display.height, true);
    sdtx_draw();
    sgl_draw();
    if (state.draw_extra_cb) {
        state.draw_extra_cb();
    }
    sg_end_pass();
    sg_commit();
}

void gfx_shutdown() {
    assert(state.valid);
    sgl_shutdown();
    sdtx_shutdown();
    sg_shutdown();
}

void* gfx_create_texture(gfx_dim_t size) {
    sg_image img = sg_make_image(&(sg_image_desc){
        .width = size.width,
        .height = size.height,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage = SG_USAGE_STREAM,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE
    });
    return (void*)(uintptr_t)img.id;
}

void gfx_update_texture(void* h, void* data, int data_byte_size) {
    sg_image img = { .id=(uint32_t)(uintptr_t)h };
    sg_update_image(img, &(sg_image_data){.subimage[0][0] = { .ptr = data, .size=data_byte_size } });
}

void gfx_destroy_texture(void* h) {
    sg_image img = { .id=(uint32_t)(uintptr_t)h };
    sg_destroy_image(img);
}