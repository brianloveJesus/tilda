/*
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <tilda-config.h>

#include <debug.h>
#include <tilda.h>
#include <callback_func.h>
#include <configsys.h>
#include <tilda_window.h>
#include <tilda_terminal.h>
#include <key_grabber.h>
#include <translation.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib-object.h>
#include <vte/vte.h>

static void
initialize_alpha_mode (tilda_window *tw)
{
    GdkScreen *screen;
    GdkColormap *colormap;

    screen = gtk_widget_get_screen (GTK_WIDGET (tw->window));
    colormap = gdk_screen_get_rgba_colormap (screen);
    if (colormap != NULL && gdk_screen_is_composited (screen))
	{
		/* Set RGBA colormap if possible so VTE can use real alpha
		 * channels for transparency. */

		gtk_widget_set_colormap(GTK_WIDGET (tw->window), colormap);
		tw->have_argb_visual = TRUE;
	}
	else
	{
		tw->have_argb_visual = FALSE;
	}
}

/**
 * Get a pointer to the config file name for this instance.
 *
 * NOTE: you MUST call free() on the returned value!!!
 *
 * @param tw the tilda_window structure corresponding to this instance
 * @return a pointer to a string representation of the config file's name
 */
static gchar* get_config_file_name (tilda_window *tw)
{
    DEBUG_FUNCTION ("get_config_file_name");
    DEBUG_ASSERT (tw != NULL);

    gchar *config_file;
    gchar instance_str[6];
    const gchar config_prefix[] = "/.tilda/config_";
    gint config_file_size = 0;

    /* Get a string form of the instance */
    g_snprintf (instance_str, sizeof(instance_str), "%d", tw->instance);

    /* Calculate the config_file variable's size */
    config_file_size = strlen (tw->home_dir) + strlen (config_prefix) + strlen (instance_str) + 1;

    /* Allocate the config_file variable */
    if ((config_file = (gchar*) malloc (config_file_size * sizeof(gchar))) == NULL)
        print_and_exit (_("Out of memory, exiting ..."), EXIT_FAILURE);

    /* Store the config file name in the allocated space */
    g_snprintf (config_file, config_file_size, "%s%s%s", tw->home_dir, config_prefix, instance_str);

    return config_file;
}

/**
 * Gets the tw->instance number.
 * Sets tw->config_file.
 * Starts up the config system.
 *
 * @param tw the tilda_window in which to store the config
 */
void init_tilda_window_instance (tilda_window *tw)
{
    DEBUG_FUNCTION ("init_tilda_window_instance");
    DEBUG_ASSERT (tw != NULL);

    /* Get the instance number for this tilda, and store it in tw->instance.
     * Also create the lock file for this instance. */
    getinstance (tw);

    /* Get and store the config file's name */
    tw->config_file = get_config_file_name (tw);

    /* Start up the configuration system */
    config_init (tw->config_file);
}

void add_tab (tilda_window *tw)
{
    DEBUG_FUNCTION ("add_tab");
    DEBUG_ASSERT (tw != NULL);

    tilda_term *tt;

    tt = (tilda_term *) malloc (sizeof (tilda_term));

    if (tt == NULL)
    {
        TILDA_PERROR ();
        fprintf (stderr, _("Out of memory, cannot create tab\n"));
        return;
    }

    init_tilda_terminal (tw, tt, FALSE);
}

void add_tab_menu_call (gpointer data, guint callback_action, GtkWidget *w)
{
    DEBUG_FUNCTION ("add_tab_menu_call");
    DEBUG_ASSERT (data != NULL);

    add_tab (((tilda_collect *) data)->tw);
}

static tilda_term* find_tt_in_g_list (tilda_window *tw, gint pos)
{
    DEBUG_FUNCTION ("find_tt_in_g_list");
    DEBUG_ASSERT (tw != NULL);
    DEBUG_ASSERT (tw->terms != NULL);

    GtkWidget *page, *current_page;
    GList *terms = g_list_first (tw->terms);
    tilda_term *result=NULL;

    current_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (tw->notebook), pos);

    do
    {
        page = ((tilda_term *) terms->data)->hbox;
        if (page == current_page)
        {
            result = terms->data;
            break;
        }
    } while ((terms = terms->next) != NULL);

    return result;
}

void close_current_tab (tilda_window *tw)
{
    DEBUG_FUNCTION ("close_current_tab");
    DEBUG_ASSERT (tw != NULL);

    gint pos;
    tilda_term *tt;

    if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (tw->notebook)) < 2)
    {
        clean_up (tw);
    }
    else
    {
        pos = gtk_notebook_get_current_page (GTK_NOTEBOOK (tw->notebook));
        if ((tt = find_tt_in_g_list (tw, pos)) != NULL)
        {
            gtk_notebook_remove_page (GTK_NOTEBOOK (tw->notebook), pos);

            if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (tw->notebook)) == 1)
                gtk_notebook_set_show_tabs (GTK_NOTEBOOK (tw->notebook), FALSE);

            tw->terms = g_list_remove (tw->terms, tt);
            free (tt);
        }
    }
}

void close_tab (gpointer data, guint callback_action, GtkWidget *w)
{
    DEBUG_FUNCTION ("close_tab");
    DEBUG_ASSERT (data != NULL);

    gint pos;
    tilda_term *tt;
    tilda_window *tw;
    tilda_collect *tc = (tilda_collect *) data;

    tw = tc->tw;
    tt = tc->tt;

    if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (tw->notebook)) < 2)
    {
        clean_up (tw);
    }
    else
    {
        pos = gtk_notebook_page_num (GTK_NOTEBOOK (tw->notebook), tt->hbox);
        gtk_notebook_remove_page (GTK_NOTEBOOK (tw->notebook), pos);

        if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (tw->notebook)) == 1)
            gtk_notebook_set_show_tabs (GTK_NOTEBOOK (tw->notebook), FALSE);

        tw->terms = g_list_remove (tw->terms, tt);
        free (tt);
    }

    free (tc);
}

gboolean init_tilda_window (tilda_window *tw, tilda_term *tt)
{
    DEBUG_FUNCTION ("init_tilda_window");
    DEBUG_ASSERT (tw != NULL);
    DEBUG_ASSERT (tt != NULL);

    GtkAccelGroup *accel_group;
    GClosure *clean, *close, *next, *prev, *add, *copy_closure, *paste_closure;
    GClosure *goto_tab_closure_1, *goto_tab_closure_2, *goto_tab_closure_3, *goto_tab_closure_4;
    GClosure *goto_tab_closure_5, *goto_tab_closure_6, *goto_tab_closure_7, *goto_tab_closure_8;
    GClosure *goto_tab_closure_9, *goto_tab_closure_10;
    GError *error;

    GdkPixbuf *window_icon;
    const gchar *window_icon_file = g_build_filename (DATADIR, "pixmaps", "tilda.png", NULL);
    gboolean ret = FALSE;

    /* Create a window to hold the scrolling shell, and hook its
     * delete event to the quit function.. */
    tw->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    initialize_alpha_mode (tw);
    gtk_container_set_resize_mode (GTK_CONTAINER(tw->window), GTK_RESIZE_IMMEDIATE);
    g_signal_connect (G_OBJECT(tw->window), "delete_event", GTK_SIGNAL_FUNC(deleted_and_quit), tw->window);

    /* Create notebook to hold all terminal widgets */
    tw->notebook = gtk_notebook_new ();
    gtk_notebook_set_homogeneous_tabs (GTK_NOTEBOOK(tw->notebook), TRUE);
    g_signal_connect (G_OBJECT(tw->window), "show", GTK_SIGNAL_FUNC(focus_term), tw->notebook);

    /* Init GList of all tilda_term structures */
    tw->terms = NULL;

    switch (config_getint ("tab_pos"))
    {
        case 0:
            gtk_notebook_set_tab_pos (GTK_NOTEBOOK (tw->notebook), GTK_POS_TOP);
            break;
        case 1:
            gtk_notebook_set_tab_pos (GTK_NOTEBOOK (tw->notebook), GTK_POS_BOTTOM);
            break;
        case 2:
            gtk_notebook_set_tab_pos (GTK_NOTEBOOK (tw->notebook), GTK_POS_LEFT);
            break;
        case 3:
            gtk_notebook_set_tab_pos (GTK_NOTEBOOK (tw->notebook), GTK_POS_RIGHT);
            break;
        default:
            DEBUG_ERROR ("Tab position");
            fprintf (stderr, _("Bad tab_pos, not changing anything...\n"));
            break;
    }

    gtk_container_add (GTK_CONTAINER(tw->window), tw->notebook);
    gtk_widget_show (tw->notebook);

    gtk_notebook_set_show_border (GTK_NOTEBOOK (tw->notebook), config_getbool("notebook_border"));

    init_tilda_terminal (tw, tt, TRUE);

    /* Create Accel Group to add key codes for quit, next, prev and new tabs */
    accel_group = gtk_accel_group_new ();
    gtk_window_add_accel_group (GTK_WINDOW (tw->window), accel_group);

    /* Exit on Ctrl-Q */
    clean = g_cclosure_new_swap ((GCallback) clean_up, tw, NULL);
    gtk_accel_group_connect (accel_group, 'q', GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE, clean);

    /* Go to Next Tab */
    next = g_cclosure_new_swap ((GCallback) next_tab, tw, NULL);
    gtk_accel_group_connect (accel_group, GDK_Page_Up, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE, next);

    /* Go to Prev Tab */
    prev = g_cclosure_new_swap ((GCallback) prev_tab, tw, NULL);
    gtk_accel_group_connect (accel_group, GDK_Page_Down, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE, prev);

    /* Go to New Tab */
    add = g_cclosure_new_swap ((GCallback) add_tab, tw, NULL);
    gtk_accel_group_connect (accel_group, 't', GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE, add);

    /* Delete Current Tab */
    close = g_cclosure_new_swap ((GCallback) close_current_tab, tw, NULL);
    gtk_accel_group_connect (accel_group, 'w', GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE, close);

    /* Goto Tab # */
    /* Know a better way? Then you do. */
    goto_tab_closure_1 = g_cclosure_new_swap ((GCallback) goto_tab_1, tw, NULL);
    gtk_accel_group_connect (accel_group, '1', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE, goto_tab_closure_1);

    goto_tab_closure_2 = g_cclosure_new_swap ((GCallback) goto_tab_2, tw, NULL);
    gtk_accel_group_connect (accel_group, '2', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE, goto_tab_closure_2);

    goto_tab_closure_3 = g_cclosure_new_swap ((GCallback) goto_tab_3, tw, NULL);
    gtk_accel_group_connect (accel_group, '3', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE, goto_tab_closure_3);

    goto_tab_closure_4 = g_cclosure_new_swap ((GCallback) goto_tab_4, tw, NULL);
    gtk_accel_group_connect (accel_group, '4', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE, goto_tab_closure_4);

    goto_tab_closure_5 = g_cclosure_new_swap ((GCallback) goto_tab_5, tw, NULL);
    gtk_accel_group_connect (accel_group, '5', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE, goto_tab_closure_5);

    goto_tab_closure_6 = g_cclosure_new_swap ((GCallback) goto_tab_6, tw, NULL);
    gtk_accel_group_connect (accel_group, '6', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE, goto_tab_closure_6);

    goto_tab_closure_7 = g_cclosure_new_swap ((GCallback) goto_tab_7, tw, NULL);
    gtk_accel_group_connect (accel_group, '7', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE, goto_tab_closure_7);

    goto_tab_closure_8 = g_cclosure_new_swap ((GCallback) goto_tab_8, tw, NULL);
    gtk_accel_group_connect (accel_group, '8', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE, goto_tab_closure_8);

    goto_tab_closure_9 = g_cclosure_new_swap ((GCallback) goto_tab_9, tw, NULL);
    gtk_accel_group_connect (accel_group, '9', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE, goto_tab_closure_9);

    goto_tab_closure_10 = g_cclosure_new_swap ((GCallback) goto_tab_10, tw, NULL);
    gtk_accel_group_connect (accel_group, '0', GDK_MOD1_MASK, GTK_ACCEL_VISIBLE, goto_tab_closure_10);

    copy_closure = g_cclosure_new_swap ((GCallback) ccopy, tw, NULL);
    gtk_accel_group_connect (accel_group, 'c', GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE, copy_closure);

    paste_closure = g_cclosure_new_swap ((GCallback) cpaste, tw, NULL);
    gtk_accel_group_connect (accel_group, 'v', GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE, paste_closure);

    gtk_window_set_decorated (GTK_WINDOW(tw->window), FALSE);

    /*  Set a window icon! */
    window_icon = gdk_pixbuf_new_from_file (window_icon_file, NULL);

    if (window_icon == NULL)
    {
        TILDA_PERROR ();
        DEBUG_ERROR ("Cannot open window icon");
        fprintf (stderr, _("Unable to set tilda's icon: %s\n"), window_icon_file);
    }
    else
    {
        gtk_window_set_icon (GTK_WINDOW(tw->window), window_icon);
        g_object_unref (window_icon);
    }

    gtk_widget_set_size_request (GTK_WIDGET(tw->window), 0, 0);

    /* Initialize and set up the keybinding to toggle tilda's visibility. */
    tomboy_keybinder_init ();
    ret = tomboy_keybinder_bind (config_getstr ("key"), onKeybindingPull, tw);

    if (!ret)
    {
        /* Something really bad happened, we were unable to bind the key. */
        // FIXME
        DEBUG_ERROR ("Unable to bind key");
        return FALSE;
    }

    /* Set up all window properties */
    if (config_getbool ("pinned"))
        gtk_window_stick (GTK_WINDOW(tw->window));

    gtk_window_set_keep_above (GTK_WINDOW(tw->window), config_getbool ("above"));

    /* Position the window, and show it if we're ready */
    tw->current_state = UP;
    gtk_window_move (GTK_WINDOW(tw->window), config_getint ("x_pos"), config_getint ("y_pos"));
    gtk_window_set_default_size (GTK_WINDOW(tw->window), config_getint ("max_width"), config_getint ("max_height"));
    gtk_window_resize (GTK_WINDOW(tw->window), config_getint ("max_width"), config_getint ("max_height"));
    gdk_flush ();

    if (config_getbool ("hidden"))
    {
        /* It does not cause graphical glitches to make tilda hidden on start this way.
         * It does make tilda appear much faster on it's first appearance, so I'm leaving
         * it this way, because it has a good benefit, and no apparent drawbacks. */
        gtk_widget_show (GTK_WIDGET(tw->window));
        gtk_widget_hide (GTK_WIDGET(tw->window));
    }
    else
    {
        pull (tw, PULL_DOWN);
    }

    return TRUE;
}

