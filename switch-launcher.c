#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <errno.h>

/* These symbols are supplied by the binary object files made in the Makefile. */
extern const unsigned char _binary_fusee_nano_start[], _binary_fusee_nano_end[];
extern const unsigned char _binary_hekate_bin_start[], _binary_hekate_bin_end[];
extern const unsigned char _binary_icons_switch_launcher_svg_start[], _binary_icons_switch_launcher_svg_end[];

typedef struct {
    GtkWidget *window;
    GtkWidget *send_button;
    GtkWidget *status;
    gchar *work_dir;
} App;

static void set_status(App *app, const gchar *text) {
    gtk_label_set_text(GTK_LABEL(app->status), text);
}

static gboolean write_embedded(const gchar *path, const unsigned char *start,
                               const unsigned char *end, gboolean executable,
                               GError **error) {
    if (!g_file_set_contents(path, (const gchar *)start, (gssize)(end - start), error))
        return FALSE;
    if (executable && g_chmod(path, 0700) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Could not mark %s executable: %s", path, g_strerror(errno));
        return FALSE;
    }
    return TRUE;
}

static void launcher_finished(GObject *source, GAsyncResult *result, gpointer user_data) {
    App *app = user_data;
    GError *error = NULL;
    gchar *standard_output = NULL;
    gchar *standard_error = NULL;
    gboolean completed = g_subprocess_communicate_utf8_finish(
        G_SUBPROCESS(source), result, &standard_output, &standard_error, &error);
    gtk_widget_set_sensitive(app->send_button, TRUE);
    if (completed && g_subprocess_get_successful(G_SUBPROCESS(source))) {
        set_status(app, "Payload sent. Your Switch should now boot into Hekate.");
    } else {
        if (standard_error != NULL)
            g_strstrip(standard_error);
        const gchar *detail = standard_error != NULL && *standard_error != '\0'
            ? standard_error
            : (error ? error->message : "Unknown error");
        gchar *message = g_strdup_printf("Payload was not sent:\n%s", detail);
        set_status(app, message);
        g_free(message);
        g_clear_error(&error);
    }
    g_free(standard_output);
    g_free(standard_error);
}

static void send_payload(GtkButton *button, gpointer user_data) {
    App *app = user_data;
    GError *error = NULL;
    gchar *launcher = g_build_filename(app->work_dir, "fusee-nano", NULL);
    gchar *payload = g_build_filename(app->work_dir, "hekate.bin", NULL);

    if (!write_embedded(launcher, _binary_fusee_nano_start, _binary_fusee_nano_end, TRUE, &error) ||
        !write_embedded(payload, _binary_hekate_bin_start, _binary_hekate_bin_end, FALSE, &error)) {
        gchar *message = g_strdup_printf("Could not prepare the payload: %s", error->message);
        set_status(app, message);
        g_free(message);
        g_clear_error(&error);
        g_free(launcher);
        g_free(payload);
        return;
    }

    const gchar *command[] = { "pkexec", launcher, payload, NULL };
    GSubprocess *process = g_subprocess_newv(command, G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);
    if (process == NULL) {
        gchar *message = g_strdup_printf("Could not start the launcher: %s", error->message);
        set_status(app, message);
        g_free(message);
        g_clear_error(&error);
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
        set_status(app, "Waiting for permission and sending the payload…");
        g_subprocess_communicate_utf8_async(process, NULL, NULL, launcher_finished, app);
        g_object_unref(process);
    }
    g_free(launcher);
    g_free(payload);
}

static GdkPixbuf *embedded_icon(void) {
    GInputStream *stream = g_memory_input_stream_new_from_data(
        _binary_icons_switch_launcher_svg_start,
        (gssize)(_binary_icons_switch_launcher_svg_end - _binary_icons_switch_launcher_svg_start), NULL);
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_stream_at_scale(stream, 96, 96, TRUE, NULL, &error);
    g_clear_error(&error);
    g_object_unref(stream);
    return pixbuf;
}

static void activate(GtkApplication *gtk_app, gpointer user_data) {
    App *app = user_data;
    app->window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->window), "Switch Payload Launcher");
    gtk_window_set_default_size(GTK_WINDOW(app->window), 580, 500);
    gtk_window_set_resizable(GTK_WINDOW(app->window), FALSE);
    GdkPixbuf *icon = embedded_icon();
    if (icon != NULL) gtk_window_set_icon(GTK_WINDOW(app->window), icon);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_top(box, 28);
    gtk_widget_set_margin_bottom(box, 28);
    gtk_widget_set_margin_start(box, 32);
    gtk_widget_set_margin_end(box, 32);
    gtk_container_add(GTK_CONTAINER(app->window), box);

    GtkWidget *image = icon ? gtk_image_new_from_pixbuf(icon) : gtk_image_new();
    gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);
    if (icon) g_object_unref(icon);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='x-large' weight='bold'>Switch Payload Launcher</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    GtkWidget *instructions = gtk_label_new("Connect your Nintendo Switch in RCM mode, then send Hekate.");
    gtk_label_set_line_wrap(GTK_LABEL(instructions), TRUE);
    gtk_label_set_justify(GTK_LABEL(instructions), GTK_JUSTIFY_CENTER);
    gtk_widget_set_halign(instructions, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), instructions, FALSE, FALSE, 2);

    GtkWidget *rcm_instructions = gtk_label_new(
        "To enter Recovery Mode (RCM), power off the Nintendo console completely, "
        "insert the RCM jig into the right Joy-Con rail. Then hold the Volume Up "
        "button, and press the Power button once.");
    gtk_label_set_line_wrap(GTK_LABEL(rcm_instructions), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(rcm_instructions), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_justify(GTK_LABEL(rcm_instructions), GTK_JUSTIFY_CENTER);
    gtk_widget_set_halign(rcm_instructions, GTK_ALIGN_CENTER);
    /* Keep the label's natural width from expanding the entire window. */
    gtk_widget_set_size_request(rcm_instructions, 500, -1);
    gtk_box_pack_start(GTK_BOX(box), rcm_instructions, FALSE, FALSE, 0);

    app->send_button = gtk_button_new_with_label("Send Hekate Payload");
    gtk_widget_set_margin_top(app->send_button, 10);
    gtk_widget_set_size_request(app->send_button, -1, 44);
    g_signal_connect(app->send_button, "clicked", G_CALLBACK(send_payload), app);
    gtk_box_pack_start(GTK_BOX(box), app->send_button, FALSE, FALSE, 0);

    app->status = gtk_label_new("Ready. You will be asked for administrator permission.");
    gtk_label_set_line_wrap(GTK_LABEL(app->status), TRUE);
    gtk_label_set_justify(GTK_LABEL(app->status), GTK_JUSTIFY_CENTER);
    gtk_widget_set_halign(app->status, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), app->status, FALSE, FALSE, 4);

    GtkWidget *exit_button = gtk_button_new_with_label("Exit / Close Program");
    gtk_widget_set_margin_top(exit_button, 12);
    gtk_widget_set_size_request(exit_button, -1, 38);
    g_signal_connect_swapped(exit_button, "clicked", G_CALLBACK(gtk_window_close), app->window);
    gtk_box_pack_start(GTK_BOX(box), exit_button, FALSE, FALSE, 0);

    gtk_widget_show_all(app->window);
}

int main(int argc, char **argv) {
    App app = {0};
    GError *error = NULL;
    app.work_dir = g_dir_make_tmp("switch-payload-launcher-XXXXXX", &error);
    if (app.work_dir == NULL) {
        g_printerr("Could not create a temporary working directory: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }
    GtkApplication *gtk_app = gtk_application_new("com.switchpayload.launcher", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), &app);
    int result = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    g_object_unref(gtk_app);
    gchar *launcher = g_build_filename(app.work_dir, "fusee-nano", NULL);
    gchar *payload = g_build_filename(app.work_dir, "hekate.bin", NULL);
    g_remove(launcher); g_remove(payload); g_rmdir(app.work_dir);
    g_free(launcher); g_free(payload); g_free(app.work_dir);
    return result;
}
