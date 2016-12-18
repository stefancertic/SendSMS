/*
 * Compile me with:
 * gcc -o gsendsms gsendsms.c $(pkg-config --cflags --libs gtk+-3.0 ) -export-dynamic
 */

#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>
#include "sendsms.h"
#include "../src/encoder.h"

#ifdef DEBUG
#define debug(x, y ...) fprintf(stderr, ## y)
#else
#define debug(x, y ...) do {} while(0)
#endif

static guint sb_cont;

static struct sendsms_cfg config;


static void free_config(struct sendsms_cfg *cfg)
{
    if (cfg->username) {
        free(cfg->username);
        cfg->username = NULL;
    }
    if (cfg->password) {
        free(cfg->password);
        cfg->password = NULL;
    }
    if (cfg->sourceaddr) {
        free(cfg->sourceaddr);
        cfg->sourceaddr = NULL;
    }

}

/* Load config from the cli  */
static int load_config(struct sendsms_cfg *cfg)
{
    GError *error = NULL;
    gchar command[PATH_MAX];
    gchar *output = NULL;


    free_config(cfg);
    snprintf(command, sizeof(command), "sendsms -e");

    g_spawn_command_line_sync (command, &output, NULL, NULL, &error);

    if (error != NULL)
    {
            GtkWidget *dialog;

            /* create an error dialog containing the error message */
            dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK,
                                            "%s", error->message);
            gtk_window_set_title (GTK_WINDOW (dialog), "Error");

            /* run the dialog */
            gtk_dialog_run (GTK_DIALOG (dialog));

            /* clean up memory used by dialog and error */
            gtk_widget_destroy (dialog);
            g_error_free(error);

            return -1;
    }

    if (output) {
        char *strp = output;
        char *cp;
        char *dp;
        char var[255];
        char val[255];
        debug(1, "output:[%s]", output);
        while (NULL != (cp = strsep(&strp, "\n\r"))) {
            var[0] = '\0';
            val[0] = '\0';
            sscanf(cp,"%[^ =] = %[^\r\n]", var, val);

            if (!strcasecmp(var, "username")) {
                cfg->username = strdup(val);
            } else if (!strcasecmp(var, "password")) {
                cfg->password = strdup(val);
            }
            else if (!strcasecmp(var, "sourceaddr")) {
                dp = val;
                if (*dp == '+')
                    dp++;
                cfg->sourceaddr = strdup(dp);
            }
        }
        g_free(output);
    }

    return cfg->username ? 0 : -1;
}

static int save_config(struct sendsms_cfg *cfg)
{
    GError *error = NULL;
    gchar *output = NULL;
    gchar command[PATH_MAX];
    int sz;
    sz = snprintf(command, sizeof(command), "sendsms -w");
    if (cfg->username) {
        sz += snprintf(command + sz, sizeof(command) - sz, " -u \"%s\"", cfg->username);
    }
    if (cfg->password) {
        sz += snprintf(command + sz, sizeof(command) - sz, " -p \"%s\"", cfg->password);
    }
    if (cfg->sourceaddr) {
        sz += snprintf(command + sz, sizeof(command) - sz, " -s \"%s\"", cfg->sourceaddr);
    }
    g_spawn_command_line_sync (command, &output, NULL, NULL, &error);

    if (error != NULL)
    {
            GtkWidget *dialog;

            /* create an error dialog containing the error message */
            dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK,
                                            "%s", error->message);
            gtk_window_set_title (GTK_WINDOW (dialog), "Error");

            /* run the dialog */
            gtk_dialog_run (GTK_DIALOG (dialog));

            /* clean up memory used by dialog and error */
            gtk_widget_destroy (dialog);
            g_error_free(error);

            return -1;
    }
    load_config(&config);

    return 0;
}



/* GUI Code  */

/* Config Dialog */
static GtkEntry  *ent_username;
static GtkEntry  *ent_password;
static GtkEntry  *ent_source;

/* Main View */
static GtkLabel  *chars_left;
static GtkEntry  *destaddr_entry;
static GtkTextBuffer  *msg_buffer;
static GtkStatusbar  *statusbar;
static GtkWidget *configdialog;
static GtkWidget *main_window;
static int num_chars = 160*10;

typedef struct _Data Data;
struct _Data
{
    GtkWidget *about;
    GtkWidget *config;
};

static void show_status(char *status)
{
    gtk_statusbar_pop(statusbar, sb_cont);
    gtk_statusbar_push (statusbar, sb_cont, status);
}

static void set_config_to_dialog(void)
{
    gtk_entry_set_text(ent_username, config.username ? : "");
    gtk_entry_set_text(ent_password, config.password ? : "");
    gtk_entry_set_text(ent_source, config.sourceaddr ? : "");
}

static void get_config_from_dialog(void)
{
    const char *tmp;
    free_config(&config);

    tmp = gtk_entry_get_text(ent_username);
    if (tmp && *tmp) {
        config.username = strdup(tmp);
    }
    tmp = gtk_entry_get_text(ent_username);
    if (tmp && *tmp) {
        config.username = strdup(tmp);
    }
    tmp = gtk_entry_get_text(ent_password);
    if (tmp && *tmp) {
        config.password = strdup(tmp);
    }
    tmp = gtk_entry_get_text(ent_source);
    if (tmp && *tmp) {
        config.sourceaddr = strdup(tmp);
    }
}


G_MODULE_EXPORT void
on_open_cb(GtkButton *button, Data *data)
{
     GtkWidget *dialog;

     dialog = gtk_file_chooser_dialog_new ("Select Bulk Numbers File",
                          GTK_WINDOW(main_window),
                          GTK_FILE_CHOOSER_ACTION_OPEN,
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                          GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                          NULL);

     if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;
        char p[PATH_MAX];
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        debug(1,"Bulkfile:[%s]\n", filename);
        show_status(filename);
        snprintf(p, sizeof(p), "file://%s", filename);
        gtk_entry_set_text(destaddr_entry, p);
        g_free (filename);
    }

     gtk_widget_destroy (dialog);
}

G_MODULE_EXPORT void
on_main_window_destroy(GtkWidget *object, gpointer user_data)
{
        gtk_main_quit();
}

G_MODULE_EXPORT void
cb_show_about( GtkButton *button,
               Data      *data )
{
    /* Run dialog */
    gtk_dialog_run( GTK_DIALOG( data->about ) );
    gtk_widget_hide( data->about );
}

G_MODULE_EXPORT void
cb_show_config( GtkButton *button,
               Data      *data )
{
    /* Run dialog */
    load_config(&config);
    set_config_to_dialog();
    gtk_dialog_run( GTK_DIALOG( data->config ) );
    gtk_widget_hide( data->config );
}

#define FILEURL "file://"

static int
send_message(const gchar *text, const gchar *destnumber)
{
    GError *error = NULL;
    gchar command[PATH_MAX];
    gchar *output = NULL;
    int furl = strlen(FILEURL);

    if (!strncmp(FILEURL, destnumber, furl)) {
        snprintf(command, sizeof(command), "sendsms -f \"%s\" \"%s\" ", destnumber+furl, text);
    } else {
        snprintf(command, sizeof(command), "sendsms -d \"%s\" \"%s\" ", destnumber, text);
    }

    debug(1,"SEND:[%s]\n", command);
    /* execute command */

    g_spawn_command_line_sync (command, &output, NULL, NULL, &error);

    if (error != NULL)
    {
            GtkWidget *dialog;

            /* create an error dialog containing the error message */
            dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK,
                                            "%s", error->message);
            gtk_window_set_title (GTK_WINDOW (dialog), "Error");

            /* run the dialog */
            gtk_dialog_run (GTK_DIALOG (dialog));

            /* clean up memory used by dialog and error */
            gtk_widget_destroy (dialog);
            g_error_free(error);

            return -1;
    }
    if (output) {
        debug(1, "output:[%s]", output);
        g_free(output);
    }
    return 0;
}

G_MODULE_EXPORT void
quit_app_cb( GtkButton *button,
            gpointer   data )
{
    gtk_main_quit();
}


G_MODULE_EXPORT void
cfg_save_button_clicked_cb( GtkButton *button, gpointer   data )
{
    debug(1, "Saving settings\n");
    show_status("Settings saved");
    get_config_from_dialog();
    save_config(&config);
}

G_MODULE_EXPORT void
cfg_cancel_button_clicked_cb( GtkButton *button, gpointer   data )
{
    // fprintf(stderr, "Cancel  settings\n");
}

G_MODULE_EXPORT void
send_clicked( GtkButton *button,
            gpointer   data )
{
    GtkTextIter start, end;
    gchar *text;
    const gchar *destnumber;
    gint rc;
    GtkWidget *dialog;

    if (!config.username) {
        /* Run dialog */
        load_config(&config);
        set_config_to_dialog();
        gtk_dialog_run( GTK_DIALOG( configdialog ) );
        gtk_widget_hide( configdialog );
        if (!config.username)
            return;
    }

    gtk_text_buffer_get_start_iter (msg_buffer, &start);
    gtk_text_buffer_get_end_iter (msg_buffer, &end);
    text = gtk_text_buffer_get_text(msg_buffer, &start, &end, TRUE);
    if (!text || !*text) {
           dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    "%s", "Can not send empty message");
            gtk_window_set_title (GTK_WINDOW (dialog), "Error");

            /* run the dialog */
            gtk_dialog_run (GTK_DIALOG (dialog));
            gtk_widget_destroy (dialog);
           return;
    }
    destnumber = gtk_entry_get_text(destaddr_entry);
    if (!destnumber || !*destnumber) {
           dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    "%s", "Please, Enter the destination number");
            gtk_window_set_title (GTK_WINDOW (dialog), "Error");

            /* run the dialog */
            gtk_dialog_run (GTK_DIALOG (dialog));
            gtk_widget_destroy (dialog);
            return;
    }
    show_status("Sending");
    fprintf(stderr, "Send request: dest:[%s] msg:[%s] len=%d\n", destnumber,text, strlen(text));
    rc = send_message(text, destnumber);
    gtk_statusbar_pop(statusbar, sb_cont);
    if (!rc) {
        show_status("Success");
        gtk_statusbar_push (statusbar, sb_cont, "Success");
    } else {
        show_status("Failed to send message");
    }
}

/*
 *   GSM default 7-bit alphabet (IA5), max 160 characters
 *   CS2 (16-bit Unicode), max 70 characters UTF-16BE
 *   Calculate message mode and aprox messages
 */

static int calc_len(char *message, int *max_chars, int *num_messages)
{
    char *ret = NULL;
    int bad = 0;
    int rc;

    *max_chars = 160;
    *num_messages = 0;

    if (!*message)
        return 0;

    /* Can add parameter to stop on first bad char */
    rc = encode_str_gsm7(message, &ret, 0, &bad);
    if (bad || rc <= 0) {
        if (ret) free(ret);
        ret = NULL;
        rc = encode_str_ucs2(message, &ret);
        if (rc < 0) {
            *max_chars = 70;
            *num_messages = 1;
        }
        /* UTF16 bytes string len /2 */
        rc /= 2;
        *max_chars = 70;
        *num_messages = 1 + ((rc-1)/70);
    } else {
        if (ret) free(ret);
        *max_chars = 160;
        *num_messages = 1 + ((rc-1)/160);
    }
    return rc;
}

static void
update_chars_label(char *text)
{
    int left = 144;
    gchar buf[512];
    int max_chars = 0;
    int num_messages = 0;
    int chars;

    chars = calc_len(text, &max_chars, &num_messages);

    // left = num_chars - strlen(text);
    left = (max_chars - chars %  max_chars);
    sprintf(buf, "%d/%d (%d messages)", left,  max_chars, num_messages);
    gtk_label_set_text(chars_left, buf);
}

void
text_changed_cb( GtkTextBuffer *buffer,
            gpointer   data )
{
    // GtkTextBuffer *buffer;
    GtkTextIter start, end;
    gchar *text;
    int remaining;

    gtk_text_buffer_get_start_iter (buffer, &start);
    gtk_text_buffer_get_end_iter (buffer, &end);
    text = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
    remaining = num_chars - strlen(text);
    if (remaining >= 0) {
        // fprintf(stderr, "%s\n", text);
        update_chars_label(text);
    } else {
        // fprintf(stderr, "Max chars reached:%d", remaining);
/*         GtkTextIter dstart;
         GtkTextIter dend;
         gtk_text_buffer_get_iter_at_offset( buffer,
                                             &dstart,
                                             num_chars );
         gtk_text_buffer_get_iter_at_offset( buffer,
                                             &dend,
                                             num_chars + 1 );
*/
        gtk_text_iter_forward_chars(&start, num_chars);
        gtk_text_buffer_delete( buffer, &start, &end );
        // gtk_text_buffer_get_start_iter (buffer, &start);
        // gtk_text_buffer_get_end_iter (buffer, &end);
        // text = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
        // update_chars_label(text);
    }

}


void
insert_text_cb(GtkTextBuffer *buffer,
            gpointer   data )
{
    GtkTextIter range_start, range_end;
    gchar *text;
    int max_chars = 160;
    gtk_text_buffer_get_start_iter (buffer, &range_start);
    gtk_text_buffer_get_end_iter (buffer, &range_end);

    text = gtk_text_buffer_get_text(buffer, &range_start, &range_end, TRUE);
    if (strlen(text) > max_chars - 1) {
        // gtk_text_buffer_get_start_iter(buffer, &range_start);
        // range_start = range_end;
        gtk_text_iter_forward_chars(&range_start, (max_chars - 1));
        gtk_text_buffer_delete(buffer, &range_start, &range_end);
        gtk_text_buffer_get_start_iter (buffer, &range_start);
        gtk_text_buffer_get_end_iter (buffer, &range_end);
        text = gtk_text_buffer_get_text(buffer, &range_start, &range_end, TRUE);
        gtk_text_buffer_place_cursor(buffer, &range_end);
        update_chars_label(text);
    }
}

#define GLADEFILE "gsendsms.glade"
#define QUOTE(str) #str
#define EQUOTE(str) QUOTE(str)

int
main( int    argc,
      char **argv )
{
    GtkBuilder *builder;
    GtkWidget  *window;
    GtkTextView  *msg_edit;
    GError     *error = NULL;
    Data        data;
    char glade[PATH_MAX];

    /* Init GTK+ */
    gtk_init( &argc, &argv );

    /* Create new GtkBuilder object */
    builder = gtk_builder_new();
#ifdef DEBUG
    if( ! gtk_builder_add_from_file( builder, "./gsendsms.glade", &error ) ) {
        g_warning( "%s", error->message );
        g_error_free( error );
        error = NULL;
#endif
        snprintf(glade, sizeof(glade), "%s/sendsms/%s", EQUOTE(DATADIR), GLADEFILE);
        if( ! gtk_builder_add_from_file( builder, glade, &error ) ) {
            g_warning( "%s", error->message );
            g_error_free( error );
            error = NULL;
            return( 1 );
        }
#ifdef DEBUG
    }
#endif

    /* Get main window pointer from UI */
    window = GTK_WIDGET( gtk_builder_get_object( builder, "main_window" ) );
    main_window = window;

    data.about = GTK_WIDGET( gtk_builder_get_object( builder, "aboutdialog" ) );
    configdialog = data.config = GTK_WIDGET( gtk_builder_get_object( builder, "configdialog" ) );

    /* Connect signals */
    gtk_builder_connect_signals( builder, &data );

    statusbar  = GTK_STATUSBAR(gtk_builder_get_object( builder, "statusbar" ));
    sb_cont = gtk_statusbar_get_context_id (statusbar, "msgq");

    msg_edit  = GTK_TEXT_VIEW(gtk_builder_get_object( builder, "message_edit" ));
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (msg_edit);
    msg_buffer = buffer;
    g_signal_connect(G_OBJECT(buffer), "changed", G_CALLBACK(text_changed_cb), NULL);
    // g_signal_connect(G_OBJECT(buffer), "insert-text", G_CALLBACK(insert_text_cb), NULL);

    chars_left = GTK_LABEL(gtk_builder_get_object( builder, "chars_count_label" ) );
    destaddr_entry = GTK_ENTRY(gtk_builder_get_object( builder, "destaddr" ) );

    ent_username = GTK_ENTRY(gtk_builder_get_object( builder, "ent_username" ) );
    ent_password = GTK_ENTRY(gtk_builder_get_object( builder, "ent_password" ) );
    ent_source = GTK_ENTRY(gtk_builder_get_object( builder, "ent_source" ) );

    /* Destroy builder, since we don't need it anymore */
    g_object_unref( G_OBJECT( builder ) );

    load_config(&config);

    /* Show window. All other widgets are automatically shown by GtkBuilder */
    gtk_widget_show( window );

    /* Start main loop */
    gtk_main();

    return( 0 );
}
