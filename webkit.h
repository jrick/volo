// Copyright (c) 2014 Josh Rickmar.
// Use of this source code is governed by an ISC
// license that can be found in the LICENSE file.

#ifndef _WEBKIT_H
#define _WEBKIT_H

#include <string>

#include <webkit2/webkit2.h>

#include <gtk.h>

namespace webkit {

template <class T = WebKitWebContext>
struct web_context : gtk::gobject<T> {
	using c_type = WebKitWebContext;

	// get_default returns the wrapped default WebKitWebContext.
	// Non-default web_contexts may be representable in later versions
	// of WebKitGTK, but as of 2.6, it appears that only the default context
	// ever exists.
	//
	// Because user-created web_contexts cannot be created, there is no need
	// for a handle type.
	static auto get_default() {
		return reinterpret_cast<web_context *>(webkit_web_context_get_default());
	}

	// set_process_model modifies the process model for a web context.
	// By default, a web context will use a single process to manage
	// all WebViews.
	//
	// It is unsafe to change the process model after WebViews have been
	// created.
	void set_process_model(WebKitProcessModel model) {
		webkit_web_context_set_process_model(ptr(), model);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

template <class T = WebKitWebView>
struct web_view : gtk::widget<T> {
	using c_type = WebKitWebView;

	static auto create() {
		return reinterpret_cast<web_view *>(webkit_web_view_new());
	}

	static auto create(const char *uri) {
		auto ptr = create();
		ptr->load_uri(uri);
		return ptr;
	}
	static auto create(const std::string& uri) {
		return create(uri.c_str());
	}

	// load_uri begins the loading the URI described by uri in the web_view.
	void load_uri(const std::string& uri) {
		load_uri(uri.c_str());
	}
	void load_uri(const char *uri) {
		webkit_web_view_load_uri(ptr(), uri);
	}

	// get_uri returns the URI of the web_view.
	const char * get_uri() {
		auto uri = webkit_web_view_get_uri(ptr());
		return uri ? uri : "";
	}

	const char * get_title() {
		auto title = webkit_web_view_get_title(ptr());
		return title ? title : "";
	}

	// reload reloads the current web_view's URI.
	void reload() {
		webkit_web_view_reload(ptr());
	}

	// go_back loads the previous history item.
	void go_back() {
		webkit_web_view_go_back(ptr());
	}

	// go_forward loads the next history item.
	void go_forward() {
		webkit_web_view_go_forward(ptr());
	}

	// can_go_back returns whether there is a previous history item the
	// webview can navigate back to.
	bool can_go_back() {
		return webkit_web_view_can_go_back(ptr());
	}

	// can_go_forward returns whether there is a next history item the
	// webview can nagivate forward to.
	bool can_go_forward() {
		return webkit_web_view_can_go_forward(ptr());
	}

	// get_back_forward_list returnes the back forward list associated with
	// the webview.  It is owned by webview and may not be destroyed by the
	// caller (if wrapped, use gtk::handles::ref_counted or
	// gtk::handles::floating).
	WebKitBackForwardList * get_back_forward_list() {
		return webkit_web_view_get_back_forward_list(ptr());
	}

	// Signals

	template <class U>
	using back_forward_list_changed_slot = void (*)(WebKitBackForwardList *,
		WebKitBackForwardListItem *, gpointer, U *);
	template <class U>
	gtk::connection connect_back_forward_list_changed(U& obj, back_forward_list_changed_slot<U> slot) {
		auto bfl = get_back_forward_list();
		auto bfl_obj = static_cast<gtk::gobject<WebKitBackForwardList> *>(bfl);
		return bfl_obj->connect("changed", G_CALLBACK(slot), &obj);
	}

	template <class U>
	using notify_title_slot = void (*)(web_view *, GParamSpec *, U *);
	template <class U>
	gtk::connection connect_notify_title(U& obj, notify_title_slot<U> slot) {
		return this->connect("notify::title", G_CALLBACK(slot), &obj);
	}

	template <class U>
	using notify_uri_slot = void (*)(web_view *, GParamSpec *, U *);
	template <class U>
	gtk::connection connect_notify_uri(U& obj, notify_uri_slot<U> slot) {
		return this->connect("notify::uri", G_CALLBACK(slot), &obj);
	}

	c_type * ptr() { return reinterpret_cast<c_type *>(this); }
};

} // namespace webkit

#endif // _WEBKIT_H
