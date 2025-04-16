#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

/* Browser structure with essential components */
typedef struct {
    GtkWidget *window;
    WebKitWebView *web_view;
    GtkEntry *url_entry;
    GtkWidget *header_bar;
    
    // Download manager components
    GtkWidget *downloads_window;
    GtkListStore *downloads_store;
    GtkWidget *downloads_view;
    
    // History components
    GtkWidget *history_popover;
    GtkWidget *history_list;
    GList *history_items;
    
    // Site settings components
    GHashTable *site_settings;
    
    // Zoom level tracking
    gdouble zoom_level;
} Browser;

/* History item structure */
typedef struct {
    gchar *title;
    gchar *url;
    time_t timestamp;
} HistoryItem;

/* Site settings structure */
typedef struct {
    gboolean enable_javascript;
    gboolean enable_cookies;
    gboolean block_popups;
    gboolean block_images;
} SiteSettings;

/* Function declarations */
static void configure_browser(Browser *browser);
static void load_url(Browser *browser, const gchar *url);
static void setup_ui(Browser *browser);
static void setup_signals(Browser *browser);
static void setup_keyboard_shortcuts(Browser *browser);
static void setup_downloads_manager(Browser *browser);
static void setup_history(Browser *browser);
static void setup_privacy_features(Browser *browser);
static void apply_global_css(void);
static GtkWidget* create_navbar_button(const gchar *icon_name, const gchar *tooltip);

/* Load URLs with proper handling and formatting */
static void load_url(Browser *browser, const gchar *url) {
    if (!url || !*url) return;
    
    gchar *actual_url = NULL;
    if (!g_str_has_prefix(url, "http://") && !g_str_has_prefix(url, "https://")) {
        actual_url = g_strdup_printf("https://%s", url);
    } else {
        actual_url = g_strdup(url);
    }
    
    // Apply site-specific settings before loading
    gchar *domain = g_strdup(actual_url);
    // Apply site settings here when implemented
    
    webkit_web_view_load_uri(browser->web_view, actual_url);
    g_free(actual_url);
    g_free(domain);
}

/* Navigation callbacks */
static void on_url_activate(GtkEntry *entry, gpointer user_data) {
    Browser *browser = (Browser *)user_data;
    const gchar *url = gtk_entry_get_text(entry);
    load_url(browser, url);
}

static void on_back_clicked(GtkButton *button, gpointer user_data) {
    webkit_web_view_go_back(((Browser *)user_data)->web_view);
}

static void on_forward_clicked(GtkButton *button, gpointer user_data) {
    webkit_web_view_go_forward(((Browser *)user_data)->web_view);
}

static void on_reload_clicked(GtkButton *button, gpointer user_data) {
    webkit_web_view_reload(((Browser *)user_data)->web_view);
}

static void on_home_clicked(GtkButton *button, gpointer user_data) {
    load_url((Browser *)user_data, "https://lite.duckduckgo.com");
}

/* WebView load event handlers */
static void on_load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event, gpointer user_data) {
    Browser *browser = (Browser *)user_data;
    
    // Update URL bar when navigating
    if (load_event == WEBKIT_LOAD_STARTED || 
        load_event == WEBKIT_LOAD_REDIRECTED || 
        load_event == WEBKIT_LOAD_COMMITTED) {
        
        const gchar *uri = webkit_web_view_get_uri(web_view);
        if (uri) {
            gtk_entry_set_text(browser->url_entry, uri);
        }
    }
    
    // Add to history when page fully loads
    if (load_event == WEBKIT_LOAD_FINISHED) {
        const gchar *uri = webkit_web_view_get_uri(web_view);
        const gchar *title = webkit_web_view_get_title(web_view);
        
        if (uri && title) {
            // Add to history (implementation would go here)
            // add_to_history(browser, title, uri);
        }
    }
}

/* Update window title when page title changes */
static void on_title_changed(WebKitWebView *web_view, GParamSpec *pspec, gpointer user_data) {
    Browser *browser = (Browser *)user_data;
    const gchar *title = webkit_web_view_get_title(web_view);
    
    if (title && *title) {
        gtk_header_bar_set_title(GTK_HEADER_BAR(browser->header_bar), title);
    } else {
        gtk_header_bar_set_title(GTK_HEADER_BAR(browser->header_bar), "Surfboard");
    }
}

/* Show loading progress in URL bar */
static void on_estimated_load_progress_changed(WebKitWebView *web_view, GParamSpec *pspec, gpointer user_data) {
    Browser *browser = (Browser *)user_data;
    gdouble progress = webkit_web_view_get_estimated_load_progress(web_view);
    
    // Update URL bar with visual progress indicator
    GtkCssProvider *provider = gtk_css_provider_new();
    
    if (progress < 1.0) {
        gchar *css = g_strdup_printf(
            "entry { background: linear-gradient(to right, "
            "alpha(@theme_selected_bg_color, 0.3) 0%%, "
            "alpha(@theme_selected_bg_color, 0.3) %.0f%%, "
            "alpha(@theme_bg_color, 0.1) %.0f%%, "
            "alpha(@theme_bg_color, 0.1) 100%%); "
            "border-radius: 15px; padding: 3px 10px; }",
            progress * 100, progress * 100);
        
        gtk_css_provider_load_from_data(provider, css, -1, NULL);
        g_free(css);
    } else {
        gtk_css_provider_load_from_data(provider, 
            "entry { background: alpha(@theme_bg_color, 0.1); "
            "border-radius: 15px; padding: 3px 10px; }", -1, NULL);
    }
    
    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(browser->url_entry));
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), 
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    g_object_unref(provider);
}

/* Memory management functions */
static gboolean reduce_memory_usage(gpointer user_data) {
    Browser *browser = (Browser *)user_data;
    
    // Request garbage collection via JavaScript
    webkit_web_view_evaluate_javascript(browser->web_view, 
                                      "if (window.gc) { window.gc(); }", 
                                      -1, NULL, NULL, NULL, NULL, NULL);
    
    // Clear WebKit cache
    webkit_web_context_clear_cache(webkit_web_view_get_context(browser->web_view));
    
    return G_SOURCE_CONTINUE; // Keep timer running
}

/* Apply global CSS styling */
static void apply_global_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        /* Modern, flat UI style */
        "headerbar {"
        "  border-bottom: none;"
        "  box-shadow: none;"
        "  padding: 4px;"
        "}"
        "headerbar button {"
        "  margin: 0;"
        "  padding: 8px 8px;"
        "  border-radius: 4px;"
        "  border: none;"
        "  background: none;"
        "  box-shadow: none;"
        "  transition: background 0.2s ease;"
        "}"
        "headerbar button:hover {"
        "  background: alpha(@theme_selected_bg_color, 0.15);"
        "}"
        "headerbar entry {"
        "  border-radius: 15px;"
        "  background: alpha(@theme_bg_color, 0.4);"
        "  border: none;"
        "  min-height: 24px;"
        "  padding: 3px 10px;"
        "  margin: 5px 10px;"
        "}"
        /* Clean list styling */
        "list {"
        "  background: @theme_bg_color;"
        "}"
        "list row {"
        "  padding: 8px 5px;"
        "  transition: background 0.2s ease;"
        "}"
        "list row:hover {"
        "  background: alpha(@theme_selected_bg_color, 0.1);"
        "}"
        "list row:selected {"
        "  background: @theme_selected_bg_color;"
        "}"
        /* Subtle progress bar */
        "progressbar trough {"
        "  background: alpha(@theme_bg_color, 0.2);"
        "  border: none;"
        "  border-radius: 3px;"
        "  min-height: 6px;"
        "}"
        "progressbar progress {"
        "  background: @theme_selected_bg_color;"
        "  border: none;"
        "  border-radius: 3px;"
        "}"
        /* Clean window background */
        "window {"
        "  background: @theme_bg_color;"
        "}",
        -1, NULL);
        
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    g_object_unref(provider);
}

/* Create minimal navbar button */
static GtkWidget* create_navbar_button(const gchar *icon_name, const gchar *tooltip) {
    GtkWidget *button = gtk_button_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
    gtk_widget_set_tooltip_text(button, tooltip);
    
    // Flat button styling
    GtkStyleContext *context = gtk_widget_get_style_context(button);
    gtk_style_context_add_class(context, "flat");
    
    return button;
}

/* Download management functions */
static void setup_downloads_manager(Browser *browser) {
    // Create downloads window
    browser->downloads_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(browser->downloads_window), "Downloads");
    gtk_window_set_default_size(GTK_WINDOW(browser->downloads_window), 500, 300);
    gtk_window_set_transient_for(GTK_WINDOW(browser->downloads_window), GTK_WINDOW(browser->window));
    
    // Create list store for downloads
    browser->downloads_store = gtk_list_store_new(4, 
                                               G_TYPE_STRING,   // Filename
                                               G_TYPE_DOUBLE,   // Progress
                                               G_TYPE_STRING,   // Status
                                               G_TYPE_POINTER); // Download object
    
    // Create tree view
    browser->downloads_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(browser->downloads_store));
    
    // Add columns
    GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *filename_column = gtk_tree_view_column_new_with_attributes(
        "Filename", text_renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->downloads_view), filename_column);
    
    GtkCellRenderer *progress_renderer = gtk_cell_renderer_progress_new();
    GtkTreeViewColumn *progress_column = gtk_tree_view_column_new_with_attributes(
        "Progress", progress_renderer, "value", 1, NULL);
    gtk_tree_view_column_set_min_width(progress_column, 150);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->downloads_view), progress_column);
    
    GtkCellRenderer *status_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *status_column = gtk_tree_view_column_new_with_attributes(
        "Status", status_renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(browser->downloads_view), status_column);
    
    // Add tree view to scrolled window
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled_window), browser->downloads_view);
    
    // Add clear button
    GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(button_box), 5);
    
    GtkWidget *clear_button = gtk_button_new_with_label("Clear Completed");
    // Connect clear button signal here
    gtk_container_add(GTK_CONTAINER(button_box), clear_button);
    
    // Layout everything
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(browser->downloads_window), vbox);
    
    // Add download button to header bar
    GtkWidget *downloads_button = create_navbar_button("document-save-symbolic", "Downloads");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(browser->header_bar), downloads_button);
}

/* Placeholder for setup_history function */
static void setup_history(Browser *browser) {
    // TODO: Implement history functionality
    browser->history_items = NULL;
    
    // Create history popover and list
    browser->history_popover = gtk_popover_new(NULL);
    browser->history_list = gtk_list_box_new();
    
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled_window), 300);
    gtk_container_add(GTK_CONTAINER(scrolled_window), browser->history_list);
    gtk_container_add(GTK_CONTAINER(browser->history_popover), scrolled_window);
    
    // Add history button to header bar
    GtkWidget *history_button = create_navbar_button("document-open-recent-symbolic", "History");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(browser->header_bar), history_button);
    
    // Connect the history button to the popover
    gtk_popover_set_relative_to(GTK_POPOVER(browser->history_popover), history_button);
    g_signal_connect_swapped(history_button, "clicked", G_CALLBACK(gtk_widget_show_all), browser->history_popover);
}

/* Placeholder for setup_privacy_features function */
static void setup_privacy_features(Browser *browser) {
    // TODO: Implement privacy features
    browser->site_settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
}

/* Setup the browser UI */
static void setup_ui(Browser *browser) {
    // Main window
    browser->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(browser->window), 1024, 768);
    
    // Header bar
    browser->header_bar = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(browser->header_bar), "Surfboard");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(browser->header_bar), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(browser->window), browser->header_bar);
    
    // Navigation buttons
    GtkWidget *back_button = create_navbar_button("go-previous-symbolic", "Back");
    GtkWidget *forward_button = create_navbar_button("go-next-symbolic", "Forward");
    GtkWidget *reload_button = create_navbar_button("view-refresh-symbolic", "Reload");
    GtkWidget *home_button = create_navbar_button("go-home-symbolic", "Home");
    
    g_signal_connect(back_button, "clicked", G_CALLBACK(on_back_clicked), browser);
    g_signal_connect(forward_button, "clicked", G_CALLBACK(on_forward_clicked), browser);
    g_signal_connect(reload_button, "clicked", G_CALLBACK(on_reload_clicked), browser);
    g_signal_connect(home_button, "clicked", G_CALLBACK(on_home_clicked), browser);
    
    // URL entry
    browser->url_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(browser->url_entry, "Enter URL or search term");
    g_signal_connect(browser->url_entry, "activate", G_CALLBACK(on_url_activate), browser);
    
    // Pack navigation buttons
    gtk_header_bar_pack_start(GTK_HEADER_BAR(browser->header_bar), back_button);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(browser->header_bar), forward_button);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(browser->header_bar), reload_button);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(browser->header_bar), home_button);
    
    // Create custom container for URL entry
    GtkWidget *url_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(GTK_WIDGET(browser->url_entry), TRUE);
    gtk_container_add(GTK_CONTAINER(url_container), GTK_WIDGET(browser->url_entry));
    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(browser->header_bar), url_container);
    
    // Web view
    browser->web_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    
    // Container for web view
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(browser->web_view));
    gtk_container_add(GTK_CONTAINER(browser->window), scrolled_window);
    
    // Show all widgets
    gtk_widget_show_all(browser->window);
}

/* Configure browser settings */
static void configure_browser(Browser *browser) {
    WebKitSettings *settings = webkit_web_view_get_settings(browser->web_view);
    
    // Basic settings
    webkit_settings_set_enable_javascript(settings, TRUE);
    webkit_settings_set_auto_load_images(settings, TRUE);
    // Removed deprecated plugin setting
    
    // Security settings
    webkit_settings_set_enable_webaudio(settings, FALSE);
    webkit_settings_set_enable_webgl(settings, FALSE);
    webkit_settings_set_hardware_acceleration_policy(settings, 
                                                  WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER);
    
    // Privacy settings
    webkit_settings_set_enable_media_stream(settings, FALSE);
    webkit_settings_set_enable_mediasource(settings, FALSE);
    webkit_settings_set_enable_site_specific_quirks(settings, FALSE);
    
    // Content settings
    webkit_settings_set_default_font_family(settings, "Noto Sans");
    webkit_settings_set_default_font_size(settings, 16);
    webkit_settings_set_monospace_font_family(settings, "Noto Mono");
    
    // Memory management timer
    g_timeout_add_seconds(60, reduce_memory_usage, browser);
}

/* Setup signals and event handlers */
static void setup_signals(Browser *browser) {
    // Window close signal
    g_signal_connect(browser->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // WebView signals
    g_signal_connect(browser->web_view, "load-changed", 
                   G_CALLBACK(on_load_changed), browser);
    g_signal_connect(browser->web_view, "notify::title", 
                   G_CALLBACK(on_title_changed), browser);
    g_signal_connect(browser->web_view, "notify::estimated-load-progress", 
                   G_CALLBACK(on_estimated_load_progress_changed), browser);
}

/* Setup keyboard shortcuts */
static void setup_keyboard_shortcuts(Browser *browser) {
    GtkAccelGroup *accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(browser->window), accel_group);
    
    // Ctrl+L to focus URL bar
    gtk_widget_add_accelerator(GTK_WIDGET(browser->url_entry), "grab-focus", 
                             accel_group, GDK_KEY_l, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    
    // Ctrl+R to reload
    gtk_accel_group_connect(accel_group, GDK_KEY_r, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE,
                          g_cclosure_new_swap(G_CALLBACK(on_reload_clicked), browser, NULL));
}

/* Main function */
int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    // Create browser instance
    Browser browser = {0};
    
    // Initialize browser UI
    setup_ui(&browser);
    
    // Configure browser settings
    configure_browser(&browser);
    
    // Setup signals
    setup_signals(&browser);
    
    // Setup keyboard shortcuts
    setup_keyboard_shortcuts(&browser);
    
    // Setup downloads manager
    setup_downloads_manager(&browser);
    
    // Setup history and privacy features (previously just declared)
    setup_history(&browser);
    setup_privacy_features(&browser);
    
    // Apply global CSS
    apply_global_css();
    
    // Load homepage
    load_url(&browser, "https://lite.duckduckgo.com");
    
    // Main loop
    gtk_main();
    
    return 0;
}
