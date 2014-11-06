// Copyright (c) 2014 Josh Rickmar.
// Use of this source code is governed by an ISC
// license that can be found in the LICENSE file.

#ifndef _VOLO_H
#define _VOLO_H

#include <array>
#include <vector>

#include <gtkmm.h>
#include <webkit2/webkit2.h>


namespace volo {

// make_managed creates a dynamically allocated gtkmm widget whose
// destruction will be delegated to the container it is added to.
//
// This is a replacement for the gtkmm idiom:
//
//   Gtk::manage(new Gtk::Widget)
//
// to create a managed Gtk::Widget pointer and avoids an explicit use of
// `new' in implemenation code.  The returned object must still be added
// to a container, or the allocation will leak.
template<typename T, typename... Args>
T* make_managed(Args&&... args) {
	return Gtk::manage(new T{args...});
}

// WebContext wraps the default WebKitWebContext.  Non-default contexts may be
// representable in later versions of WebKitGTK, but as of 2.6, it appears that
// only the default context ever exists.
//
// Additional details regarding the methods wrapped by this class can be
// found at: http://webkitgtk.org/reference/webkit2gtk/stable/WebKitWebContext.html
class WebContext {
private:
	// Underlying web context.
	WebKitWebContext * const wc;

public:
	// get_default returns a WebContext wrapping the default web context.
	// This is the only way to create a WebContext, since other contexts
	// other than the default cannot be constructed.
	static WebContext get_default();

	// set_process_model modifies the process model for a web context.
	// By default, a web context will use a single process to manage
	// all WebViews.
	//
	// It is unsafe to change the process model after WebViews have been
	// created.
	void set_process_model(WebKitProcessModel);

protected:
	// Constructor to wrap the default web context.  This is not public
	// since it may be possible to construct non-default web contexts
	// in the future.
	WebContext(WebKitWebContext *wc) : wc{wc} {}
};


// WebView is a thin wrapper around a WebKitWebView, deriving from a gtkmm
// widget so it may be used in combination with other GTK widgets.
//
// Additional details regarding the methods wrapped by this class can be
// found at: http://webkitgtk.org/reference/webkit2gtk/stable/WebKitWebView.html
class WebView : public Gtk::Widget {
private:
	// Signals.
	sigc::signal<void, WebKitLoadEvent> signal_load_changed;
	sigc::signal<void, WebKitBackForwardList *> signal_back_forward_list_changed;
	sigc::signal<void> signal_notify_title;
	sigc::signal<void> signal_notify_uri;

public:
	// Constructors to allocate and initialize a new WebKitWebView.  The
	// default constructor will load the blank about page, but a string
	// any other URI may be specified to begin loading the resource as the
	// WebView is created.
	WebView() : WebView{reinterpret_cast<WebKitWebView *>(webkit_web_view_new())} {}
	WebView(const Glib::ustring&);

	// load_uri begins the loading the URI described by uri in the WebView.
	void load_uri(const Glib::ustring&);

	// get_uri returns the URI of the WebView.
	Glib::ustring get_uri();

	// reload reloads the current WebView's URI.
	void reload();

	// go_back loads the previous history item.
	void go_back();

	// go_forward loads the next history item.
	void go_forward();

	// Signal connections.
	sigc::connection connect_load_changed(std::function<void(WebKitLoadEvent)>);
	sigc::connection connect_back_forward_list_changed(std::function<void(WebKitBackForwardList *)>);
	sigc::connection connect_notify_title(std::function<void()>);
	sigc::connection connect_notify_uri(std::function<void()>);

protected:
	// Constructor to create a WebView from a C pointer.
	WebView(WebKitWebView *wv);

public:
	// Accessor methods for the underlying GObject.
	WebKitWebView * gobj() { return reinterpret_cast<WebKitWebView *>(gobject_); }
	const WebKitWebView * gobj() const { return reinterpret_cast<WebKitWebView *>(gobject_); }
};


// URIEntry represents the URI entry box that is used as the Browser
// window's custom title.
class URIEntry : public Gtk::Entry {
private:
	bool editing{false};
	bool refresh_pressed{false};

	// Signals
	sigc::signal<void> signal_refresh;

public:
	// Default constructor.  No surprises here.
	URIEntry();

	// Setter for the text in the URI entry.  This only modifies the
	// text if the entry is not receiving input events (that is, when
	// it is not the grab widget).
	void set_uri(const Glib::ustring& uri);

	// signal_uri_entered returns a connectable signal which fires
	// whenever the URI is entered by the user using the entry.
	Glib::SignalProxy0<void> signal_uri_entered();

	// Signal connections.
	sigc::connection connect_refresh(std::function<void()>);

protected:
	bool on_button_release_event(GdkEventButton *) override;
	bool on_focus_out_event(GdkEventFocus *) override;
};


// Browser represents the top level widget which creates the browser.  It
// contains a navigation bar with buttons to move the current visable page
// back and forward in history and a URI entry to begin loading any other
// page.  Multiple webpages are managed via a GTK notebook with tabs under
// the navigation bar.
//
// A Browser will show no less than one tab at all times.  Removing the last
// tab will close the browser.
class Browser : public Gtk::Window {
private:
	// Tab represents the widgets added to the Browser's notebook.  Note
	// that there is an additional box which holds the tab's title and
	// close button that is not owned by this struct.
	struct Tab {
		WebView wv;
		Gtk::Label tab_title;
		Gtk::Button tab_close;
		Tab(const Glib::ustring&);
	};

	// A vector of Tab pointers is used instead of a vector of Tabs since
	// the Tab structs cannot be moved.  This appears to be a limitation
	// with gtkmm.
	std::vector<std::unique_ptr<Tab>> tabs;
	Gtk::HeaderBar navbar;
	Gtk::Box histnav;
	Gtk::Button back, fwd, new_tab;
	URIEntry nav_entry;
	Gtk::Notebook nb;
	// Details about the currently shown page.
	std::array<sigc::connection, 6> page_signals;
	struct VisableTab {
		unsigned int tab_index{0};
		WebView *webview{nullptr};
		WebKitBackForwardList *bfl{nullptr};
		VisableTab() {}
		VisableTab(unsigned int n, WebView& wv) : tab_index{n}, webview{&wv},
			bfl{webkit_web_view_get_back_forward_list(wv.gobj())} {}
	} visable_tab;

public:
	// Constructors to create the toplevel browser window widget.  Multiple
	// URIs (a "session") to open may be specified, while the default
	// constructor will open a single tab to a single blank page.
	Browser() : Browser{{""}} {}
	Browser(const std::vector<Glib::ustring>&);

	// open_new_tab creates a new tab, loading the specified resource, and
	// adds it to the Browser, appending the page to the end of the
	// notebook.  The notebook index is returned and may be used to switch
	// view to the newly opened tab.
	int open_new_tab(const Glib::ustring&);

private:
	void show_webview(unsigned int, WebView&);
	void switch_page(unsigned int) noexcept;
	void update_histnav(WebView&);

protected:
	bool on_key_press_event(GdkEventKey *) override;
};

} // namespace volo

#endif // _VOLO_H
