import gi
import json
import os
import time
import threading

gi.require_version('Gtk', '3.0')
gi.require_version('WebKit2', '4.0')
from gi.repository import Gtk, WebKit2, GLib, Gdk

HISTORY_FILE = os.path.expanduser('~/.local/share/surfboard-browser/history.json')

class BrowserTab(Gtk.VBox):
    def __init__(self, uri, on_close_callback, on_url_changed_callback):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.webview = WebKit2.WebView()
        self.webview.load_uri(uri)
        self.pack_start(self.webview, True, True, 0)
        self.on_close_callback = on_close_callback
        self.on_url_changed_callback = on_url_changed_callback

        # Timer for unloading inactive tabs
        self.last_active_time = time.time()
        self.inactivity_timer = None

        self.webview.connect("notify::uri", self.on_uri_changed)
        self.webview.connect("notify::title", self.on_title_changed)
        self.webview.connect("load-failed", self.on_load_failed)
        self.label = Gtk.Label(label="New Tab")
        self.start_inactivity_timer()

    def get_tab_label(self):
        box = Gtk.HBox()
        box.pack_start(self.label, True, True, 0)
        close_button = Gtk.Button.new_from_icon_name("window-close", Gtk.IconSize.BUTTON)
        close_button.set_relief(Gtk.ReliefStyle.NONE)
        close_button.connect("clicked", self.on_close_callback, self)
        box.pack_start(close_button, False, False, 0)
        box.show_all()
        return box

    def on_uri_changed(self, webview, _):
        uri = webview.get_uri()
        if uri:
            GLib.idle_add(self.on_url_changed_callback, self, uri)
            self.save_to_history(uri)

    def on_title_changed(self, webview, _):
        title = webview.get_title()
        if title:
            self.label.set_text(title)

    def on_load_failed(self, webview, frame, error):
        print(f"Load failed: {error}")

    def start_inactivity_timer(self):
        self.reset_inactivity_timer()

    def unload_tab(self):
        if time.time() - self.last_active_time >= 1500:
            self.on_close_callback(None, self)
        else:
            self.start_inactivity_timer()

    def reset_inactivity_timer(self):
        self.last_active_time = time.time()
        if self.inactivity_timer:
            self.inactivity_timer.cancel()
        self.inactivity_timer = threading.Timer(1500, self.unload_tab)
        self.inactivity_timer.start()

    def stop_timers(self):
        if self.inactivity_timer:
            self.inactivity_timer.cancel()

    def apply_settings(self):
        settings = self.webview.get_settings()
        settings.set_property("enable-javascript", True)
        settings.set_property("enable-smooth-scrolling", True)
        settings.set_property("enable-write-console-messages-to-stdout", True)
        settings.set_property("enable-media-stream", True)
        settings.set_property("javascript-can-access-clipboard", True)
        settings.set_property("enable-developer-extras", True)
        settings.set_property("enable-page-cache", False)
        self.webview.set_settings(settings)

    def save_to_history(self, uri):
        if not os.path.exists(os.path.dirname(HISTORY_FILE)):
            os.makedirs(os.path.dirname(HISTORY_FILE))

        history = []
        if os.path.exists(HISTORY_FILE):
            with open(HISTORY_FILE, 'r') as f:
                history = json.load(f)

        history.append({"url": uri, "timestamp": time.time()})

        with open(HISTORY_FILE, 'w') as f:
            json.dump(history, f)

class Surfboard(Gtk.Window):
    def __init__(self):
        super().__init__(title="Surfboard")
        self.set_default_size(1024, 768)

        # Initialize attributes
        self.bookmarks = []
        self.settings = {
            "homepage": "https://www.google.com",
            "search_engine": "https://www.google.com/search?q="
        }

        # Create HeaderBar
        self.headerbar = Gtk.HeaderBar()
        self.headerbar.set_show_close_button(True)
        self.set_titlebar(self.headerbar)

        # Back Button
        self.back_button = Gtk.Button.new_from_icon_name("go-previous", Gtk.IconSize.BUTTON)
        self.back_button.connect("clicked", self.go_back)
        self.headerbar.pack_start(self.back_button)

        # Forward Button
        self.forward_button = Gtk.Button.new_from_icon_name("go-next", Gtk.IconSize.BUTTON)
        self.forward_button.connect("clicked", self.go_forward)
        self.headerbar.pack_start(self.forward_button)

        # Refresh Button
        self.refresh_button = Gtk.Button.new_from_icon_name("view-refresh", Gtk.IconSize.BUTTON)
        self.refresh_button.connect("clicked", self.refresh_page)
        self.headerbar.pack_start(self.refresh_button)

        # Options Button
        self.options_button = Gtk.Button.new_from_icon_name("preferences-system", Gtk.IconSize.BUTTON)
        self.options_button.connect("clicked", self.show_options_menu)
        self.headerbar.pack_end(self.options_button)

        # URL Bar
        self.url_bar = Gtk.Entry()
        self.url_bar.set_hexpand(True)  # Allow the URL bar to expand
        self.url_bar.set_property("expand", True)  # Ensure it expands to fill available space
        self.url_bar.connect("activate", self.navigate_to_url)
        self.headerbar.pack_start(self.url_bar)  # Add url_bar to the headerbar

        # New Tab Button
        self.new_tab_button = Gtk.Button.new_from_icon_name("list-add", Gtk.IconSize.BUTTON)
        self.new_tab_button.connect("clicked", self.on_new_tab_clicked)
        self.headerbar.pack_end(self.new_tab_button)

        # Notebook (Tabs)
        self.tab_view = Gtk.Notebook()
        self.tab_view.set_scrollable(True)
        self.tab_view.set_show_tabs(True)
        self.tab_view.set_tab_pos(Gtk.PositionType.TOP)
        self.tab_view.connect("switch-page", self.on_tab_switched)

        # Main Box to hold everything
        self.main_box = Gtk.VBox()
        self.main_box.pack_start(self.tab_view, True, True, 0)

        # Loading Bar
        self.loading_bar = Gtk.ProgressBar()
        self.loading_bar.set_visible(False)
        self.main_box.pack_start(self.loading_bar, False, False, 0)

        self.add(self.main_box)

        # Create Initial Tab
        self.new_tab(self.settings["homepage"])

        # Apply CSS for Chrome-like Tab Style
        self.apply_css()

        # Connect destroy event to quit the application
        self.connect("destroy", self.on_destroy)

        self.show_all()

    def apply_css(self):
        provider = Gtk.CssProvider()
        css = """
        .tab {
            padding: 8px 12px;
            border-radius: 5px 5px 0 0;
            margin-right: 2px;
            background-color: #e0e0e0;
            border: 1px solid #ccc;
            min-width: 150px;
            transition: background-color 0.2s ease;
        }
        .tab:hover {
            background-color: #d0d0d0;
        }
        """
        provider.load_from_data(css.encode('utf-8'))
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(),
            provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

    def new_tab(self, url):
        browser_tab = BrowserTab(url, self.on_tab_close, self.update_url_bar)
        browser_tab.apply_settings()  # Apply settings to each new tab
        tab_label = browser_tab.get_tab_label()
        self.tab_view.append_page(browser_tab, tab_label)
        self.tab_view.show_all()
        self.tab_view.set_current_page(-1)  # Set focus to the newly created tab
        self.update_url_bar(browser_tab, url)

    def on_tab_close(self, button, tab):
        page_num = self.tab_view.page_num(tab)
        if page_num != -1:
            self.tab_view.remove_page(page_num)
            tab.stop_timers()  # Stop any running timers
            tab.webview.destroy()  # Ensure WebView resources are cleaned up
            if self.tab_view.get_n_pages() == 0:
                self.on_destroy(None)

    def on_new_tab_clicked(self, button):
        self.new_tab(self.settings["homepage"])

    def update_url_bar(self, tab, uri):
        if self.tab_view.get_n_pages() > 0:
            current_page = self.tab_view.get_current_page()
            current_tab = self.tab_view.get_nth_page(current_page)
            if current_tab and hasattr(current_tab, 'webview'):
                self.url_bar.set_text(uri)

    def navigate_to_url(self, entry):
        url = entry.get_text()
        if not url.startswith("http"):
            url = self.settings["search_engine"] + url
        self.update_url(url)

    def go_back(self, button):
        current_page = self.tab_view.get_current_page()
        current_tab = self.tab_view.get_nth_page(current_page)
        if current_tab and hasattr(current_tab, 'webview'):
            current_tab.webview.go_back()

    def go_forward(self, button):
        current_page = self.tab_view.get_current_page()
        current_tab = self.tab_view.get_nth_page(current_page)
        if current_tab and hasattr(current_tab, 'webview'):
            current_tab.webview.go_forward()

    def refresh_page(self, button):
        current_page = self.tab_view.get_current_page()
        current_tab = self.tab_view.get_nth_page(current_page)
        if current_tab and hasattr(current_tab, 'webview'):
            current_tab.webview.reload()

    def show_options_menu(self, button):
        # Placeholder for options menu functionality
        print("Options menu not implemented.")

    def on_tab_switched(self, notebook, current_page, previous_page):
        if current_page != -1:  # Ensure current_page is valid
            current_tab = notebook.get_nth_page(current_page)  # Get the current tab directly using the index
            if current_tab and hasattr(current_tab, 'webview'):
                self.url_bar.set_text(current_tab.webview.get_uri())

    def on_destroy(self, widget):
        self.destroy()

if __name__ == "__main__":
    app = Surfboard()
    Gtk.main()
