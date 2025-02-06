#include <wireplumber-0.5/wp/wp.h>
#include <spa-0.2/spa/utils/defs.h>
#include <spa-0.2/spa/utils/string.h>
#include <pipewire-0.3/pipewire/pipewire.h>
#include <pipewire-0.3/pipewire/keys.h>
#include <pipewire-0.3/pipewire/extensions/session-manager/keys.h>

#define LOGGER_CATEGORY Logger::OUTPUT
#include "output.hpp"
#include "includes.hpp"
#include "layout.hpp"
#include "server.hpp"

#include <functional>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cmath>
#include <atomic>

//mainloop sleep time in ms
//to safe resources: audio is on a seperate thread and
//volume updates don't need to be extremly responsive
#define AUDIO_MAINLOOP_REFRESHTIME 10

namespace Output {

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
			std::shared_ptr<Display> display;
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

		display = std::make_shared<Display>(extends);
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

	void setupPipeWire();
	//TODO: cleanup Pipewire

    void setup() {
        output_layout = wlr_output_layout_create(Server::display);

        new_output_listener.notify = backend_new_output_notify;
        wl_signal_add(&Server::backend->events.new_output, &new_output_listener);

        scene = wlr_scene_create();
        scene_layout = wlr_scene_attach_output_layout(scene, output_layout);

		setupPipeWire();
    }

    void cleanup() {
        wlr_scene_node_destroy(&scene->tree.node);
        wlr_output_layout_destroy(output_layout);

		//cleanup pipewire
    }

	//MONITOR END



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


	//PIPEWIRE / WIREPLUMBER

	#define MAXINT32	(0x7fffffff)
	#define MAXUINT32	(0xffffffff)

	std::atomic<bool> mainloop_running = false;
	std::atomic<bool> mute_toggle = false;
	std::atomic<double> volume_change = 0.0;

	std::atomic<double> current_volume = 1.0f;
	std::atomic<bool> current_mute = false;

	void adjustVolume(double vol) {
		volume_change += vol;
	}

	void muteVolume() {
		mute_toggle = !mute_toggle;
	}

	void synchronizedVolumeUpdate(void* data) {
		const double volume = current_volume;
		debug("volume updated: volume={:.2f}", volume);
	 	Titlebar::updateVolume(std::round(volume*100));
	}

	void synchronizedMuteUpdate(void* data) {
		const bool mute = current_mute;
		debug("audio mute status updated: mute={}", mute);
		Titlebar::updateAudioMute(mute);
	}

	struct WpData {
		GOptionContext* gcontext;
		GMainLoop* loop;
		WpCore* core;
		WpObjectManager* om;
		uint pending_plugins = 0;

		WpPlugin* def_nodes_api = nullptr;
		WpPlugin* mixer_api = nullptr;

		bool ready = false;

		struct {
			double volume = 1.0;
			bool muted = false;
			uint32_t id = 0;
		} defaultSink;
	};

	int32_t get_default_sink_id(WpPlugin* def_nodes_api) {
		int32_t res = -1;
		g_signal_emit_by_name (def_nodes_api, "get-default-node", "Audio/Sink", &res); //"Audio/Source" für source
		if(res <= 0 || res >= MAXUINT32) error("default node id is not valid");
		return res;
	}

	void update_volume(WpData* wpd) {
		GVariant *variant = NULL;
		gboolean mute = FALSE;
		gdouble volume = 1.0;

		g_signal_emit_by_name (wpd->mixer_api, "get-volume", wpd->defaultSink.id, &variant);
		if (!variant) {
			error("Node {} does not support volume", wpd->defaultSink.id);
			return;
		}

		g_variant_lookup (variant, "volume", "d", &volume);
		g_variant_lookup (variant, "mute", "b", &mute);
		g_clear_pointer (&variant, g_variant_unref);


		if(volume != wpd->defaultSink.volume) {
			wpd->defaultSink.volume = volume;
			current_volume = volume;
			Server::queueEvent(synchronizedVolumeUpdate, nullptr);
		}

		if(mute != wpd->defaultSink.muted) {
			wpd->defaultSink.muted = mute;
			current_mute = mute;
			Server::queueEvent(synchronizedMuteUpdate, nullptr);
		}
	}

	void set_volume(WpData* wpd, double volume) {
		GVariantBuilder b = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_VARDICT);
		GVariant *variant = NULL;
		gboolean res = FALSE;

		g_variant_builder_add (&b, "{sv}", "volume", g_variant_new_double(volume));
		variant = g_variant_builder_end (&b);

		g_signal_emit_by_name (wpd->mixer_api, "set-volume", wpd->defaultSink.id, variant, &res);
		if (!res) error("Node {} does not support volume", wpd->defaultSink.id);
	}

	void set_mute(WpData* wpd, bool mute) {
		GVariantBuilder b = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_VARDICT);
		GVariant *variant = NULL;
		gboolean res = FALSE;

		g_variant_builder_add (&b, "{sv}", "mute", g_variant_new_boolean(mute));
		variant = g_variant_builder_end (&b);

		g_signal_emit_by_name (wpd->mixer_api, "set-volume", wpd->defaultSink.id, variant, &res);
		if (!res) error("Node {} does not support volume", wpd->defaultSink.id);
	}

	void default_node_update_notify(GObject *emitter, guint arg, WpData* wpd) {
		uint32_t id = get_default_sink_id(wpd->def_nodes_api);
		if(id != wpd->defaultSink.id) {
			debug("default audio node changed: new_id={}",id);
			wpd->defaultSink.id = id;
			update_volume(wpd);
		}
	}

	void mixer_update_notify(GObject *emitter, guint arg, WpData* wpd) {
		update_volume(wpd);
	}

	void wireplumber_installed_notify(WpData* wpd) {
		if(!wpd->def_nodes_api) error("defnodes not loaded");
		if(!wpd->mixer_api) error("mixer not loaded");
		
		wpd->defaultSink.id = get_default_sink_id(wpd->def_nodes_api);
		update_volume(wpd);

		g_signal_connect(wpd->def_nodes_api, "changed", G_CALLBACK(default_node_update_notify), wpd);
		g_signal_connect(wpd->mixer_api, "changed", G_CALLBACK(mixer_update_notify), wpd);

		wpd->ready = true;
	}

	void wireplumber_disconnect_notify(void* data) {
		warn("wireplumber disconnected");
		mainloop_running = false;
	}

	void on_plugin_loaded (WpCore * core, GAsyncResult * res, WpData* wpd) {
		GError *error = NULL;
		if (!wp_core_load_component_finish (core, res, &error)) {
			error("error loading wireplumber plugin errormessage=\"{}\"", error->message);
			mainloop_running = false;
			return;
		}

		if (--wpd->pending_plugins == 0) {
			info("wireplumber plugins loaded");
			wpd->def_nodes_api = wp_plugin_find(core, "default-nodes-api");
			wpd->mixer_api  = wp_plugin_find(core, "mixer-api");
			g_object_set (wpd->mixer_api, "scale", 1 /* cubic */, NULL);
			wp_core_install_object_manager (core, wpd->om);
		}
	}

	void wireplumber_run_mainloop() {
		WpData wpd;

		wp_init (WP_INIT_ALL);
		wpd.gcontext = g_option_context_new ("Vito Manager");
		wpd.loop = g_main_loop_new (NULL, FALSE);
		wpd.core = wp_core_new (NULL, NULL, NULL);
		wpd.om = wp_object_manager_new ();

		wp_object_manager_add_interest (wpd.om, WP_TYPE_NODE, NULL);
		wp_object_manager_request_object_features (wpd.om, WP_TYPE_GLOBAL_PROXY, WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);

		 /* load required API modules */
		wpd.pending_plugins++;
		wp_core_load_component (wpd.core, "libwireplumber-module-default-nodes-api",
			"module", NULL, NULL, NULL, (GAsyncReadyCallback) on_plugin_loaded, &wpd);
		wpd.pending_plugins++;
		wp_core_load_component (wpd.core, "libwireplumber-module-mixer-api",
			"module", NULL, NULL, NULL, (GAsyncReadyCallback) on_plugin_loaded, &wpd);

		/* connect */
		if (!wp_core_connect (wpd.core)) {
			fprintf (stderr, "Could not connect to PipeWire\n");
			error("Could not connect to PipeWire");
			return;
		}

		/* run */
		g_signal_connect_swapped (wpd.core, "disconnected",
			(GCallback) wireplumber_disconnect_notify, nullptr);
		g_signal_connect_swapped (wpd.om, "installed",
			(GCallback) wireplumber_installed_notify, &wpd);

		mainloop_running = true;
		while(mainloop_running) {
			while(g_main_context_iteration(g_main_loop_get_context(wpd.loop), false));
			if(wpd.ready) {
				if(volume_change != 0) {
					set_volume(&wpd, wpd.defaultSink.volume + volume_change);
					volume_change = 0;
				}
				if(mute_toggle) {
					set_mute(&wpd, !wpd.defaultSink.muted);
					mute_toggle = false;
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(AUDIO_MAINLOOP_REFRESHTIME));
		}
		info("wireplumber mainloop stopped");
	}

	std::thread* wireplumber_mainloop = nullptr;
	void setupPipeWire() {
		wireplumber_mainloop = new std::thread(wireplumber_run_mainloop);
	}

}