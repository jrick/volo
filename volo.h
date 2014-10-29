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
// only the default 
class WebContext {
private:
	WebKitWebContext * const wc;

public:
	WebContext();
	WebContext(WebKitProcessModel);

	void set_process_model(WebKitProcessModel);
};


// WebView is a thin wrapper around a WebKitWebView, deriving from a gtkmm
// widget so it may be used in combination with other GTK widgets.
class WebView : public Gtk::Widget {
private:
	// Signals.
	sigc::signal<void, WebKitLoadEvent> signal_load_changed;

public:
	// Constructors to allocate and initialize a new WebKitWebView.  The
	// default constructor will load the blank about page, but a string
	// any other URI may be specified to begin loading the resource as the
	// WebView is created.
	WebView() : WebView{WEBKIT_WEB_VIEW(webkit_web_view_new())} {}
	WebView(const Glib::ustring&);

	// load_uri begins the loading the URI described by uri in the WebView.
	//
	// See http://webkitgtk.org/reference/webkit2gtk/stable/WebKitWebView.html#webkit-web-view-load-uri
	// for additional details.
	void load_uri(const Glib::ustring&);

	// Signal connections.
	sigc::connection connect_load_changed(std::function<void(WebKitLoadEvent)>);

	// Slot..
	void on_load_changed(WebKitLoadEvent);

protected:
	// Constructor to create a WebView from a C pointer.
	WebView(WebKitWebView *wv) : Gtk::Widget{GTK_WIDGET(wv)} {}

	// Accessor methods for the underlying GObject.
	WebKitWebView * gobj() { return reinterpret_cast<WebKitWebView *>(gobject_); }
	const WebKitWebView * gobj() const { return reinterpret_cast<WebKitWebView *>(gobject_); }
};


// Browser represents the top level widget which creates the browser.  It
// contains a navigation bar with buttons to move the current visable page
// back and forward in history and a URI entry to begin loading any other
// page.  Multiple webpages are managed via a GTK notebook with tabs under
// the navigation bar.
//
// A Browser will show no less than one tab at all times.  Removing the last
// tab will open a new one to the blank page.
class Browser : public Gtk::Window {
private:
	WebContext wc;
	// A vector of WebView pointers is used instead of a vector of WebViews
	// since no move constructor is defined for WebView and gtkmm classes
	// from which it derives.  This appears to be a limitation with gtkmm.
	std::vector<std::unique_ptr<WebView>> webviews;
	Gtk::HeaderBar navbar;
	Gtk::Button back, fwd, stop, reload;
	Gtk::Entry nav_entry;
	Gtk::Notebook nb;
	std::array<sigc::connection, 3> page_signals;

public:
	// Constructors to create the toplevel browser window widget.  Multiple
	// URIs (a "session") to open may be specified, while the default
	// constructor will open a single tab to a single blank page.
	Browser();
	Browser(const std::vector<Glib::ustring>&);

	// open_uri opens a new tab and begins loading the specified resource.
	int open_uri(const Glib::ustring&);

private:
	void show_webview(WebView &);
	void switch_webview(WebView &);
};

} // namespace volo

#endif // _VOLO_H
