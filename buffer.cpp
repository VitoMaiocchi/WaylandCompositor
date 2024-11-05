#include "buffer.hpp"
#include <cassert>

struct cairo_buffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
};

static void cairo_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct cairo_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	cairo_surface_destroy(buffer->surface);
	free(buffer);
}

static bool cairo_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
		uint32_t flags, void **data, uint32_t *format, size_t *stride) {
	struct cairo_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);

	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}

	#define fourcc_code(a, b, c, d) ((__u32)(a) | ((__u32)(b) << 8) | \
					((__u32)(c) << 16) | ((__u32)(d) << 24))
	#define DRM_FORMAT_ARGB8888	fourcc_code('A', 'R', '2', '4') /* [31:0] A:R:G:B 8:8:8:8 little endian */

	*format = DRM_FORMAT_ARGB8888;
	*data = cairo_image_surface_get_data(buffer->surface);
	*stride = cairo_image_surface_get_stride(buffer->surface);
	return true;
}

static void cairo_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
}

static const struct wlr_buffer_impl cairo_buffer_impl = {
	.destroy = cairo_buffer_destroy,
	.begin_data_ptr_access = cairo_buffer_begin_data_ptr_access,
	.end_data_ptr_access = cairo_buffer_end_data_ptr_access
};

static struct cairo_buffer *create_cairo_buffer(int width, int height) {
	struct cairo_buffer *buffer = (cairo_buffer*) calloc(1, sizeof(*buffer));
	if (!buffer) {
		return NULL;
	}

	wlr_buffer_init(&buffer->base, &cairo_buffer_impl, width, height);

	buffer->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			width, height);
	if (cairo_surface_status(buffer->surface) != CAIRO_STATUS_SUCCESS) {
		free(buffer);
		return NULL;
	}

	return buffer;
}



Buffer::Buffer(wlr_scene_tree* parent) {
    scene_buffer = wlr_scene_buffer_create(parent, NULL);
	assert(scene_buffer);
}

void Buffer::setPosition(int x, int y) {
    wlr_scene_node_set_position(&scene_buffer->node, x, y);
}

void Buffer::draw(std::function<void(cairo_t*)> draw, uint width, uint height) {

    cairo_buffer* buffer = create_cairo_buffer(width, height);
	assert(buffer);
	cairo_t *cr = cairo_create(buffer->surface);

    draw(cr);

    cairo_destroy(cr);
	wlr_scene_buffer_set_buffer(scene_buffer, &buffer->base);
	wlr_buffer_drop(&buffer->base);
}