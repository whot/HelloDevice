/*
 * Copyright © 2017 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <sys/signalfd.h>

#include <X11/extensions/XI2.h>
#include <X11/extensions/XInput2.h>

#define ARRAY_LENGTH(a_) (sizeof(a_)/sizeof(a_[0]))
#define MAX_DEVICES 40

struct hd_context {
	Display *dpy;
	char *command;

	/* When a device is removed, the event doesn't contain the
	 * device name and we can't query it anymore, so we need to
	 * save it. index is deviceid */
	char *device_names[MAX_DEVICES];
};

enum notification_type {
	ADDED = 0,
	REMOVED,
	PRESENT,
};

static const char *notification_type_str[3] = {
	"added", "removed", "present",
};

#define error(...) \
	fprintf(stderr, "ERROR: " __VA_ARGS__ )

static char *
read_config_file(void)
{
	char *config_path;
	GKeyFile *keyfile;
	gboolean rc;
	char *command;

	config_path = g_strdup_printf("%s/HelloDevice/HelloDevice.conf",
				      g_get_user_config_dir());

	keyfile = g_key_file_new();
	rc = g_key_file_load_from_file(keyfile,
				       config_path,
				       G_KEY_FILE_NONE,
				       NULL);
	if (!rc) {
		error("Failed to load config file from: %s\n",
		      config_path);
		return NULL;
	}

	g_free(config_path);

	command = g_key_file_get_string(keyfile, "General", "command", NULL);
	if (!command)
		error("Failed to load command string.\n");

	g_key_file_unref(keyfile);

	return command;
}

static void
run_for_device(const char *command,
	       enum notification_type type,
	       const char *name,
	       int id)
{
	GPid pid;
	char *argv[5];
	char **env;
	const char *path;
	char new_path[PATH_MAX];
	int rc;

	g_assert(type <= PRESENT);
	g_assert(type < ARRAY_LENGTH(notification_type_str));

	printf("%s (%d) %s\n", name, id, notification_type_str[type]);

	/* Prefix XDG_CONFIG_HOME to PATH */
	env = g_get_environ();
	path = g_environ_getenv(env, "PATH");
	g_snprintf(new_path, sizeof(new_path), "%s/HelloDevice/:%s",
		   g_get_user_config_dir(), path);
	env = g_environ_setenv(env, "PATH", new_path, true);

	/* prep argv */
	argv[0] = g_strdup(command);
	argv[1] = g_strdup_printf("-t %s", notification_type_str[type]);
	argv[2] = g_strdup_printf("-i %d", id);
	argv[3] = g_strdup_printf("%s", name);
	argv[4] = NULL;

	/* bork bork bork fork fork */
	rc = g_spawn_async(g_get_home_dir(), argv, env,
			   G_SPAWN_SEARCH_PATH_FROM_ENVP,
			   NULL /* child setup */,
			   NULL /* user data */,
			   &pid,
			   NULL);
	if (!rc)
		error("Failed to spawn command %s\n", command);
	else
		g_spawn_close_pid(pid);

	g_strfreev(env);
	for (size_t i = 0; i < ARRAY_LENGTH(argv); i++)
		g_free(argv[i]);
}

static void
xi_process_current_devices(struct hd_context *ctx)
{
	XIDeviceInfo *info;
	int ndevices;

	info = XIQueryDevice(ctx->dpy, XIAllDevices, &ndevices);
	while(ndevices-- > 0) {
		XIDeviceInfo *d = &info[ndevices];

		if (d->use == XIMasterPointer ||
		    d->use == XIMasterKeyboard)
			continue;

		if (!d->enabled)
			continue;

		ctx->device_names[d->deviceid] = g_strdup(d->name);
		run_for_device(ctx->command, PRESENT, d->name, d->deviceid);
	}

	XIFreeDeviceInfo(info);
}

static Display*
xi_setup(void)
{
	Display *dpy;
	Status status;
	int major = 2, minor = 0;
	unsigned char evmask[XIMaskLen(XI_HierarchyChanged)] = {0};
	XIEventMask mask = {
		.deviceid = XIAllDevices,
		.mask_len = sizeof(evmask),
		.mask = evmask,
	};

	XISetMask(evmask, XI_HierarchyChanged);

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		error("Failed to open X display.\n");
		return NULL;
	}

	status = XIQueryVersion(dpy, &major, &minor);
	if (status != Success) {
		error("Failed to set up XI2 version.\n");
		return NULL;
	}


	status = XISelectEvents(dpy, DefaultRootWindow(dpy),
				&mask, 1);
	if (status != Success) {
		error("Failed to register for XI2 events.\n");
		return NULL;
	}

	XFlush(dpy);

	return dpy;
}

static inline char *
xi_get_device_name(Display *dpy, int deviceid)
{
	XIDeviceInfo *info;
	int ndevices;
	char *name;

	info = XIQueryDevice(dpy, deviceid, &ndevices);
	name = g_strdup(info->name);
	XIFreeDeviceInfo(info);

	return name;
}

static void
xi_handle_event(struct hd_context *ctx, XGenericEventCookie *cookie)
{
	XIHierarchyEvent *event = cookie->data;
	XIHierarchyInfo *info;

	if ((event->flags & (XIDeviceEnabled|XIDeviceDisabled)) == 0)
		return;

	for (int i = 0; i < event->num_info; i++) {
		char *name;
		int deviceid;

		info = &event->info[i];
		deviceid = info->deviceid;

		if (info->flags & (XIDeviceEnabled|XIDeviceDisabled)) {
			if (info->use == XIMasterKeyboard ||
			    info->use == XIMasterPointer)
				continue;

			name = ctx->device_names[deviceid];
			if (!name) {
				name = xi_get_device_name(ctx->dpy, deviceid);
				ctx->device_names[info->deviceid] = name;
			}

			/* We sleep a second before we do anything, this
			 * gives the DE/WM chance to configure the device.
			 * We then override that config merely by being
			 * slower than the DE/WM. Slow and delayed wins the
			 * race, right?
			 */
			sleep(1);

			run_for_device(ctx->command,
				       (info->flags & XIDeviceEnabled) ?  ADDED : REMOVED,
				       name,
				       deviceid);
		}

		if (info->flags & XISlaveRemoved) {
			free(ctx->device_names[deviceid]);
			ctx->device_names[deviceid] = NULL;
		}
	}
}

static void
loop(struct hd_context *ctx)
{
	static int xi_opcode, xi_error, xi_event;
	struct pollfd fds[2];
	sigset_t mask;

	fds[0].fd = ConnectionNumber(ctx->dpy);
	fds[0].events = POLLIN;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);

	fds[1].fd = signalfd(-1, &mask, SFD_NONBLOCK);
	fds[1].events = POLLIN;
	sigprocmask(SIG_BLOCK, &mask, NULL);

	if (xi_opcode == 0)
		XQueryExtension(ctx->dpy, "XInputExtension", &xi_opcode,
				&xi_error, &xi_event);

	while (poll(fds, 2, -1)) {
		static int foo = 0;
		if (fds[1].revents)
			break;

		if (foo++ > 5)
			break;
		while (XPending(ctx->dpy)) {
			XEvent ev;
			XGenericEventCookie *cookie = &ev.xcookie;

			XNextEvent(ctx->dpy, &ev);

			if (ev.xcookie.type != GenericEvent ||
			    ev.xcookie.extension != xi_opcode ||
			    !XGetEventData(ctx->dpy, &ev.xcookie))
				continue;

			if (cookie->evtype == XI_HierarchyChanged)
				xi_handle_event(ctx, cookie);

			XFreeEventData(ctx->dpy, cookie);
		}
	}
}

int
main(int argc, char **argv)
{
	struct hd_context ctx = {0};
	int rc = EXIT_FAILURE;

	ctx.command = read_config_file();
	if (!ctx.command)
		goto out;

	ctx.dpy = xi_setup();
	if (ctx.dpy == NULL)
		goto out;

	xi_process_current_devices(&ctx);

	loop(&ctx);

	rc = EXIT_SUCCESS;
out:
	free(ctx.command);
	if (ctx.dpy)
		XCloseDisplay(ctx.dpy);

	for (int i = 0; i < ARRAY_LENGTH(ctx.device_names); i++)
		free(ctx.device_names[i]);

	return rc;
}
