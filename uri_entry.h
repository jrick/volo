// Copyright (c) 2014 Josh Rickmar.
// Use of this source code is governed by an ISC
// license that can be found in the LICENSE file.

#ifndef _VOLO_URI_ENTRY_H
#define _VOLO_URI_ENTRY_H

#include <string>

#include <gtk/gtk.h>

#include <gtk.h>

G_BEGIN_DECLS

#define VOLO_TYPE_URI_ENTRY			(volo_uri_entry_get_type())
#define VOLO_URI_ENRY(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), VOLO_TYPE_URI_ENTRY, VoloURIEntry))
#define VOLO_IS_URI_BAR(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), VOLO_TYPE_URI_ENTRY))
#define VOLO_URI_ENTRY_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), VOLO_TYPE_URI_ENTRY, VoloURIEntryClass))
#define VOLO_IS_URI_ENTRY_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE((klass), VOLO_TYPE_URI_ENTRY))
#define VOLO_URI_ENTRY_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS((obj), VOLO_TYPE_URI_ENTRY, VoloURIEntryClass))

class VoloURIEntry : public gtk::entry {
private:
	bool editing;
	bool refresh_pressed;

	static gtk::entry::class_type* parent_vtable();

public:
	friend void init(VoloURIEntry&);

	// Setter for the text in the URI entry.  This only modifies the
	// text if the entry is not receiving input events (that is, when
	// it is not the grab widget).
	void set_uri(const std::string& uri);
	void set_uri(const char *uri);

	// Overridden signal vfuncs.  A static overload is used to allow
	// assignment to a parent class, and simply calls the non-static
	// member function.
	static gboolean button_release(GtkWidget *, GdkEventButton *);
	bool button_release(GdkEventButton&);
	static gboolean focus_out_event(GtkWidget *, GdkEventFocus *);
	bool focus_out_event(GdkEventFocus&);
	static void icon_press(GtkEntry *, GtkEntryIconPosition, GdkEvent *, gpointer);
	void icon_press(GtkEntryIconPosition, GdkEvent&, gpointer);
};

struct VoloURIEntryClass {
	GtkEntryClass parent_class;

	void (*refresh_clicked)(VoloURIEntry *);
};

GtkWidget * volo_uri_entry_new(void);

GType uri_entry_get_type(void);

G_END_DECLS

namespace volo {

namespace methods {

template <class T, class Derived>
struct uri_entry : gtk::methods::entry<T, Derived> {
	using c_type = VoloURIEntry;

	// Signals.

	template <class U>
	using refresh_clicked_slot = void (*)(Derived *, U *);
	template <class U>
	gtk::connection connect_refresh_clicked(U& obj, refresh_clicked_slot<U> slot) {
		return this->connect("refresh-clicked", G_CALLBACK(slot), &obj);
	}
};

} // namespace methods

struct uri_entry : methods::uri_entry<VoloURIEntry, uri_entry> {
	static auto create() {
		return reinterpret_cast<uri_entry *>(volo_uri_entry_new());
	}
};

} // namespace volo

#endif // _VOLO_URI_ENTRY_H
