// Copyright (c) 2014 Josh Rickmar.
// Use of this source code is governed by an ISC
// license that can be found in the LICENSE file.

#include <volo.h>
#include <iostream>

using namespace volo;

const Glib::ustring blank_page = "about:blank";

WebContext::WebContext(WebKitProcessModel model) : wc{webkit_web_context_get_default()} {
	set_process_model(model);
}

WebContext::WebContext() : WebContext{WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES} {}

void WebContext::set_process_model(WebKitProcessModel model) {
	webkit_web_context_set_process_model(wc, model);
}


WebView::WebView() : WebView{blank_page} {}

WebView::WebView(const Glib::ustring& uri) : wv{WEBKIT_WEB_VIEW(webkit_web_view_new())} {
	// Create underlying webview and wrap in a gtkmm widget.  Destruction
	// of the widget is delegated to the derived container.
	const auto mw = Gtk::manage(Glib::wrap(GTK_WIDGET(wv)));
	mw->show();
	add(*mw);

	load_uri(uri);
}

WebView::WebView

void WebView::load_uri(const Glib::ustring& uri) {
	webkit_web_view_load_uri(wv, uri.c_str());
}

static void WebView_connect_load_changed_callback(WebKitWebView *wv,
	WebKitLoadEvent ev, gpointer signal) {

	// TODO: only emit once, no matter how many times this was connected.
	static_cast<sigc::signal<void, WebKitLoadEvent> *>(signal)->emit(ev);
}

sigc::connection WebView::connect_load_changed(std::function<void(WebKitLoadEvent)> handler) {
	g_signal_connect(wv, "load-changed",
		G_CALLBACK(WebView_connect_load_changed_callback),
		&signal_load_changed);
	return signal_load_changed.connect(handler);
}

void WebView::on_load_changed(WebKitLoadEvent ev) {
	std::cout << ev << '\n';
}


const std::initializer_list<Glib::ustring> default_session = {
	"https://duckduckgo.com/lite",
	"https://github.com",
};

Browser::Browser() : Browser{default_session} {} // TODO: use about:blank

Browser::Browser(const std::vector<Glib::ustring>& uris) {
	// Set navigation button images.  No gtkmm constructor wraps
	// gtk_button_new_from_icon_name (due to a conflict with
	// gtk_button_new_with_label) so this must be done in the Browser
	// constructor.
	back.set_image_from_icon_name("go-previous");
	fwd.set_image_from_icon_name("go-next");
	//stop.set_image_from_icon_name("process-stop");
	//reload.set_image_from_icon_name("view-refresh");

	navbar.add(back);
	navbar.add(fwd);

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

	show_webview(*webviews.front());

	show_all_children();
}

int Browser::open_uri(const Glib::ustring& uri) {
	const auto tab  = make_managed<Gtk::Grid>();
	const auto title = make_managed<Gtk::Label>("New tab");
	const auto close = make_managed<Gtk::Button>();

	close->set_image_from_icon_name("window-close");

	title->set_can_focus(false);
	close->set_can_focus(false);
	tab->set_can_focus(false);

	tab->add(*title);
	tab->add(*close);
	tab->show_all_children();

	webviews.emplace_back(std::make_unique<WebView>(uri));
	const auto& back_iter = webviews.back();
	const auto n = nb.append_page(*back_iter, *tab);
	nb.set_tab_reorderable(*back_iter, true);
	return n;
}

void Browser::show_webview(WebView &wv) {
	page_signals = { {
		back.signal_clicked().connect([] { std::cout << "back\n"; }),
		fwd.signal_clicked().connect([] { std::cout << "fwd\n"; }),
		wv.connect_load_changed(sigc::mem_fun(wv, &WebView::on_load_changed)),
	} };
	back.signal_clicked().connect([] { std::cout << "lalala\n"; });
}

void Browser::switch_webview(WebView &wv) {
	// Disconnect previous WebView's signals before showing and connecting
	// the new WebView.
	for (auto& sig : page_signals) {
		sig.disconnect();
	}

	show_webview(wv);
}


int main(int argc, char **argv) {
	const auto app = Gtk::Application::create(argc, argv, "org.jrick.volo");

	Browser b;
	return app->run(b);
}
