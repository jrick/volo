// Copyright (c) 2014 Josh Rickmar.
// Use of this source code is governed by an ISC
// license that can be found in the LICENSE file.

#include <volo.h>

using namespace volo;

WebContext WebContext::get_default() {
	return WebContext{};
}

void WebContext::set_process_model(WebKitProcessModel model) {
	webkit_web_context_set_process_model(wc, model);
}


static void WebView_load_changed_callback(WebKitWebView *wv,
	WebKitLoadEvent ev, gpointer signal) {

	static_cast<sigc::signal<void, WebKitLoadEvent> *>(signal)->emit(ev);
}

static void BackForwardList_changed_callback(WebKitBackForwardList *list,
	WebKitBackForwardListItem *item, gpointer removed, gpointer signal) {

	static_cast<sigc::signal<void, WebKitBackForwardList *> *>(signal)->emit(list);
}

static void WebView_notify_title_callback(GObject *, GParamSpec *, gpointer signal) {
	static_cast<sigc::signal<void> *>(signal)->emit();
}

static void WebView_notify_uri_callback(GObject *, GParamSpec *, gpointer signal) {
	static_cast<sigc::signal<void> *>(signal)->emit();
}

WebView::WebView(WebKitWebView *wv) : Gtk::Widget{reinterpret_cast<GtkWidget *>(wv)} {
	g_signal_connect(wv, "load-changed",
		G_CALLBACK(WebView_load_changed_callback),
		&signal_load_changed);

	auto bfl = webkit_web_view_get_back_forward_list(wv);
	g_signal_connect(bfl, "changed",
		G_CALLBACK(BackForwardList_changed_callback),
		&signal_back_forward_list_changed);

	g_signal_connect(wv, "notify::title",
		G_CALLBACK(WebView_notify_title_callback),
		&signal_notify_title);

	g_signal_connect(wv, "notify::uri",
		G_CALLBACK(WebView_notify_uri_callback),
		&signal_notify_uri);
}

WebView::WebView(const Glib::ustring& uri) : WebView{} {
	load_uri(uri);
}

void WebView::load_uri(const Glib::ustring& uri) {
	webkit_web_view_load_uri(gobj(), uri.c_str());
}

Glib::ustring WebView::get_uri() {
	auto uri = webkit_web_view_get_uri(gobj());
	return uri ? uri : "";
}

void WebView::reload() {
	webkit_web_view_reload(gobj());
}

void WebView::go_back() {
	webkit_web_view_go_back(gobj());
}

void WebView::go_forward() {
	webkit_web_view_go_forward(gobj());
}

sigc::connection WebView::connect_load_changed(
	std::function<void(WebKitLoadEvent)> handler) {

	return signal_load_changed.connect(handler);
}

sigc::connection WebView::connect_back_forward_list_changed(
	std::function<void(WebKitBackForwardList *)> handler) {

	return signal_back_forward_list_changed.connect(handler);
}

sigc::connection WebView::connect_notify_title(std::function<void()> handler) {
	return signal_notify_title.connect(handler);
}

sigc::connection WebView::connect_notify_uri(std::function<void()> handler) {
	return signal_notify_uri.connect(handler);
}


URIEntry::URIEntry() {
	set_size_request(600, -1); // TODO: make this dynamic with the window size
	set_margin_left(6);
	set_margin_right(6);
	entry.set_hexpand(true);

	entry.set_input_purpose(Gtk::INPUT_PURPOSE_URL);
	entry.set_icon_from_icon_name("view-refresh", Gtk::ENTRY_ICON_SECONDARY);

	entry.signal_button_release_event().connect(sigc::mem_fun(*this,
		&URIEntry::on_button_release_event), false);
	entry.property_is_focus().signal_changed().connect([this](...) {
		if (!entry.is_focus()) {
			entry.select_region(0, 0);
			editing = false;
		}
	});
	entry.signal_icon_press().connect([this](Gtk::EntryIconPosition pos, ...) {
		refresh_pressed = true;
	});
	entry.signal_icon_release().connect([this](Gtk::EntryIconPosition pos, ...) {
		if (refresh_pressed) {
			signal_refresh.emit();
		}
		refresh_pressed = false;
	});

	add(entry);

	show_all_children();
}

Glib::SignalProxy0<void> URIEntry::signal_uri_entered() {
	return entry.signal_activate();
}

Glib::ustring URIEntry::get_uri() {
	return entry.get_text();
}

void URIEntry::set_uri(const Glib::ustring& uri) {
	entry.set_text(uri);
}

// This can't be a lambda since we can't create sigc::slots from lambdas
// with return values.
bool URIEntry::on_button_release_event(GdkEventButton *) {
	if (!editing && !refresh_pressed) {
		int start, end;
		if (!entry.get_selection_bounds(start, end)) {
			entry.grab_focus();
		}
	}
	editing = true;
	return false;
}

sigc::connection URIEntry::connect_refresh(std::function<void()> handler) {
	return signal_refresh.connect(handler);
}


Browser::Browser(const std::vector<Glib::ustring>& uris) {
	// Set navigation button images.  No gtkmm constructor wraps
	// gtk_button_new_from_icon_name (due to a conflict with
	// gtk_button_new_with_label) so this must be done in the Browser
	// constructor.
	back.set_image_from_icon_name("go-previous");
	fwd.set_image_from_icon_name("go-next");
	new_tab.set_image_from_icon_name("add");

	auto histnav_style = histnav.get_style_context();
	histnav_style->add_class("raised");
	histnav_style->add_class("linked");
	histnav.add(back);
	histnav.add(fwd);
	navbar.pack_start(histnav);

	navbar.set_custom_title(nav_entry);

	new_tab.set_can_focus(false);
	new_tab.set_relief(Gtk::RELIEF_NONE);
	new_tab.show();
	navbar.pack_end(new_tab);

	navbar.set_show_close_button(true);

	auto num_uris = uris.size();
	nb.set_show_tabs(num_uris > 1);

	set_title("volo");
	set_default_size(1024, 768);
	set_titlebar(navbar);
	nb.set_scrollable();
	add(nb);

	tabs.reserve(num_uris);
	for (const auto& uri : uris) {
		open_new_tab(uri);
	}

	nav_entry.signal_uri_entered().connect([this] {
		auto text = nav_entry.get_uri();
		// TODO: this could not be a valid uri.
		visable_tab.webview->load_uri(text);
		visable_tab.webview->grab_focus();
	});
	nb.signal_switch_page().connect([this] (auto, guint page_num) {
		switch_page(page_num);
	});
	nb.signal_page_added().connect([this] (...) {
		nb.set_show_tabs(true);
	});
	nb.signal_page_removed().connect([this] (...) {
		nb.set_show_tabs(nb.get_n_pages() > 1);
	});
	new_tab.signal_clicked().connect([this] {
		auto n = open_new_tab("");
		nb.set_current_page(n);
	});

	show_webview(0, tabs.front()->wv);

	show_all_children();
}

Browser::Tab::Tab(const Glib::ustring& uri) : wv{uri}, tab_title{"New tab"} {
	tab_title.set_can_focus(false);
	tab_title.set_hexpand(true);
	tab_title.set_ellipsize(Pango::ELLIPSIZE_END);
	tab_title.set_size_request(50, -1);

	tab_close.set_image_from_icon_name("window-close");
}

int Browser::open_new_tab(const Glib::ustring& uri) {
	tabs.emplace_back(std::make_unique<Tab>(uri));
	const auto& tab = tabs.back();
	auto& wv = tab->wv;

	auto tab_content = make_managed<Gtk::Box>();
	tab_content->set_can_focus(false);
	tab_content->add(tab->tab_title);
	tab_content->add(tab->tab_close);

	wv.show_all();
	tab_content->show_all();
	const auto n = nb.append_page(wv, *tab_content);
	nb.set_tab_reorderable(wv, true);

	tab->tab_close.signal_clicked().connect([this, &tab = *tab] {
		// NOTE: This is very fast because it does not need to
		// dereference every pointer in the tabs vector, but it
		// relies on the Tab struct never being moved.  Currently
		// this holds true because moving for this type is disabled
		// (no move constructors or assignment operators exists).
		for (auto it = tabs.begin(); it != tabs.end(); ++it) {
			// Compare using pointer equality.  We intentionally
			// captured a reference to the Tab, and not the
			// vector's unique_ptr<Tab>, so that we could take the
			// address of the actual Tab object without the
			// vector's unique_ptr having been zeroed after a move.
			if (it->get() != &tab) {
				continue;
			}
			tabs.erase(it);
			break;
		}
		if (tabs.size() == 0) {
			close();
		}
	});
	wv.connect_notify_title([this, &tab = *tab] {
		// Update tab title and the window title (if this tab is
		// currently visable) with the new page title.
		auto& wv = tab.wv;
		auto c_title = webkit_web_view_get_title(wv.gobj());
		Glib::ustring title{c_title ? c_title : "(Untitled)"};
		tab.tab_title.set_text(title);
		if (visable_tab.webview == &wv) {
			set_title(title);
		}
	});

	return n;
}

void Browser::show_webview(unsigned int page_num, WebView& wv) {
	visable_tab = VisableTab{page_num, wv};

	// Update navbar/titlebar with the current state of the webview being
	// shown.
	auto c_title = webkit_web_view_get_title(wv.gobj());
	set_title(c_title ? c_title : "volo");
	update_histnav(wv);
	nav_entry.set_uri(wv.get_uri());

	page_signals = { {
		back.signal_clicked().connect([&wv] { wv.go_back(); }),
		fwd.signal_clicked().connect([&wv] { wv.go_forward(); }),
		wv.connect_back_forward_list_changed([this](WebKitBackForwardList *bfl) {
			if (visable_tab.bfl == bfl) {
				update_histnav(*visable_tab.webview);
			}
		}),
		wv.connect_notify_uri([this, &wv] { nav_entry.set_uri(wv.get_uri()); }),
		nav_entry.connect_refresh([&wv] { wv.reload(); }),
	} };
}

void Browser::update_histnav(WebView& wv) {
	const auto p = wv.gobj();
	back.set_sensitive(webkit_web_view_can_go_back(p));
	fwd.set_sensitive(webkit_web_view_can_go_forward(p));
}

void Browser::switch_page(unsigned int page_num) noexcept {
	// Disconnect previous WebView's signals before showing and connecting
	// the new WebView.
	for (auto& sig : page_signals) {
		sig.disconnect();
	}

	show_webview(page_num, tabs.at(page_num)->wv);
}


int main(int argc, char **argv) {
	const auto app = Gtk::Application::create(argc, argv, "org.jrick.volo");

	auto wc = WebContext::get_default();
	wc.set_process_model(WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);

	Browser b;
	return app->run(b);
}
