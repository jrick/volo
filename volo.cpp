// Copyright (c) 2014 Josh Rickmar.
// Use of this source code is governed by an ISC
// license that can be found in the LICENSE file.

#include <algorithm>
#include <string>

#include <volo.h>
#include <gdk/gdkkeysyms.h>

using namespace volo;

const std::array<std::string, 2> recognized_uri_schemes = { {
	"http://",
	"https://",
} };

// has_prefix returns whether p is a prefix of s.
template <typename T>
bool has_prefix(const T& s, const T& p) {
	return s.size() >= p.size() && std::equal(p.begin(), p.end(), s.begin());
}

void guess_uri(std::string& uri) {
	for (auto& scheme : recognized_uri_schemes) {
		if (has_prefix(uri, scheme)) {
			return;
		}
	}
	uri = "http://" + uri;
}

browser::browser(const std::vector<const char *>&uris) {
	back->set_can_focus(false);
	fwd->set_can_focus(false);
	auto histnav_style = histnav->get_style_context();
	histnav_style->add_class("raised");
	histnav_style->add_class("linked");
	histnav->add(*back);
	histnav->add(*fwd);
	navbar->pack_start(*histnav);

	navbar->set_custom_title(*nav_entry);

	new_tab->set_can_focus(false);
	new_tab->set_relief(GTK_RELIEF_NONE);
	new_tab->show();
	navbar->pack_end(*new_tab);

	navbar->set_show_close_button(true);

	auto num_uris = uris.size();
	nb->set_show_tabs(num_uris > 1);

	window->set_title("volo");
	window->set_default_size(1024, 768);
	window->set_titlebar(*navbar);
	nb->set_scrollable(true);
	window->add(*nb);

	tabs.reserve(num_uris);
	for (auto& uri : uris) {
		open_new_tab(uri);
	}

	nav_entry->connect_activate(*this, on_nav_entry_activate);
	nb->connect_switch_page(*this, on_notebook_switch_page);
	nb->connect_page_added(*this, on_notebook_page_added);
	nb->connect_page_removed(*this, on_notebook_page_removed);
	new_tab->connect_clicked(*this, on_new_tab_clicked);
	window->connect_key_press_event(*this, on_window_key_press_event);
	window->connect_destroy(*this, on_window_destroy);

	show_webview(0, *tabs.front().wv);

	window->show_all();
}

void browser::on_nav_entry_activate(uri_entry& entry) {
	std::string uri = nav_entry->get_text();
	guess_uri(uri);
	visable_tab.web_view->load_uri(uri);
	visable_tab.web_view->grab_focus();
}

void browser::on_notebook_switch_page(gtk::notebook& notebook, gtk::widget& page,
	unsigned int page_num) {

	switch_page(page_num);
}

void browser::on_notebook_page_added(gtk::notebook& notebook, gtk::widget& child,
	unsigned int page_num) {

	notebook.set_show_tabs(true);
}

void browser::on_notebook_page_removed(gtk::notebook& notebook, gtk::widget& child,
	unsigned int page_num) {

	notebook.set_show_tabs(notebook.get_n_pages() > 1);
}

void browser::on_new_tab_clicked(gtk::button& button) {
	auto n = open_new_tab("");
	nb->set_current_page(n);
}

bool browser::on_window_key_press_event(gtk::window& window, GdkEventKey& ev) {
	auto kv = ev.keyval;
	auto state = ev.state;

	if (state == (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) {
		if (kv == GDK_KEY_ISO_Left_Tab) {
			auto n = nb->get_current_page();
			if (n == 0) {
				n = tabs.size() - 1;
			} else {
				--n;
			}
			switch_page(n);
			nb->set_current_page(n);
			return true;
		}
	} else if (state == GDK_CONTROL_MASK) {
		if (kv == GDK_KEY_Tab) {
			auto n = nb->get_current_page();
			if (++n == tabs.size()) {
				n = 0;
			}
			switch_page(n);
			nb->set_current_page(n);
			return true;
		}
	}

	// Let the window begin handling the event.  This is done before some
	// of the Ctrl keybindings below to allow various events which modify
	// text fields (such as ^W to delete the preivous word) to be handled
	// by the child, rather than closing the current tab.
	if (window.key_press_event(ev)) {
		return true;
	}

	if (state == GDK_CONTROL_MASK) {
		if (kv == GDK_KEY_l) {
			nav_entry->grab_focus();
			return true;
		} else if (kv == GDK_KEY_t) {
			auto n = open_new_tab("");
			nb->set_current_page(n);
			return true;
		} else if (kv == GDK_KEY_w) {
			tabs.erase(tabs.begin() + visable_tab.tab_index);
			if (tabs.size() == 0) {
				window.destroy();
			}
			return true;
		} else if (kv == GDK_KEY_q) {
			tabs.clear();
			window.destroy();
			return true;
		} else if (kv >= GDK_KEY_1 && kv <= GDK_KEY_8) {
			auto n = kv - GDK_KEY_1;
			if (tabs.size() > n) {
				switch_page(n);
				nb->set_current_page(n);
			}
			return true;
		} else if (kv == GDK_KEY_9) {
			auto n = tabs.size() - 1;
			switch_page(n);
			nb->set_current_page(n);
			return true;
		}
	}

	return true;
}

void browser::on_window_destroy(gtk::window& w) {
	gtk_main_quit();
}

browser_tab::browser_tab(const char *uri) :
	wv{gtk::make_unique<webkit::web_view>(uri)},
	tab_title{gtk::make_unique<gtk::label>("New tab")} {

	tab_title->set_can_focus(false);
	tab_title->set_hexpand(true);
	tab_title->set_ellipsize(PANGO_ELLIPSIZE_END);
	tab_title->set_size_request(50, -1);
}

void browser::on_web_view_load_changed(webkit::web_view& wv, WebKitLoadEvent load_event) {
	GTlsCertificate *certificate = nullptr;
	GTlsCertificateFlags errors{};

	switch (load_event) {
	case WEBKIT_LOAD_STARTED:
		break;

	case WEBKIT_LOAD_REDIRECTED:
		break;

	case WEBKIT_LOAD_COMMITTED:
		if (wv.get_tls_info(certificate, errors)) {
			// TODO: Display certificate details.
		} else {
			// TODO: Clear any displayed HTTPS details.
		}
		break;

	case WEBKIT_LOAD_FINISHED:
		break;
	}
}

void browser::on_web_view_notify_title(webkit::web_view& wv, GParamSpec& param_spec) {
	auto title = wv.get_title();
	if (visable_tab.web_view == &wv) {
		window->set_title(title);
		tabs[visable_tab.tab_index].tab_title->set_text(title);
		return;
	}

	// If the notified webview is not the currently-shown tab, we must
	// search for the correct tab title to modify.
	for (auto& tab : tabs) {
		if (tab.wv.get() == &wv) {
			tab.tab_title->set_text(title);
			break;
		}
	}
}

int browser::open_new_tab(const char *uri) {
	tabs.emplace_back(uri);
	auto& tab = tabs.back();
	auto& wv = *tab.wv;

	auto tab_content = gtk::box::create();
	tab_content->set_can_focus(false);
	tab_content->add(*tab.tab_title);
	tab_content->add(*tab.tab_close);

	wv.show_all();
	tab_content->show_all();
	const auto n = nb->append_page(wv, *tab_content);
	nb->set_tab_reorderable(wv, true);

	tab.tab_close->connect_clicked(*this, on_tab_close_clicked);
	wv.connect_notify_title(*this, on_web_view_notify_title);

	return n;
}

void browser::show_window() {
	window->show();
}

void browser::on_tab_close_clicked(gtk::button& tab_close) {
	for (auto it = tabs.begin(), ite = tabs.end(); it != ite; ++it) {
		// Compare using pointer equality.
		if (it->tab_close->ptr() != &tab_close) {
			continue;
		}
		auto removed_index = std::distance(tabs.begin(), it);
		tabs.erase(it);

		if (tabs.size() == 0) {
			window->destroy();
			break;
		}

		// If the removed tab had an index smaller than the visable tab,
		// the visable tab index must be decremented.
		auto prev_index = visable_tab.tab_index;
		if (removed_index < prev_index) {
			visable_tab.tab_index = prev_index - 1;
		}

		break;
	}
}

void browser::on_back_button_clicked(gtk::button& back) {
	visable_tab.web_view->go_back();
}

void browser::on_fwd_button_clicked(gtk::button& fwd) {
	visable_tab.web_view->go_forward();
}

void browser::on_back_forward_list_changed(WebKitBackForwardList& bfl,
	WebKitBackForwardListItem&, gpointer) {

	if (visable_tab.bfl == &bfl) {
		update_histnav(*visable_tab.web_view);
	}
}

void browser::on_web_view_notify_uri(webkit::web_view& web_view, GParamSpec& param_spec) {
	nav_entry->set_uri(web_view.get_uri());
}

static void on_nav_entry_refresh_clicked(uri_entry *entry, webkit::web_view *web_view) {
	web_view->reload();
	web_view->grab_focus();
}

void browser::on_notebook_page_reordered(gtk::notebook& notebook, gtk::widget& child,
	unsigned int new_idx) {

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
}

void browser::show_webview(unsigned int page_num, webkit::web_view& wv) {
	visable_tab = {page_num, wv};

	// Update navbar/titlebar with the current state of the webview being
	// shown.
	auto c_title = webkit_web_view_get_title(wv.ptr());
	window->set_title(c_title ? c_title : "volo");
	update_histnav(wv);
	auto uri = wv.get_uri();
	nav_entry->set_uri(uri);

	page_signals = { {
		back->connect_clicked(*this, on_back_button_clicked),
		fwd->connect_clicked(*this, on_fwd_button_clicked),
		wv.connect_back_forward_list_changed(*this, on_back_forward_list_changed),
		wv.connect_notify_uri(*this, on_web_view_notify_uri),
		wv.connect_load_changed(*this, on_web_view_load_changed),
		nav_entry->connect_refresh_clicked(wv, ::on_nav_entry_refresh_clicked),
		nb->connect_page_reordered(*this, on_notebook_page_reordered),
	} };

	// Grab URI entry focus if the shown tab is blank.
	//
	// TODO: If this webview is being shown by clicking another notebook
	// tab, grabbing the entry focus has no effect.
	if (!strcmp(uri, "")) {
		nav_entry->grab_focus();
	} else {
		wv.grab_focus();
	}
}

void browser::update_histnav(webkit::web_view& wv) {
	back->set_sensitive(wv.can_go_back());
	fwd->set_sensitive(wv.can_go_forward());
}

void browser::switch_page(unsigned int page_num) {
	// Disconnect previous web_view's signals before showing and connecting
	// the new web_view.
	for (auto& sig : page_signals) {
		sig.disconnect();
	}

	show_webview(page_num, *tabs.at(page_num).wv);
}


int main(int argc, char **argv) {
	gtk_init(&argc, &argv);

	auto web_cxt = webkit::web_context::get_default();
	web_cxt->set_process_model(WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES);

	auto b = browser{};
	b.show_window();
	gtk_main();
}
