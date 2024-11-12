#include "output.hpp"
#include "includes.hpp"
#include "layout.hpp"
#include "server.hpp"

#include <functional>
#include <algorithm>

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

	std::list<DisplayPtr> displays;

	//MONITOR BEGIN
	wl_listener new_output_listener;
	wlr_scene_output_layout* scene_layout;
	wlr_output_layout* output_layout;
    wlr_scene* scene;

	class Monitor { //represents a Physical Monitor / output
        public:
            Monitor(wlr_output* output);
            void frameNotify();
            void updateState(const wlr_output_state* state);
        private:
            Extends extends;
            wlr_output* output;
			DisplayPtr display = nullptr;
    };

	struct MonitorListeners {
		Monitor* monitor;

		wl_listener frame;
		wl_listener request_state;
		wl_listener destroy;
	};

	Monitor::Monitor(wlr_output* output) {
		this->output = output;

		wlr_output_init_render(output, Server::allocator, Server::renderer);

		//configure output state: ultra basic da chan den so advancte shit ane.
		struct wlr_output_state state;
		struct wlr_output_mode *mode = wlr_output_preferred_mode(output);
		wlr_output_state_init(&state);
		wlr_output_state_set_enabled(&state, true);
		if (mode) wlr_output_state_set_mode(&state, mode);
		wlr_output_commit_state(output, &state);
		wlr_output_state_finish(&state);

		//basic output configuration. das chani den no advanced mache.
		struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(output_layout, output);
		struct wlr_scene_output *scene_output = wlr_scene_output_create(scene, output);
		wlr_scene_output_layout_add_output(scene_layout, l_output, scene_output);

		extends.width = output->width;
		extends.height = output->height;
		extends.x = scene_output->x;
		extends.y = scene_output->y;

		display = new Display(extends);
		displays.push_back(display);

		//TEST
		Buffer* buffer = new Buffer();
		Extends ext = extends;
		ext.height = 30;
		buffer->draw(draw, ext);
	}

	void Monitor::frameNotify() {
		wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, output);
		wlr_scene_output_commit(scene_output, NULL);

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		wlr_scene_output_send_frame_done(scene_output, &now);
	}

	void Monitor::updateState(const wlr_output_state* state) {
		wlr_output_commit_state(output, state);

		extends.width = output->width;
		extends.height = output->height;

		display->updateExtends(extends);
	}

	void backend_new_output_notify(struct wl_listener *listener, void *data) {
		struct wlr_output *wlr_output = (struct wlr_output*) data;

		MonitorListeners* listeners = new MonitorListeners();
		listeners->monitor = new Monitor(wlr_output);

		//Listeners
		listeners->frame.notify = [](struct wl_listener *listener, void *data){
			MonitorListeners* listeners = wl_container_of(listener, listeners, frame);
			listeners->monitor->frameNotify();
		};

		listeners->request_state.notify = [](struct wl_listener *listener, void *data){
			MonitorListeners* listeners = wl_container_of(listener, listeners, request_state);
			const wlr_output_event_request_state *event = (wlr_output_event_request_state*) data;
			listeners->monitor->updateState(event->state);
		};

		listeners->destroy.notify = [](struct wl_listener *listener, void *data){
			MonitorListeners* listeners = wl_container_of(listener, listeners, destroy);
			wl_list_remove(&listeners->frame.link);
			wl_list_remove(&listeners->request_state.link);
			wl_list_remove(&listeners->destroy.link);

			delete listeners->monitor;
			delete listeners;
		};

		wl_signal_add(&wlr_output->events.frame, &listeners->frame);
		wl_signal_add(&wlr_output->events.request_state, &listeners->request_state);
		wl_signal_add(&wlr_output->events.destroy, &listeners->destroy);
	}

    void setup() {
        output_layout = wlr_output_layout_create(Server::display);

        new_output_listener.notify = backend_new_output_notify;
        wl_signal_add(&Server::backend->events.new_output, &new_output_listener);

        scene = wlr_scene_create();
        scene_layout = wlr_scene_attach_output_layout(scene, output_layout);
    }

    void cleanup() {
        wlr_scene_node_destroy(&scene->tree.node);
        wlr_output_layout_destroy(output_layout);
    }

	//MONITOR END





	//DISPALY
	Display::Display(Extends ext) : extends(ext) {
		Extends layout_ext = ext;
		layout_ext.height -= 30;
		layout_ext.y += 30;
		layout = Layout::generateNewLayout(layout_ext);
		monitorCount = 1;
	}

	Display::~Display() {
		auto it = std::find_if(displays.begin(), displays.end(), [this](DisplayPtr ptr){
			return ptr.get() == this; 
		});
		assert(it != displays.end());
		displays.erase(it);
	}

	void Display::updateExtends(Extends ext) {
		extends = ext;
		Extends layout_ext = ext;
		layout_ext.height -= 30;
		layout_ext.y += 30;
		layout->updateExtends(layout_ext);
	}

	//DISPLAY END










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