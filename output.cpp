#include "output.hpp"
#include "includes.hpp"
#include "layout.hpp"
#include "server.hpp"

#include <functional>

//TEST
void draw(cairo_t* cr) {
	cairo_set_source_rgb(cr, 0.2, 0.8, 0.6);
	cairo_paint(cr);

	cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, 20.0);

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to (cr, 200.0, 25.0);
	cairo_show_text (cr, "Ich habe gerade 100'000 Euro auf bravolotto gewonnen");
}

namespace Output {
	wl_list outputs;
	wl_listener new_output_listener;
	wlr_scene_output_layout* scene_layout;
	wlr_output_layout* output_layout;
    wlr_scene* scene;

	struct WaylandOutput {
		struct wl_list link;
		struct wlr_output *wlr_output;
		struct wl_listener frame;
		struct wl_listener request_state;
		struct wl_listener destroy;

		struct wlr_box extends;
	};

		// OUTPUTS (MONITORS)

	void output_frame_notify(struct wl_listener *listener, void *data) {
		//render frame
		struct WaylandOutput *output = wl_container_of(listener, output, frame);

		struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);
		wlr_scene_output_commit(scene_output, NULL);

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		wlr_scene_output_send_frame_done(scene_output, &now);
	}

	void output_request_state_notify(struct wl_listener *listener, void *data) {
		//output state (resolution etc) changes
		struct WaylandOutput *output = wl_container_of(listener, output, request_state);
		const struct wlr_output_event_request_state *event = (wlr_output_event_request_state*) data;
		wlr_output_commit_state(output->wlr_output, event->state);

		output->extends.width = output->wlr_output->width;
		output->extends.height = output->wlr_output->height;

		//layout_init(output->extends);
		Layout::setScreenExtends(output->extends);
	}

	void output_destroy_notify(struct wl_listener *listener, void *data) {
		struct WaylandOutput *output = wl_container_of(listener, output, destroy);

		wl_list_remove(&output->frame.link);
		wl_list_remove(&output->request_state.link);
		wl_list_remove(&output->destroy.link);
		wl_list_remove(&output->link);
		free(output);
	}

	void backend_new_output_notify(struct wl_listener *listener, void *data) {
		struct wlr_output *wlr_output = (struct wlr_output*) data;

		wlr_output_init_render(wlr_output, Server::allocator, Server::renderer);

		//configure output state: ultra basic da chan den so advancte shit ane.
		struct wlr_output_state state;
		struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
		wlr_output_state_init(&state);
		wlr_output_state_set_enabled(&state, true);
		if (mode) wlr_output_state_set_mode(&state, mode);
		wlr_output_commit_state(wlr_output, &state);
		wlr_output_state_finish(&state);

		//create struct
		struct WaylandOutput *output = (WaylandOutput*) calloc(1, sizeof(*output));
		output->wlr_output = wlr_output;
		wl_list_insert(&outputs, &output->link);

		//Listeners
		output->frame.notify = output_frame_notify;
		output->request_state.notify = output_request_state_notify;
		output->destroy.notify = output_destroy_notify;
		wl_signal_add(&wlr_output->events.frame, &output->frame);
		wl_signal_add(&wlr_output->events.request_state, &output->request_state);
		wl_signal_add(&wlr_output->events.destroy, &output->destroy);


		//basic output configuration. das chani den no advanced mache.
		struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(output_layout, wlr_output);
		struct wlr_scene_output *scene_output = wlr_scene_output_create(scene, wlr_output);
		wlr_scene_output_layout_add_output(scene_layout, l_output, scene_output);

		output->extends.width = wlr_output->width;
		output->extends.height = wlr_output->height;
		output->extends.x = scene_output->x;
		output->extends.y = scene_output->y;

		//layout_init(output->extends);
		Layout::setScreenExtends(output->extends);

		//TEST
		Buffer* buffer = new Buffer();
		Extends ext = output->extends;
		ext.height = 30;
		buffer->draw(draw, ext);
	}

    void setup() {
        output_layout = wlr_output_layout_create(Server::display);
        wl_list_init(&outputs);
        new_output_listener.notify = backend_new_output_notify;
        wl_signal_add(&Server::backend->events.new_output, &new_output_listener);

        scene = wlr_scene_create();
        scene_layout = wlr_scene_attach_output_layout(scene, output_layout);
    }

    void cleanup() {
        wlr_scene_node_destroy(&scene->tree.node);
        wlr_output_layout_destroy(output_layout);
    }



	//BUFFER
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



	Buffer::Buffer() {
		scene_buffer = wlr_scene_buffer_create(&scene->tree, NULL);
		assert(scene_buffer);
	}

	Buffer::~Buffer() {
		//TODO
	}

	void Buffer::draw(std::function<void(cairo_t*)> draw, Extends ext) {
		wlr_scene_node_set_position(&scene_buffer->node, ext.x, ext.y);

		cairo_buffer* buffer = create_cairo_buffer(ext.width, ext.height);
		assert(buffer);
		cairo_t *cr = cairo_create(buffer->surface);

		draw(cr);

		cairo_destroy(cr);
		wlr_scene_buffer_set_buffer(scene_buffer, &buffer->base);
		wlr_buffer_drop(&buffer->base);
	}
}