// Copyright (c) 2014 Josh Rickmar.
// Use of this source code is governed by an ISC
// license that can be found in the LICENSE file.

#ifndef _GTK_H
#define _GTK_H

#include <string>
#include <utility>

#include <gtk/gtk.h>

namespace gtk {

namespace handles {

template<class T, template <typename> class Ownership>
class handle {
protected:
	typename T::impl_type *opaque;

public:
	handle(typename T::impl_type *impl) : opaque{impl} {
		static_cast<Ownership<T> *>(this)->own();
	}
	~handle() {
		if (opaque != nullptr) {
			static_cast<Ownership<T> *>(this)->unown();
		}
	}

	// Non-copyable, movable.
	handle(const handle&) = delete;
	handle& operator=(const handle&) = delete;
	handle(handle&& other) {
		opaque = other.opaque;
		other.opaque = nullptr;
	}
	handle& operator=(handle&& other) {
		if (opaque != nullptr) {
			static_cast<Ownership<T> *>(this)->unown();
		}
		opaque = other.opaque;
		other.opaque = nullptr;
		return *this;
	}

	auto& operator*() { return *opaque; }
	const auto& operator*() const { return *opaque; }
	auto get() { return opaque; }
	const auto get() const { return opaque; }
	auto operator->() { return get(); }
	const auto operator->() const { return get(); }
};

template <class T>
struct ref_counted : handle<T, ref_counted> {
	void own() {
		this->opaque->ref_sink();
	}
	void unown() {
		this->opaque->unref();
	}
};

template <class T>
struct unmanaged : handle<T, unmanaged> {
	void own() {}
	void unown() {}
};

template <class T>
struct unique : handle<T, unique> {
	void own() {
		this->opaque->ref_sink();
	}
	void unown() {
		this->opaque->destroy();
	}
};

} // namespace handles


struct connection {
	GObject *object;
	unsigned long handler_id;

	void disconnect() {
		g_signal_handler_disconnect(object, handler_id);
	}
};

template <class T = GObject>
struct gobject : T {
	using c_type = GObject;

	// This struct and derived structs are only used for the method
	// interface (by reinterpret_cast'ing T pointers to gobject<T>
	// pointers).  They cannot be constructed, copied, or moved.  See the
	// handles namespace for RAII handles for pointers to wrapped objects
	// and widgets.
	gobject() = delete;
	~gobject() = delete;
	gobject(const gobject&) = delete;
	gobject& operator=(const gobject&) = delete;
	gobject(gobject&&) = delete;
	gobject& operator=(gobject&&) = delete;

	void ref() {
		g_object_ref(ptr());
	}
	void ref_sink() {
		g_object_ref_sink(ptr());
	}
	void unref() {
		g_object_unref(ptr());
	}

	connection connect(const char *detailed_signal, GCallback handler, gpointer data) {
		auto id = g_signal_connect(ptr(), detailed_signal, handler, data);
		return {ptr(), id};
	}
	void disconnect(unsigned long handler_id) {
		g_signal_handler_disconnect(ptr(), handler_id);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T = GtkStyleContext>
struct style_context : gobject<T> {
	using c_type = GtkStyleContext;

	void add_class(const std::string& class_name) {
		add_class(ptr(), class_name.c_str());
	}
	void add_class(const char *class_name) {
		gtk_style_context_add_class(ptr(), class_name);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T = GtkWidget>
struct widget : gobject<T> {
	using c_type = GtkWidget;
	using c_class_type = GtkWidgetClass;

	void show() {
		gtk_widget_show(ptr());
	}

	void show_all() {
		gtk_widget_show_all(ptr());
	}

	void set_can_focus(bool can_focus) {
		gtk_widget_set_can_focus(ptr(), can_focus);
	}

	void set_size_request(int width, int height) {
		gtk_widget_set_size_request(ptr(), width, height);
	}

	void set_margin_start(int margin) {
		gtk_widget_set_margin_start(ptr(), margin);
	}

	void set_margin_end(int margin) {
		gtk_widget_set_margin_end(ptr(), margin);
	}

	void set_hexpand(bool expand) {
		gtk_widget_set_hexpand(ptr(), expand);
	}

	void set_sensitive(bool sensitive) {
		gtk_widget_set_sensitive(ptr(), sensitive);
	}

	void grab_focus() {
		gtk_widget_grab_focus(ptr());
	}

	bool has_focus() {
		return gtk_widget_has_focus(ptr());
	}

	void destroy() {
		gtk_widget_destroy(ptr());
	}

	auto get_style_context() {
		return reinterpret_cast<style_context<> *>(
			gtk_widget_get_style_context(ptr())
		);
	}

	// Signals.

	template <class U>
	using key_press_event_slot = bool (*)(widget *, GdkEventKey *, U *);
	template <class U>
	connection connect_key_press_event(U& obj, key_press_event_slot<U> slot) {
		return this->connect("key-press-event", G_CALLBACK(slot), &obj);
	}

	template <class U>
	using destroy_slot = void (*)(widget *, U *);
	template <class U>
	connection connect_destroy(U& obj, destroy_slot<U> slot) {
		return this->connect("destroy", G_CALLBACK(slot), &obj);
	}

	// Class vfuncs.

	int key_press_event(GdkEventKey& event) {
		return vtable()->key_press_event(ptr(), &event);
	}

	bool focus_out_event(GdkEventFocus& event) {
		return focus_out_event(vtable(), event);
	}
	bool focus_out_event(c_class_type *vtable, GdkEventFocus& event) {
		return vtable->focus_out_event(ptr(), &event);
	}

	bool button_release_event(GdkEventButton& event) {
		return button_release_event(vtable(), event);
	}
	bool button_release_event(c_class_type *vtable, GdkEventButton& event) {
		return vtable->button_release_event(ptr(), &event);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
	c_class_type * vtable() { return GTK_WIDGET_GET_CLASS(ptr()); }
};

template <class T = GtkContainer>
struct container : widget<T> {
	using c_type = GtkContainer;

	template <class U>
	void add(widget<U>& w) {
		gtk_container_add(ptr(), w.ptr());
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T = GtkBin>
struct bin : container<T> {};

template <class T = GtkWindow>
struct window : bin<T> {
	using c_type = GtkWindow;
	using impl_type = window<c_type>;

	template <template <typename> class Ownership = handles::unique>
	struct handle : handles::handle<impl_type, Ownership> {
		handle(GtkWindowType type = GTK_WINDOW_TOPLEVEL) :
		handles::handle<impl_type, Ownership>{reinterpret_cast<impl_type *>(
			gtk_window_new(type)
		)} {}
	};

	void set_title(const std::string& title) {
		set_title(title.c_str());
	}
	void set_title(const char *title) {
		gtk_window_set_title(ptr(), title);
	}

	void set_default_size(int width, int height) {
		gtk_window_set_default_size(ptr(), width, height);
	}

	template <class U>
	void set_titlebar(widget<U>& titlebar) {
		gtk_window_set_titlebar(ptr(), titlebar.ptr());
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T = GtkEditable>
struct editable : gobject<T> {
	using c_type = GtkEditable;
	using impl_type = editable<c_type>;

	bool get_selection_bounds(int& start, int& end) {
		return gtk_editable_get_selection_bounds(ptr(), &start, &end);
	}

	void select_region(int start_pos, int end_pos) {
		gtk_editable_select_region(ptr(), start_pos, end_pos);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T = GtkEntry>
struct entry : editable<widget<T>> {
	using c_type = GtkEntry;
	using impl_type = entry<c_type>;

	template <template <typename> class Ownership = handles::unique>
	struct handle : handles::handle<impl_type, Ownership> {
		handle() :
		handles::handle<impl_type, Ownership>{reinterpret_cast<impl_type *>(
			gtk_entry_new()
		)} {}
	};

	const char * get_text() {
		return gtk_entry_get_text(ptr());
	}

	void set_input_purpose(GtkInputPurpose purpose) {
		gtk_entry_set_input_purpose(ptr(), purpose);
	}

	void set_icon_from_icon_name(GtkEntryIconPosition pos, const std::string& name) {
		set_icon_from_icon_name(pos, name.c_str());
	}
	void set_icon_from_icon_name(GtkEntryIconPosition pos, const char *name) {
		gtk_entry_set_icon_from_icon_name(ptr(), pos, name);
	}

	void set_text(const std::string& text) {
		set_text(text.c_str());
	}
	void set_text(const char *text) {
		gtk_entry_set_text(ptr(), text);
	}

	// Signals.

	template <class U>
	using activate_slot = void (*)(entry *, U *);
	template <class U>
	connection connect_activate(U& obj, activate_slot<U> slot) {
		return this->connect("activate", G_CALLBACK(slot), &obj);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T = GtkMisc>
struct misc : widget<T> {};

template <class T = GtkLabel>
struct label : misc<T> {
	using c_type = GtkLabel;
	using impl_type = label<c_type>;

	template <template <typename> class Ownership = handles::unique>
	struct handle : handles::handle<impl_type, Ownership> {
		handle(const std::string& str = "") :
		handles::handle<impl_type, Ownership>{reinterpret_cast<impl_type *>(
			gtk_label_new(str.c_str())
		)} {}
		handle(const char *str = "") :
		handles::handle<impl_type, Ownership>{reinterpret_cast<impl_type *>(
			gtk_label_new(str)
		)} {}
	};

	void set_text(const std::string& text) {
		set_text(text.c_str());
	}
	void set_text(const char *text) {
		gtk_label_set_text(ptr(), text);
	}

	void set_ellipsize(PangoEllipsizeMode mode) {
		gtk_label_set_ellipsize(ptr(), mode);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T = GtkButton>
struct button : bin<T> {
	using c_type = GtkButton;
	using impl_type = button<c_type>;

	template <template <typename> class Ownership = handles::unique>
	struct handle : handles::handle<impl_type, Ownership> {
		handle() :
		handles::handle<impl_type, Ownership>{reinterpret_cast<impl_type *>(
			gtk_button_new()
		)} {}

		handle(const std::string& label = "") :
		handles::handle<impl_type, Ownership>{reinterpret_cast<impl_type *>(
			gtk_button_new_with_label(label.c_str())
		)} {}
		handle(const char *label = "") :
		handles::handle<impl_type, Ownership>{reinterpret_cast<impl_type *>(
			gtk_button_new_with_label(label)
		)} {}

		handle(const std::string& icon_name, GtkIconSize size) :
		handles::handle<impl_type, Ownership>{reinterpret_cast<impl_type *>(
			gtk_button_new_from_icon_name(icon_name.c_str(), size)
		)} {}
		handle(const char *icon_name, GtkIconSize size) :
		handles::handle<impl_type, Ownership>{reinterpret_cast<impl_type *>(
			gtk_button_new_from_icon_name(icon_name, size)
		)} {}
	};

	void set_relief(GtkReliefStyle relief) {
		gtk_button_set_relief(ptr(), relief);
	}

	// Signals.

	template <class U>
	using connect_clicked_slot = void (*)(button *, U *);
	template <class U>
	connection connect_clicked(U& obj, connect_clicked_slot<U> slot) {
		return this->connect("clicked", G_CALLBACK(slot), &obj);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T = GtkBox>
struct box : container<T> {
	using c_type = GtkBox;
	using impl_type = box<c_type>;

	template <template <typename> class Ownership = handles::unique>
	struct handle : handles::handle<impl_type, Ownership> {
		handle(GtkOrientation orientation = GTK_ORIENTATION_HORIZONTAL, int spacing = 0) :
		handles::handle<impl_type, Ownership>{reinterpret_cast<impl_type *>(
			gtk_box_new(orientation, spacing)
		)} {}
	};

	template <class U>
	void pack_start(widget<U>& child, bool expand = true, bool fill = true,
		unsigned int padding = 0)
	{
		gtk_box_pack_start(ptr(), child.ptr(), expand, fill, padding);
	}

	template <class U>
	void pack_end(widget<U>& child, bool expand = true, bool fill = true,
		unsigned int padding = 0)
	{
		gtk_box_pack_end(ptr(), child.ptr(), expand, fill, padding);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T = GtkNotebook>
struct notebook : container<T> {
	using c_type = GtkNotebook;
	using impl_type = notebook<c_type>;

	template <template <typename> class Ownership = handles::unique>
	struct handle : handles::handle<impl_type, Ownership> {
		handle() :
		handles::handle<impl_type, Ownership>{reinterpret_cast<impl_type *>(
			gtk_notebook_new()
		)} {}
	};

	template <class U0, class U1>
	int append_page(widget<U0>& child, widget<U1>& tab_label) {
		return gtk_notebook_append_page(ptr(), child.ptr(), tab_label.ptr());
	}

	int get_current_page() {
		return gtk_notebook_get_current_page(ptr());
	}

	int get_n_pages() {
		return gtk_notebook_get_n_pages(ptr());
	}

	void set_current_page(int page_num) {
		gtk_notebook_set_current_page(ptr(), page_num);
	}

	void set_show_tabs(bool show_tabs) {
		gtk_notebook_set_show_tabs(ptr(), show_tabs);
	}

	void set_scrollable(bool scrollable) {
		gtk_notebook_set_scrollable(ptr(), scrollable);
	}

	template <class U>
	void set_tab_reorderable(widget<U>& child, bool reorderable = true) {
		gtk_notebook_set_tab_reorderable(ptr(), child.ptr(), reorderable);
	}

	// Signals.

	template <class U>
	using page_added_slot = void (*)(notebook *, widget<> *, unsigned int, U *);
	template <class U>
	connection connect_page_added(U& obj, page_added_slot<U> slot) {
		return this->connect("page-added", G_CALLBACK(slot), &obj);
	}

	template <class U>
	using page_removed_slot = void (*)(notebook *, widget<> *, unsigned int, U *);
	template <class U>
	connection connect_page_removed(U& obj, page_removed_slot<U> slot) {
		return this->connect("page-removed", G_CALLBACK(slot), &obj);
	}

	template <class U>
	using switch_page_slot = void (*)(notebook *, widget<> *, unsigned int, U *);
	template <class U>
	connection connect_switch_page(U& obj, switch_page_slot<U> slot) {
		return this->connect("switch-page", G_CALLBACK(slot), &obj);
	}

	template <class U>
	using page_reordered_slot = void (*)(notebook *, widget<> *, unsigned int, U *);
	template <class U>
	connection connect_page_reordered(U& obj, page_reordered_slot<U> slot) {
		return this->connect("page-reordered", G_CALLBACK(slot), &obj);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T = GtkHeaderBar>
struct header_bar : container<T> {
	using c_type = GtkHeaderBar;
	using impl_type = header_bar<c_type>;

	template <template <typename> class Ownership = handles::unique>
	struct handle : handles::handle<impl_type, Ownership> {
		handle() :
		handles::handle<impl_type, Ownership>{reinterpret_cast<impl_type *>(
			gtk_header_bar_new()
		)} {}
	};

	template <class U>
	void set_custom_title(widget<U>& title_widget) {
		gtk_header_bar_set_custom_title(ptr(), title_widget.ptr());
	}

	template <class U>
	void pack_start(widget<U>& child) {
		gtk_header_bar_pack_start(ptr(), child.ptr());
	}

	template <class U>
	void pack_end(widget<U>& child) {
		gtk_header_bar_pack_end(ptr(), child.ptr());
	}

	void set_show_close_button(bool setting) {
		gtk_header_bar_set_show_close_button(ptr(), setting);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

} // namespace gtk

#endif // _GTK_H
