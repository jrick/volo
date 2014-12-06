// Copyright (c) 2014 Josh Rickmar.
// Use of this source code is governed by an ISC
// license that can be found in the LICENSE file.

#include <array>

#include <uri_entry.h>

enum {
	REFRESH_CLICKED,
	LAST_SIGNAL,
};

static std::array<guint, LAST_SIGNAL> uri_entry_signals;

G_DEFINE_TYPE(VoloURIEntry, volo_uri_entry, GTK_TYPE_ENTRY);

static void volo_uri_entry_class_init(VoloURIEntryClass *klass) {
	auto *widget_class = reinterpret_cast<GtkWidgetClass *>(klass);

	widget_class->button_release_event = &VoloURIEntry::button_release;
	widget_class->focus_out_event = &VoloURIEntry::focus_out_event;

	klass->refresh_clicked = nullptr;

	uri_entry_signals[REFRESH_CLICKED] = g_signal_new("refresh-clicked",
		G_OBJECT_CLASS_TYPE(klass),
		static_cast<GSignalFlags>(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
		G_STRUCT_OFFSET(VoloURIEntryClass, refresh_clicked),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

void init(VoloURIEntry& uri_entry) {
	uri_entry.set_size_request(600, -1); // TODO: make this dynamic with the window size
	uri_entry.set_margin_start(6);
	uri_entry.set_margin_end(6);
	uri_entry.set_hexpand(true);

	uri_entry.set_input_purpose(GTK_INPUT_PURPOSE_URL);
	uri_entry.set_icon_from_icon_name(GTK_ENTRY_ICON_SECONDARY, "view-refresh");

	uri_entry.editing = false;
	uri_entry.refresh_pressed = false;

	using icon_press_cb = void (*)(GtkEntry *, GtkEntryIconPosition, GdkEvent *, gpointer);
	g_signal_connect(&uri_entry, "icon-press",
		G_CALLBACK(icon_press_cb(&VoloURIEntry::icon_press)), nullptr);
}

static void volo_uri_entry_init(VoloURIEntry *uri_entry) {
	init(*uri_entry);
}

GtkWidget * volo_uri_entry_new() {
	return reinterpret_cast<GtkWidget *>(g_object_new(VOLO_TYPE_URI_ENTRY, NULL));
}

void VoloURIEntry::set_uri(const std::string& uri) {
	return set_uri(uri.c_str());
}

void VoloURIEntry::set_uri(const char *uri) {
	if (!has_focus()) {
		set_text(uri);
	}
}

gtk::entry::class_type * VoloURIEntry::parent_vtable() {
	return reinterpret_cast<gtk::entry::class_type *>(
		volo_uri_entry_parent_class
	);
}


// Signal vfunc overrides.  The static overload must be added to the class
// init function above to apply the override.

gboolean VoloURIEntry::button_release(GtkWidget *w, GdkEventButton *ev) {
	return reinterpret_cast<VoloURIEntry *>(w)->button_release(*ev);
}

bool VoloURIEntry::button_release(GdkEventButton& ev) {
	// If the entry's refresh button was pressed, fire the refresh
	// signal.  In this case we do not set editing mode to true and
	// do not grab focus to highlight all text in the entry.
	if (refresh_pressed) {
		g_signal_emit(this, uri_entry_signals[REFRESH_CLICKED], 0);
		refresh_pressed = false;
		return false;
	}

	// If the refresh button was not pressed, and we haven't previously
	// grabbed focus, do so now.  The editing flag will be unset after
	// focus leaves the entry, but until then, the text inside entry can
	// be modified and clicked without forcably selecting all of the text
	// again.
	int start, end;
	if (!editing && !get_selection_bounds(start, end)) {
		grab_focus();
	}
	editing = true;
	auto entry_vtable = parent_vtable();
	auto widget_vtable = &entry_vtable->parent_class;
	return gtk::entry::button_release_event(widget_vtable, ev);
}

gboolean VoloURIEntry::focus_out_event(GtkWidget *w, GdkEventFocus *f) {
	return reinterpret_cast<VoloURIEntry *>(w)->focus_out_event(*f);
}

bool VoloURIEntry::focus_out_event(GdkEventFocus& f) {
	select_region(0, 0);
	editing = false;
	auto entry_vtable = parent_vtable();
	auto widget_vtable = &entry_vtable->parent_class;
	return gtk::entry::focus_out_event(widget_vtable, f);
}

void VoloURIEntry::icon_press(GtkEntry *e, GtkEntryIconPosition icon_pos,
	GdkEvent *ev, gpointer user_data) {

	reinterpret_cast<VoloURIEntry *>(e)->icon_press(icon_pos, *ev, user_data);
}

void VoloURIEntry::icon_press(GtkEntryIconPosition, GdkEvent&, gpointer) {
	refresh_pressed = true;
}
