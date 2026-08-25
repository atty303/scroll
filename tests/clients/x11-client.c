#include <xcb/xcb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

static void sleep_ms(long milliseconds) {
	struct timespec duration = {
		.tv_sec = milliseconds / 1000,
		.tv_nsec = milliseconds % 1000 * 1000000,
	};
	nanosleep(&duration, NULL);
}

static int wait_for_event(xcb_connection_t *conn, uint8_t event_type) {
	xcb_generic_event_t *event;
	while ((event = xcb_wait_for_event(conn))) {
		uint8_t response_type = event->response_type & ~0x80;
		free(event);
		if (response_type == event_type) {
			return 0;
		}
	}
	return 1;
}

int main(int argc, char **argv) {
	xcb_connection_t *conn = xcb_connect(NULL, NULL);
	if (xcb_connection_has_error(conn)) {
		fprintf(stderr, "Failed to connect to X11 display\n");
		return 1;
	}

	xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	xcb_window_t win = xcb_generate_id(conn);

	uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	uint32_t values[2] = {
		screen->white_pixel,
		XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY
	};

	xcb_create_window(conn, XCB_COPY_FROM_PARENT, win, screen->root,
					  0, 0, 150, 150, 10,
					  XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
					  mask, values);

	const char *title = "Test X11 Window";
	if (argc > 1) title = argv[1];
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win,
						XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
						strlen(title), title);

	const char *instance = "test_instance";
	const char *class = "TestClass";
	if (argc > 2) instance = argv[2];
	if (argc > 3) class = argv[3];
	size_t class_len = strlen(instance) + 1 + strlen(class) + 1;
	char *class_str = malloc(class_len);
	if (!class_str) {
		perror("malloc");
		return 1;
	}
	strcpy(class_str, instance);
	strcpy(class_str + strlen(instance) + 1, class);
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win,
						XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8,
						class_len, class_str);
	free(class_str);

	xcb_map_window(conn, win);
	xcb_flush(conn);
	if (argc > 4 && strcmp(argv[4], "rapid-remap") == 0) {
		if (wait_for_event(conn, XCB_MAP_NOTIFY)) {
			xcb_disconnect(conn);
			return 1;
		}
		sleep_ms(50);
		for (int i = 0; i < 3; ++i) {
			xcb_unmap_window(conn, win);
			xcb_flush(conn);
			if (wait_for_event(conn, XCB_UNMAP_NOTIFY)) {
				xcb_disconnect(conn);
				return 1;
			}
			sleep_ms(50);
			xcb_map_window(conn, win);
			xcb_flush(conn);
			if (wait_for_event(conn, XCB_MAP_NOTIFY)) {
				xcb_disconnect(conn);
				return 1;
			}
			sleep_ms(50);
		}
		xcb_destroy_window(conn, win);
		xcb_flush(conn);
		xcb_disconnect(conn);
		return 0;
	}

	while (1) {
		xcb_generic_event_t *ev = xcb_wait_for_event(conn);
		if (!ev) break;
		free(ev);
	}

	xcb_disconnect(conn);
	return 0;
}
