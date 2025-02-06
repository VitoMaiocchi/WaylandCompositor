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
#include <pulse/pulseaudio.h>
#include <cmath>
#include <atomic>

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

	void setupPulseAudio();
	void cleanupPulseAudio();

    void setup() {
        output_layout = wlr_output_layout_create(Server::display);

        new_output_listener.notify = backend_new_output_notify;
        wl_signal_add(&Server::backend->events.new_output, &new_output_listener);

        scene = wlr_scene_create();
        scene_layout = wlr_scene_attach_output_layout(scene, output_layout);

		setupPulseAudio();
    }

    void cleanup() {
        wlr_scene_node_destroy(&scene->tree.node);
        wlr_output_layout_destroy(output_layout);

		cleanupPulseAudio();
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

	pa_threaded_mainloop* mainloop;
	pa_context* context;
	pa_volume_t current_volume = PA_VOLUME_INVALID;

	std::atomic<bool> connected = false;
	const pa_sink_info* default_sink = nullptr;

	void adjustVolume(double vol) {
		assert(vol < 1 && vol > -1);
		if(!connected) return;
		if(!default_sink) return;

		pa_threaded_mainloop_lock(mainloop);

		pa_cvolume new_vol;
		new_vol.channels = default_sink->volume.channels;
		debug("default channels: {}", default_sink->volume.channels);
		uint max = PA_CHANNELS_MAX < new_vol.channels ? PA_CHANNELS_MAX : new_vol.channels;
		debug("max={}",max);
		for(uint i = 0; i < max; i++) {
			pa_volume_t o = default_sink->volume.values[i];
			pa_volume_t n;
			if(vol > 0) {
				if(o + PA_VOLUME_NORM*vol > PA_VOLUME_NORM) n = PA_VOLUME_NORM;
				else n = o + PA_VOLUME_NORM*vol;
			} else {
				if(o < PA_VOLUME_NORM*(-vol)) n = 0;
				else n = o - PA_VOLUME_NORM*(-vol);
			}
			new_vol.values[i] = n;
		}
		pa_context_set_sink_volume_by_index(context, default_sink->index, &new_vol, nullptr, nullptr);
		//TODO: das mit by name und nöd by index mache den muss mer de sink nöd speichere pa_context_set_sink_volume_by_name()
		pa_threaded_mainloop_unlock(mainloop);
		debug("KYS");
	}

	void synchronized_volume_update(void* data) {
		const pa_volume_t volume = *(pa_volume_t*)data;
		delete (pa_volume_t*)data;

		const uint vol_percent = std::round( (float)volume*100/PA_VOLUME_NORM );
		Titlebar::updateVolume(vol_percent);
	}

	//TODO: add support for mute
	void get_sink_volume_callback(pa_context *c, const pa_sink_info *i, int is_last, void *userdata) {
		if(is_last) return;
		assert(i);
		default_sink = i;
		const pa_volume_t volume = pa_cvolume_avg(&i->volume);
		if(volume == current_volume) return;
		current_volume = volume;
		Server::queueEvent(synchronized_volume_update, new pa_volume_t(current_volume));
	}


	void subscribe_callback(pa_context *c, pa_subscription_event_type_t t, uint32_t index, void *userdata) {
		if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK &&
			(t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
			pa_context_get_sink_info_by_name(c, "@DEFAULT_SINK@", get_sink_volume_callback, nullptr);
		}
	}

	GOptionContext* gcontext;
	GMainLoop* loop;
	WpCore* core;
	WpObjectManager* om;
	uint pending_plugins = 0;

	void default_node_update_notify(GObject *emitter, guint arg, gpointer user_data) {
		debug("DEFAULT NODE UPDATE");
	}

	void mixer_update_notify(GObject *emitter, guint arg, gpointer user_data) {
		debug("VOLUME CHANGE (MIXER) UPDATE");
	}

	static void on_plugin_loaded (WpCore * core, GAsyncResult * res, void* data) {
		GError *error = NULL;

		if (!wp_core_load_component_finish (core, res, &error)) {
			fprintf (stderr, "%s\n", error->message);
			error("error loading pipewire/wireplumber plugin");
			g_main_loop_quit (loop);
			return;
		}

		if (--pending_plugins == 0) {
			auto mixer_api = wp_plugin_find (core, "mixer-api");
			g_object_set (mixer_api, "scale", 1 /* cubic */, NULL);
			wp_core_install_object_manager (core, om);
		}
	}

	#define MAXINT32	(0x7fffffff)
	#define MAXUINT32	(0xffffffff)

	int32_t get_default_sink_id(WpPlugin* def_nodes_api) {
		int32_t res = -1;

		g_signal_emit_by_name (def_nodes_api, "get-default-node", "Audio/Sink", &res); //"Audio/Source" für source
		if(res <= 0 || res >= MAXUINT32) error("default node id is not valid");

		return res;
	}


	void do_print_volume (gpointer proxy) {
		WpPlugin* mixer_api = wp_plugin_find (core, "mixer-api");
		GVariant *variant = NULL;
		gboolean mute = FALSE;
		gdouble volume = 1.0;
		int32_t id = wp_proxy_get_bound_id (WP_PROXY (proxy));

	
		g_signal_emit_by_name (mixer_api, "get-volume", id, &variant);
		if (!variant) {
			fprintf (stderr, "Node %d does not support volume\n", id);
			return;
		}
		g_variant_lookup (variant, "volume", "d", &volume);
		g_variant_lookup (variant, "mute", "b", &mute);
		g_clear_pointer (&variant, g_variant_unref);

		printf ("Volume: %.2f%s", volume, mute ? " [MUTED]\n" : "\n");
	}

	void state_changed_callback (WpNode * self, WpNodeState old_state, WpNodeState new_state, gpointer user_data) {
		debug("STATE CHANGED");
	}

	void get_volume_run(void* data) {
		WpPlugin* def_nodes_api = NULL;
		GError* error = NULL;
		gpointer proxy = NULL; // (WpPipewireObject*)
		int32_t id;

		def_nodes_api = wp_plugin_find (core, "default-nodes-api");
		id = get_default_sink_id(def_nodes_api);

		proxy = wp_object_manager_lookup (om, WP_TYPE_GLOBAL_PROXY,
			WP_CONSTRAINT_TYPE_G_PROPERTY, "bound-id", "=u", id, NULL);

		if (!proxy) {
			fprintf (stderr, "Node '%d' not found\n", id);
			return;
		}

		g_signal_connect(
		proxy,         // The Wireplumber object (e.g., WpNode, WpGlobalProxy)
		"state-changed",  // The name of the signal (e.g., "notify", "volume-changed")
		G_CALLBACK(state_changed_callback),  // Your callback
		nullptr     // Optional data passed to the callback
		);

		do_print_volume(proxy);

		WpPlugin* mixer_api = wp_plugin_find (core, "mixer-api");

		g_signal_connect(def_nodes_api, "changed", G_CALLBACK(default_node_update_notify), nullptr);
		g_signal_connect(mixer_api, "changed", G_CALLBACK(mixer_update_notify), nullptr);  

	}

	static gboolean get_volume_prepare() {
		wp_object_manager_add_interest (om, WP_TYPE_NODE, NULL);
		wp_object_manager_request_object_features (om, WP_TYPE_GLOBAL_PROXY,
			WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL);
		return TRUE;
	}

	void setupPipeWire() {
		wp_init (WP_INIT_ALL);
		gcontext = g_option_context_new ("Vito Manager");
		loop = g_main_loop_new (NULL, FALSE);
		core = wp_core_new (NULL, NULL, NULL);
		om = wp_object_manager_new ();

		get_volume_prepare();

		 /* load required API modules */
		pending_plugins++;
		wp_core_load_component (core, "libwireplumber-module-default-nodes-api",
			"module", NULL, NULL, NULL, (GAsyncReadyCallback) on_plugin_loaded, nullptr);
		pending_plugins++;
		wp_core_load_component (core, "libwireplumber-module-mixer-api",
			"module", NULL, NULL, NULL, (GAsyncReadyCallback) on_plugin_loaded, nullptr);

		/* connect */
		if (!wp_core_connect (core)) {
			fprintf (stderr, "Could not connect to PipeWire\n");
			error("Could not connect to PipeWire");
			return;
		}

		/* run */
		g_signal_connect_swapped (core, "disconnected",
			(GCallback) g_main_loop_quit, loop);
		g_signal_connect_swapped (om, "installed",
			(GCallback) get_volume_run, nullptr);

		while(true) g_main_context_iteration(g_main_loop_get_context(loop), true);
		debug("kys");
	}

	void setupPulseAudio() {
		setupPipeWire();

		mainloop = pa_threaded_mainloop_new();
		pa_mainloop_api *mainloop_api = pa_threaded_mainloop_get_api(mainloop);
		context = pa_context_new(mainloop_api, "Vito Manager");
		pa_context_connect(context, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr);

		pa_context_set_state_callback(context, [](pa_context *c, void *userdata) {
			if (pa_context_get_state(c) == PA_CONTEXT_READY) {
				connected = true;
				debug("pulse ready");
				pa_context_get_sink_info_by_name(c, "@DEFAULT_SINK@", get_sink_volume_callback, nullptr);
				pa_context_set_subscribe_callback(c, subscribe_callback, nullptr);
				pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SINK, nullptr, nullptr);
			}
		}, nullptr);

		pa_threaded_mainloop_start(mainloop);
	}

	void cleanupPulseAudio() {
		connected = false;
		pa_context_unref(context);
		pa_threaded_mainloop_stop(mainloop);
		pa_threaded_mainloop_free(mainloop);
	}
}