#include "output.hpp"
#include "includes.hpp"
#include "wayland.hpp"
#include "layout.hpp"

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

	struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(server.scene, output->wlr_output);
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

	wlr_output_init_render(wlr_output, server.allocator, server.renderer);

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
	wl_list_insert(&server.outputs, &output->link);

    //Listeners
	output->frame.notify = output_frame_notify;
	output->request_state.notify = output_request_state_notify;
	output->destroy.notify = output_destroy_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);


    //basic output configuration. das chani den no advanced mache.
	struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(server.output_layout, wlr_output);
	struct wlr_scene_output *scene_output = wlr_scene_output_create(server.scene, wlr_output);
	wlr_scene_output_layout_add_output(server.scene_layout, l_output, scene_output);

	output->extends.width = wlr_output->width;
	output->extends.height = wlr_output->height;
	output->extends.x = scene_output->x;
	output->extends.y = scene_output->y;

	//layout_init(output->extends);
	Layout::setScreenExtends(output->extends);
}


namespace Output {
    void setup() {
        server.output_layout = wlr_output_layout_create(server.display);
        wl_list_init(&server.outputs);
        server.new_output_listener.notify = backend_new_output_notify;
        wl_signal_add(&server.backend->events.new_output, &server.new_output_listener);
    }
}