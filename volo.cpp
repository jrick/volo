// Copyright (c) 2014 Josh Rickmar.
// Use of this source code is governed by an ISC
// license that can be found in the LICENSE file.

#include <volo.h>
#include <algorithm>
#include <gdk/gdkkeysyms.h>

using namespace volo;

const std::array<Glib::ustring, 2> recognized_uri_schemes = { {
	"http://",
	"https://",
} };

// has_prefix returns whether p is a prefix of s.
template <typename T>
bool has_prefix(const T& s, const T& p) {
	return s.size() >= p.size() && std::equal(p.begin(), p.end(), s.begin());
}

void guess_uri(Glib::ustring& uri) {
	for (const auto& scheme : recognized_uri_schemes) {
		if (has_prefix(uri, scheme)) {
			return;
		}
	}
	uri = "http://" + uri;
}

WebContext WebContext::get_default() {
	return webkit_web_context_get_default();
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
	set_hexpand(true);

	set_input_purpose(Gtk::INPUT_PURPOSE_URL);
	set_icon_from_icon_name("view-refresh", Gtk::ENTRY_ICON_SECONDARY);

	signal_icon_press().connect([this](Gtk::EntryIconPosition pos, ...) {
		refresh_pressed = true;
	});
}

Glib::SignalProxy0<void> URIEntry::signal_uri_entered() {
	return signal_activate();
}

void URIEntry::set_uri(const Glib::ustring& uri) {
	if (!has_grab()) {
		set_text(uri);
	}
}

bool URIEntry::on_button_release_event(GdkEventButton *ev) {
	// If the entry's refresh button was pressed, fire the refresh
	// signal.  In this case we do not set editing mode to true and
	// do not grab focus to highlight all text in the entry.
	if (refresh_pressed) {
		signal_refresh.emit();
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
	return Gtk::Entry::on_button_release_event(ev);
}

bool URIEntry::on_focus_out_event(GdkEventFocus *f) {
	select_region(0, 0);
	editing = false;
	return Gtk::Entry::on_focus_out_event(f);
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

	back.set_can_focus(false);
	fwd.set_can_focus(false);
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
		auto uri = nav_entry.get_text();
		guess_uri(uri);
		visable_tab.webview->load_uri(uri);
		visable_tab.webview->grab_focus();
	});
	nb.signal_switch_page().connect([this](auto, guint page_num) {
		switch_page(page_num);
	});
	nb.signal_page_added().connect([this](...) {
		nb.set_show_tabs(true);
	});
	nb.signal_page_removed().connect([this](...) {
		nb.set_show_tabs(nb.get_n_pages() > 1);
	});
	new_tab.signal_clicked().connect([this] {
		auto n = open_new_tab("");
		nb.set_current_page(n);
	});

	show_webview(0, tabs.front()->wv);

	show_all_children();
}

bool Browser::on_key_press_event(GdkEventKey *ev) {
	if (Gtk::Widget::on_key_press_event(ev)) {
		return true;
	}

	if (ev->state == GDK_CONTROL_MASK) {
		auto kv = ev->keyval;
		if (kv == GDK_KEY_l) {
			nav_entry.grab_focus();
			return true;
		} else if (kv == GDK_KEY_t) {
			auto n = open_new_tab("");
			nb.set_current_page(n);
			return true;
		} else if (kv == GDK_KEY_w) {
			tabs.erase(tabs.begin() + visable_tab.tab_index);
			if (tabs.size() == 0) {
				destroy_();
			}
			return true;
		} else if (kv == GDK_KEY_q) {
			tabs.clear();
			destroy_();
			return true;
		} else if (kv >= GDK_KEY_1 && kv <= GDK_KEY_8) {
			auto n = kv - GDK_KEY_1;
			if (tabs.size() > n) {
				switch_page(n);
				nb.set_current_page(n);
			}
			return true;
		} else if (kv == GDK_KEY_9) {
			auto n = tabs.size() - 1;
			switch_page(n);
			nb.set_current_page(n);
			return true;
		}
	}

	return false;
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
			destroy_();
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
	visable_tab = {page_num, wv};

	// Update navbar/titlebar with the current state of the webview being
	// shown.
	auto c_title = webkit_web_view_get_title(wv.gobj());
	set_title(c_title ? c_title : "volo");
	update_histnav(wv);
	auto uri = wv.get_uri();
	nav_entry.set_uri(uri);

	page_signals = { {
		back.signal_clicked().connect([&wv] { wv.go_back(); }),
		fwd.signal_clicked().connect([&wv] { wv.go_forward(); }),
		wv.connect_back_forward_list_changed([this](WebKitBackForwardList *bfl) {
			if (visable_tab.bfl == bfl) {
				update_histnav(*visable_tab.webview);
			}
		}),
		wv.connect_notify_uri([this, &wv] { nav_entry.set_uri(wv.get_uri()); }),
		nav_entry.connect_refresh([&wv] {
			wv.reload();
			wv.grab_focus();
		}),
		nb.signal_page_reordered().connect([this](auto, guint new_idx) {
			// NOTE: This only works when reordering the current visable tab.
			// However, this appears to be a safe assumption since, at least
			// when using the mouse to drag and drop tabs (which is the only
			// method we have of reordering them), the tab being reordered
			// is always focused first.
			auto old_idx = visable_tab.tab_index;
			auto tmp = std::move(tabs[old_idx]);
			if (old_idx < new_idx) {
				// Moving the range [old_idx+1, new_idx+1) to [old_idx, new_idx).
				std::move(tabs.begin()+old_idx+1, tabs.begin()+new_idx+1,
					tabs.begin()+old_idx);
			} else {
				// Moving the range [new_idx, old_idx) to [new_idx+1, old_idx+1).
				std::move_backward(tabs.begin()+new_idx, tabs.begin()+old_idx,
					tabs.begin()+old_idx+1);
			}
			tabs[new_idx] = std::move(tmp);
			visable_tab.tab_index = new_idx;
		}),
	} };

	// Grab URI entry focus if the shown tab is blank.
	//
	// TODO: If this webview is being shown by switching notebook tabs,
	// grabbing the entry focus has no effect.
	if (uri == "") {
		nav_entry.grab_focus();
	} else {
		wv.grab_focus();
	}
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
