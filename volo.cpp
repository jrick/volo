// Copyright (c) 2014 Josh Rickmar.
// Use of this source code is governed by an ISC
// license that can be found in the LICENSE file.

#include <iostream>
#include <volo.h>

using namespace volo;

const Glib::ustring blank_page = "about:blank";

WebContext WebContext::get_default() {
	return WebContext{};
}

void WebContext::set_process_model(WebKitProcessModel model) {
	webkit_web_context_set_process_model(wc, model);
}


static void WebView_load_changed_callback(WebKitWebView *wv,
	WebKitLoadEvent ev, gpointer signal) {

	// TODO: only emit once, no matter how many times this was connected.
	static_cast<sigc::signal<void, WebKitLoadEvent> *>(signal)->emit(ev);
}

static void BackForwardList_changed_callback(WebKitBackForwardList *list,
	WebKitBackForwardListItem *item, gpointer removed, gpointer signal) {

	static_cast<sigc::signal<void, WebKitBackForwardList *> *>(signal)->emit(list);
}

static void WebView_notify_title_callback(GObject *, GParamSpec *, gpointer signal) {
	static_cast<sigc::signal<void> *>(signal)->emit();
}

WebView::WebView(WebKitWebView *wv) : Gtk::Widget{GTK_WIDGET(wv)} {
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
}

WebView::WebView(const Glib::ustring& uri) : WebView{} {
	load_uri(uri);
}

void WebView::load_uri(const Glib::ustring& uri) {
	webkit_web_view_load_uri(gobj(), uri.c_str());
}

void WebView::go_back() {
	webkit_web_view_go_back(gobj());
};

void WebView::go_forward() {
	webkit_web_view_go_forward(gobj());
};

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

void WebView::on_load_changed(WebKitLoadEvent ev) {
	std::cout << ev << '\n';
}


const std::initializer_list<Glib::ustring> default_session = {
	"https://duckduckgo.com/lite",
	"https://github.com",
};

Browser::Browser(const std::vector<Glib::ustring>& uris) {
	auto wc = WebContext::get_default();
	wc.set_process_model(WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);

	// Set navigation button images.  No gtkmm constructor wraps
	// gtk_button_new_from_icon_name (due to a conflict with
	// gtk_button_new_with_label) so this must be done in the Browser
	// constructor.
	back.set_image_from_icon_name("go-previous");
	fwd.set_image_from_icon_name("go-next");
	//stop.set_image_from_icon_name("process-stop");
	//reload.set_image_from_icon_name("view-refresh");

	auto histnav_style = histnav.get_style_context();
	histnav_style->add_class("raised");
	histnav_style->add_class("linked");
	histnav.add(back);
	histnav.add(fwd);
	navbar.add(histnav);

	nav_entry.set_input_purpose(Gtk::INPUT_PURPOSE_URL);
	nav_entry.set_hexpand(true);
	navbar.set_custom_title(nav_entry);

	navbar.set_show_close_button(true);

	set_title("volo");
	set_default_size(1024, 768);
	set_titlebar(navbar);
	add(nb);

	webviews.reserve(uris.size());
	for (const auto& uri : uris) {
		open_uri(uri);
	}

	nb.signal_switch_page().connect([this] (auto, guint page_num) {
		switch_page(page_num);
	});

	show_webview(*webviews.front());

	show_all_children();
}

int Browser::open_uri(const Glib::ustring& uri) {
	auto tab  = make_managed<Gtk::Grid>();
	auto title = make_managed<Gtk::Label>("New tab");
	auto close = make_managed<Gtk::Button>();

	close->set_image_from_icon_name("window-close");

	title->set_can_focus(false);
	close->set_can_focus(false);
	tab->set_can_focus(false);

	tab->add(*title);
	tab->add(*close);

	webviews.emplace_back(std::make_unique<WebView>(uri));
	const auto& wv = webviews.back();
	wv->show_all();
	tab->show_all();
	const auto n = nb.append_page(*wv, *tab);
	nb.set_tab_reorderable(*wv, true);

	close->signal_clicked().connect([this, &wv = *wv] {
		// NOTE: This is very fast because it does not need to
		// dereference every pointer in the webviews vector, but it
		// relies on the WebView RAII handle never being moved.
		// Currently this is safe because moving for this type is
		// disabled (no move constructors or assignment operators
		// exist).
		for (auto i = webviews.begin(); i != webviews.end(); ++i) {
			// Compare using pointer equality.  We intentionally
			// captured a reference to the WebView, and not the
			// unique_ptr<WebView>, so that we could take the
			// address of the actual WebView object without the
			// unique_ptr having been zeroed after a move.
			if (i->get() != &wv) {
				continue;
			}
			webviews.erase(i);
			break;
		}
		if (webviews.size() == 0) {
 			switch_page(open_uri("https://duckduckgo.com/lite"));
		}
	});

	return n;
}

void Browser::show_webview(WebView& wv) {
	// Update navbar with the current state of the webview being shown.
	// These details will be continuously updated with the callbacks set
	// below until a different webview is shown.
	update_histnav(wv);
	update_title(wv);

	page_signals = { {
		back.signal_clicked().connect([&wv] { wv.go_back(); }),
		fwd.signal_clicked().connect([&wv] { wv.go_forward(); }),
		wv.connect_load_changed(sigc::mem_fun(wv, &WebView::on_load_changed)),
		wv.connect_back_forward_list_changed([this](WebKitBackForwardList *bfl) {
			if (current_page.bfl == bfl) {
				update_histnav(*current_page.webview);
			}
		}),
		wv.connect_notify_title([this, &wv] { update_title(wv); }),
	} };
}

void Browser::update_histnav(WebView& wv) {
	const auto p = wv.gobj();
	back.set_sensitive(webkit_web_view_can_go_back(p));
	fwd.set_sensitive(webkit_web_view_can_go_forward(p));
}

void Browser::update_title(WebView& wv) {
	auto c_title = webkit_web_view_get_title(wv.gobj());
	set_title(c_title ? c_title : "volo");
}

void Browser::switch_page(uint page_num) {
	// Disconnect previous WebView's signals before showing and connecting
	// the new WebView.
	for (auto& sig : page_signals) {
		sig.disconnect();
	}

	auto& wv = webviews.at(page_num);
	current_page = PageContext{page_num, wv};
	show_webview(*wv);
}


int main(int argc, char **argv) {
	const auto app = Gtk::Application::create(argc, argv, "org.jrick.volo");

	Browser b;
	return app->run(b);
}
