// Copyright (c) 2014 Josh Rickmar.
// Use of this source code is governed by an ISC
// license that can be found in the LICENSE file.

#ifndef _VOLO_H
#define _VOLO_H

#include <array>
#include <vector>

#include <gtk.h>
#include <webkit.h>
#include <uri_entry.h>

namespace volo {

// browser_tab represents the widgets added to the browser's notebook.  Note
// that there is an additional box which holds the tab's title and close button
// that is not owned by this struct.
struct browser_tab {
	gtk::unique_ptr<webkit::web_view> wv;
	gtk::unique_ptr<gtk::label> tab_title{gtk::make_unique<gtk::label>()};
	gtk::unique_ptr<gtk::button> tab_close{
		gtk::make_unique<gtk::button>("window-close", GTK_ICON_SIZE_BUTTON)
	};

	browser_tab(const char *);
};


// browser represents the top level widget which creates the browser.  It
// contains a navigation bar with buttons to move the current visable page
// back and forward in history and a URI entry to begin loading any other
// page.  Multiple webpages are managed via a GTK notebook with tabs under
// the navigation bar.
//
// A browser will show no less than one tab at all times.  Removing the last
// tab will close the browser.
class browser {
private:
	std::vector<browser_tab> tabs;
	gtk::unique_ptr<gtk::window> window{gtk::make_unique<gtk::window>()};
	gtk::unique_ptr<gtk::header_bar> navbar{gtk::make_unique<gtk::header_bar>()};
	gtk::unique_ptr<gtk::box> histnav{gtk::make_unique<gtk::box>()};
	gtk::unique_ptr<gtk::button> back{
		gtk::make_unique<gtk::button>("go-previous", GTK_ICON_SIZE_BUTTON)
	};
	gtk::unique_ptr<gtk::button> fwd{
		gtk::make_unique<gtk::button>("go-next", GTK_ICON_SIZE_BUTTON)
	};
	gtk::unique_ptr<gtk::button> new_tab{
		gtk::make_unique<gtk::button>("add", GTK_ICON_SIZE_BUTTON)
	};
	gtk::unique_ptr<uri_entry> nav_entry{gtk::make_unique<uri_entry>()};
	gtk::unique_ptr<gtk::notebook> nb{gtk::make_unique<gtk::notebook>()};
	// Details about the currently shown page.
	std::array<gtk::connection, 7> page_signals;
	struct visable_tab {
		unsigned int tab_index{0};
		webkit::web_view *web_view{nullptr};
		WebKitBackForwardList *bfl{nullptr};
		visable_tab() {}
		visable_tab(unsigned int n, webkit::web_view& wv) :
			tab_index{n}, web_view{&wv}, bfl{wv.get_back_forward_list()} {}
	} visable_tab;

public:
	// Constructors to create the toplevel browser window widget.  Multiple
	// URIs (a "session") to open may be specified, while the default
	// constructor will open a single tab to a single blank page.
	browser() : browser{std::vector<const char *>{""}} {}
	browser(const std::vector<const char *>&);

	// open_new_tab creates a new tab, loading the specified resource, and
	// adds it to the browser, appending the page to the end of the
	// notebook.  The notebook index is returned and may be used to switch
	// view to the newly opened tab.
	int open_new_tab(const char *);

	// show_window calls the show method of the browser's window widget.
	void show_window();

private:
	void show_webview(unsigned int, webkit::web_view&);
	void switch_page(unsigned int);
	void update_histnav(webkit::web_view&);

	// Slots (member functions)
	void on_nav_entry_activate(uri_entry&);
	void on_notebook_switch_page(gtk::notebook&, gtk::widget&, unsigned int);
	void on_notebook_page_added(gtk::notebook&, gtk::widget&, unsigned int);
	void on_notebook_page_removed(gtk::notebook&, gtk::widget&, unsigned int);
	void on_notebook_page_reordered(gtk::notebook&, gtk::widget&, unsigned int);
	void on_new_tab_clicked(gtk::button&);
	bool on_window_key_press_event(gtk::window&, GdkEventKey&);
	void on_window_destroy(gtk::window&);
	void on_tab_close_clicked(gtk::button&);
	void on_back_button_clicked(gtk::button&);
	void on_fwd_button_clicked(gtk::button&);
	void on_back_forward_list_changed(WebKitBackForwardList&, WebKitBackForwardListItem&,
		gpointer);
	void on_web_view_load_changed(webkit::web_view&, WebKitLoadEvent);
	void on_web_view_notify_uri(webkit::web_view&, GParamSpec&);
	void on_web_view_notify_title(webkit::web_view&, GParamSpec&);

	// Slots (static functions)
	static void on_nav_entry_activate(uri_entry *entry, browser *b) {
		return b->on_nav_entry_activate(*entry);
	}
	static void on_notebook_switch_page(gtk::notebook *notebook, gtk::widget *widget,
		unsigned int page_num, browser *b) {
		b->on_notebook_switch_page(*notebook, *widget, page_num);
	}
	static void on_notebook_page_added(gtk::notebook *notebook, gtk::widget *widget,
		unsigned int page_num, browser *b) {
		b->on_notebook_page_added(*notebook, *widget, page_num);
	}
	static void on_notebook_page_removed(gtk::notebook *notebook, gtk::widget *widget,
		unsigned int page_num, browser *b) {
		b->on_notebook_page_removed(*notebook, *widget, page_num);
	}
	static void on_notebook_page_reordered(gtk::notebook *notebook, gtk::widget *child,
		unsigned int page_num, browser *b) {
		b->on_notebook_page_reordered(*notebook, *child, page_num);
	}
	static void on_new_tab_clicked(gtk::button *button, browser *b) {
		b->on_new_tab_clicked(*button);
	}
	static bool on_window_key_press_event(gtk::window *w, GdkEventKey *ev, browser *b) {
		return b->on_window_key_press_event(*w, *ev);
	}
	static void on_window_destroy(gtk::window *w, browser *b) {
		return b->on_window_destroy(*w);
	}
	static void on_tab_close_clicked(gtk::button *button, browser *b) {
		b->on_tab_close_clicked(*button);
	}
	static void on_back_button_clicked(gtk::button *button, browser *b) {
		b->on_back_button_clicked(*button);
	}
	static void on_fwd_button_clicked(gtk::button *button, browser *b) {
		b->on_fwd_button_clicked(*button);
	}
	static void on_back_forward_list_changed(WebKitBackForwardList *button,
		WebKitBackForwardListItem *item_added, gpointer items_removed, browser *b) {
		b->on_back_forward_list_changed(*button, *item_added, items_removed);
	}
	static void on_web_view_load_changed(webkit::web_view *web_view,
		WebKitLoadEvent load_event, browser *b) {
		b->on_web_view_load_changed(*web_view, load_event);
	}
	static void on_web_view_notify_uri(webkit::web_view *web_view,
		GParamSpec *param_spec, browser *b) {
		b->on_web_view_notify_uri(*web_view, *param_spec);
	}
	static void on_web_view_notify_title(webkit::web_view *web_view,
		GParamSpec *param_spec, browser *b) {
		b->on_web_view_notify_title(*web_view, *param_spec);
	}
};

} // namespace volo

#endif // _VOLO_H
