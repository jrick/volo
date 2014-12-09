// Copyright (c) 2014 Josh Rickmar.
// Use of this source code is governed by an ISC
// license that can be found in the LICENSE file.

#ifndef _GTK_H
#define _GTK_H

#include <string>
#include <utility>

#include <gtk/gtk.h>

namespace gtk {

template <class T>
struct destroy_delete {
	void operator()(T *ptr) const {
		ptr->destroy();
	}
};

template <class T, class Deleter = destroy_delete<T>>
using unique_ptr = std::unique_ptr<T, Deleter>;

template <class T, class ...Args>
T * make_sunk(Args ...args) {
	auto ptr = T::create(std::forward<Args>(args)...);
	ptr->ref_sink();
	return ptr;
}

struct connection {
	GObject *object;
	unsigned long handler_id;

	void disconnect() {
		g_signal_handler_disconnect(object, handler_id);
	}
};

struct style_context;
struct widget;

namespace methods {

template <class T, class Derived>
struct gobject : T {
	using c_type = GObject;

	// This struct and derived structs are only used for the method
	// interface (by reinterpret_cast'ing T pointers to gobject<T>
	// pointers).  They cannot be constructed, copied, or moved.  See
	// the make_sunk free function and the gtk::unique_ptr class to
	// construct an object that is destroyed after going out of scope,
	// or use the static create member function to return a raw pointer
	// with a floating reference.
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

template <class T, class Derived>
struct style_context : gobject<T, Derived> {
	using c_type = GtkStyleContext;

	void add_class(const std::string& class_name) {
		add_class(ptr(), class_name.c_str());
	}
	void add_class(const char *class_name) {
		gtk_style_context_add_class(ptr(), class_name);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T, class Derived>
struct widget : gobject<T, Derived> {
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
		return reinterpret_cast<gtk::style_context *>(
			gtk_widget_get_style_context(ptr())
		);
	}

	// Signals.

	template <class U>
	using key_press_event_slot = bool (*)(Derived *, GdkEventKey *, U *);
	template <class U>
	connection connect_key_press_event(U& obj, key_press_event_slot<U> slot) {
		return this->connect("key-press-event", G_CALLBACK(slot), &obj);
	}

	template <class U>
	using destroy_slot = void (*)(Derived *, U *);
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

template <class T, class Derived>
struct container : widget<T, Derived> {
	using c_type = GtkContainer;

	template <class U, class UDerived>
	void add(widget<U, UDerived>& w) {
		gtk_container_add(ptr(), w.ptr());
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T, class Derived>
struct bin : container<T, Derived> {};

template <class T, class Derived>
struct window : bin<T, Derived> {
	using c_type = GtkWindow;

	void set_title(const std::string& title) {
		set_title(title.c_str());
	}
	void set_title(const char *title) {
		gtk_window_set_title(ptr(), title);
	}

	void set_default_size(int width, int height) {
		gtk_window_set_default_size(ptr(), width, height);
	}

	template <class U, class UDerived>
	void set_titlebar(widget<U, UDerived>& titlebar) {
		gtk_window_set_titlebar(ptr(), titlebar.ptr());
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T, class Derived>
struct editable : gobject<T, Derived> {
	using c_type = GtkEditable;

	bool get_selection_bounds(int& start, int& end) {
		return gtk_editable_get_selection_bounds(ptr(), &start, &end);
	}

	void select_region(int start_pos, int end_pos) {
		gtk_editable_select_region(ptr(), start_pos, end_pos);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T, class Derived>
struct entry : editable<widget<T, Derived>, Derived> {
	using c_type = GtkEntry;

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
	using activate_slot = void (*)(Derived *, U *);
	template <class U>
	connection connect_activate(U& obj, activate_slot<U> slot) {
		return this->connect("activate", G_CALLBACK(slot), &obj);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T, class Derived>
struct misc : widget<T, Derived> {};

template <class T, class Derived>
struct label : misc<T, Derived> {
	using c_type = GtkLabel;

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

template <class T, class Derived>
struct button : bin<T, Derived> {
	using c_type = GtkButton;

	void set_relief(GtkReliefStyle relief) {
		gtk_button_set_relief(ptr(), relief);
	}

	// Signals.

	template <class U>
	using connect_clicked_slot = void (*)(Derived *, U *);
	template <class U>
	connection connect_clicked(U& obj, connect_clicked_slot<U> slot) {
		return this->connect("clicked", G_CALLBACK(slot), &obj);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T, class Derived>
struct box : container<T, Derived> {
	using c_type = GtkBox;

	template <class U, class UDerived>
	void pack_start(widget<U, UDerived>& child, bool expand = true, bool fill = true,
		unsigned int padding = 0)
	{
		gtk_box_pack_start(ptr(), child.ptr(), expand, fill, padding);
	}

	template <class U, class UDerived>
	void pack_end(widget<U, UDerived>& child, bool expand = true, bool fill = true,
		unsigned int padding = 0)
	{
		gtk_box_pack_end(ptr(), child.ptr(), expand, fill, padding);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T, class Derived>
struct notebook : container<T, Derived> {
	using c_type = GtkNotebook;

	template <class U0, class U1, class U2, class U3>
	int append_page(widget<U0, U1>& child, widget<U2, U3>& tab_label) {
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

	template <class U, class UDerived>
	void set_tab_reorderable(widget<U, UDerived>& child, bool reorderable = true) {
		gtk_notebook_set_tab_reorderable(ptr(), child.ptr(), reorderable);
	}

	// Signals.

	template <class U>
	using page_added_slot = void (*)(Derived *, gtk::widget *, unsigned int, U *);
	template <class U>
	connection connect_page_added(U& obj, page_added_slot<U> slot) {
		return this->connect("page-added", G_CALLBACK(slot), &obj);
	}

	template <class U>
	using page_removed_slot = void (*)(Derived *, gtk::widget *, unsigned int, U *);
	template <class U>
	connection connect_page_removed(U& obj, page_removed_slot<U> slot) {
		return this->connect("page-removed", G_CALLBACK(slot), &obj);
	}

	template <class U>
	using switch_page_slot = void (*)(Derived *, gtk::widget *, unsigned int, U *);
	template <class U>
	connection connect_switch_page(U& obj, switch_page_slot<U> slot) {
		return this->connect("switch-page", G_CALLBACK(slot), &obj);
	}

	template <class U>
	using page_reordered_slot = void (*)(Derived *, gtk::widget *, unsigned int, U *);
	template <class U>
	connection connect_page_reordered(U& obj, page_reordered_slot<U> slot) {
		return this->connect("page-reordered", G_CALLBACK(slot), &obj);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T, class Derived>
struct header_bar : container<T, Derived> {
	using c_type = GtkHeaderBar;

	template <class U, class UDerived>
	void set_custom_title(widget<U, UDerived>& title_widget) {
		gtk_header_bar_set_custom_title(ptr(), title_widget.ptr());
	}

	template <class U, class UDerived>
	void pack_start(widget<U, UDerived>& child) {
		gtk_header_bar_pack_start(ptr(), child.ptr());
	}

	template <class U, class UDerived>
	void pack_end(widget<U, UDerived>& child) {
		gtk_header_bar_pack_end(ptr(), child.ptr());
	}

	void set_show_close_button(bool setting) {
		gtk_header_bar_set_show_close_button(ptr(), setting);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T, class Derived>
struct popover : bin<T, Derived> {};

} // namespace methods

struct gobject : methods::gobject<GObject, gobject> {};

struct style_context : methods::style_context<GtkStyleContext, style_context> {};

struct widget : methods::widget<GtkWidget, widget> {};

struct container : methods::container<GtkContainer, container> {};

struct bin : methods::bin<GtkBin, bin> {};

struct window : methods::window<GtkWindow, window> {
	static auto create(GtkWindowType type = GTK_WINDOW_TOPLEVEL) {
		return reinterpret_cast<window *>(gtk_window_new(type));
	}
};

struct entry : methods::entry<GtkEntry, entry> {
	using class_type = GtkEntryClass;

	static auto create() {
		return reinterpret_cast<entry *>(gtk_entry_new());
	}
};

struct misc : methods::misc<GtkMisc, misc> {};

struct label : methods::label<GtkLabel, label> {
	static auto create(const char *str = "") {
		return reinterpret_cast<label *>(gtk_label_new(str));
	}
	static auto create(const std::string& str) {
		return create(str.c_str());
	}
};

struct button : methods::button<GtkButton, button> {
	static auto create() {
		return reinterpret_cast<button *>(gtk_button_new());
	}

	static auto create(const char *label) {
		return reinterpret_cast<button *>(gtk_button_new_with_label(label));
	}
	static auto create(const std::string& label) {
		return create(label.c_str());
	}

	static auto create(const char *icon_name, GtkIconSize size) {
		return reinterpret_cast<button *>(
			gtk_button_new_from_icon_name(icon_name, size)
		);
	}
	static auto create(const std::string& icon_name, GtkIconSize size) {
		return create(icon_name.c_str(), size);
	}
};

struct box : methods::box<GtkBox, box> {
	static auto create(GtkOrientation orientation = GTK_ORIENTATION_HORIZONTAL, int spacing = 0) {
		return reinterpret_cast<box *>(gtk_box_new(orientation, spacing));
	}
};

struct notebook : methods::notebook<GtkNotebook, notebook> {
	static auto create() {
		return reinterpret_cast<notebook *>(gtk_notebook_new());
	}
};

struct header_bar : methods::header_bar<GtkHeaderBar, header_bar> {
	static auto create() {
		return reinterpret_cast<header_bar *>(gtk_header_bar_new());
	}
};

struct popover : methods::popover<GtkPopover, popover> {
	template <class Widget>
	static auto create(Widget *relative_to) {
		return reinterpret_cast<popover *>(
			gtk_popover_new(relative_to ? relative_to->widget::ptr() : nullptr)
		);
	}
};

} // namespace gtk

#endif // _GTK_H
