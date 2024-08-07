/* files-window.c
 *
 * Copyright 2023 Benjamin Montgomery
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "files-config.h"
#include "files-window.h"

#define MAX_NAME_LEN 32

GList *directory_get_files(char directory[]) {
  // TODO: sort the files and directories in some order
  GList *files = NULL;

  DIR *d;
  struct dirent *dir;
  d = opendir(directory);
  if (d) {
    while ((dir = readdir(d)) != NULL) {
      size_t len = sizeof(dir->d_name);
      gchar *newName = malloc(len);

      strncpy (newName, dir->d_name, len);

      newName[len - 1] = '\0';

      ProcessedFileInfo *pfInfo = malloc (sizeof (ProcessedFileInfo));
      pfInfo->name = newName;

      // dir->d_type might always return DT_UNKNOWN on some filesystems.
      // See https://stackoverflow.com/questions/23958040/checking-if-a-dir-entry-returned-by-readdir-is-a-directory-link-or-file-dent
      // for more info.
      if (dir->d_type != DT_UNKNOWN) {
        pfInfo->base_type = (dir->d_type == DT_DIR);
        switch(dir->d_type) {
          case DT_DIR:
            pfInfo->base_type = PROCESSED_FILE_TYPE_DIRECTORY;
            break;
          case DT_LNK: {
            struct stat file_info;
            stat (dir->d_name, &file_info);
            pfInfo->base_type = ((file_info.st_mode & S_IFMT)==S_IFDIR) ? PROCESSED_FILE_TYPE_DIRECTORY_LINK : PROCESSED_FILE_TYPE_FILE_LINK;
            break;
          } default:
            pfInfo->base_type = PROCESSED_FILE_TYPE_FILE;
        }
      } else {
        // TODO: differentiate between files, links leading to directories, and actual directories.
        //https://stackoverflow.com/questions/7674287/c-checking-the-type-of-a-file-using-lstat-and-macros-doesnt-work
        struct stat file_info;
        lstat (dir->d_name, &file_info);
        if ((file_info.st_mode & S_IFMT)==S_IFDIR) {
          pfInfo->base_type = PROCESSED_FILE_TYPE_DIRECTORY;
        } else if ((file_info.st_mode & S_IFMT)==S_IFLNK) {
          struct stat file_info_stat;
          stat (dir->d_name, &file_info_stat);
          pfInfo->base_type = ((file_info_stat.st_mode & S_IFMT)==S_IFDIR) ? PROCESSED_FILE_TYPE_DIRECTORY_LINK : PROCESSED_FILE_TYPE_FILE_LINK;
        } else {
          pfInfo->base_type = PROCESSED_FILE_TYPE_FILE;
        }
      }

      files = g_list_append (files, pfInfo);
    }
    closedir(d);
  }
  return(files);
}

struct _FilesWindow
{
  GtkApplicationWindow  parent_instance;

  GtkBox *window_box;

  GtkGrid *main_view_grid;

  GtkButton *main_flap_menu_button;
  HdyFlap *main_flap;

  GtkButton *nav_back_button;
  GtkButton *nav_forward_button;

  gchar *current_path;

  GList *previous_paths;
  GList *next_paths;

  GList *current_files;
};

G_DEFINE_TYPE (FilesWindow, files_window, HDY_TYPE_APPLICATION_WINDOW)

static void
files_window_class_init (FilesWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/com/plenjos/Files/files-window.ui");
  gtk_widget_class_bind_template_child (widget_class, FilesWindow, window_box);
  gtk_widget_class_bind_template_child (widget_class, FilesWindow, main_view_grid);
  gtk_widget_class_bind_template_child (widget_class, FilesWindow, main_flap_menu_button);
  gtk_widget_class_bind_template_child (widget_class, FilesWindow, main_flap);
  gtk_widget_class_bind_template_child (widget_class, FilesWindow, nav_back_button);
  gtk_widget_class_bind_template_child (widget_class, FilesWindow, nav_forward_button);
}

static void
refresh_view (FilesWindow *self);

static void toggle_flap (GtkWidget *widget, FilesWindow *self) {
  hdy_flap_set_reveal_flap (self->main_flap, !hdy_flap_get_reveal_flap (self->main_flap));
}

static void nav_back (GtkWidget *widget, FilesWindow *self) {
  if (!self->previous_paths || !g_list_length (self->previous_paths)) {
    return;
  }

  gchar *new_path = g_strdup (g_list_last (self->previous_paths)->data);

  self->previous_paths = g_list_remove (self->previous_paths, g_list_last (self->previous_paths)->data);
  self->next_paths = g_list_prepend (self->next_paths, g_strdup (self->current_path));

  free (self->current_path);

  self->current_path = new_path;
  refresh_view (self);
}

static void nav_forward (GtkWidget *widget, FilesWindow *self) {
  if (!self->next_paths || !g_list_length (self->next_paths)) {
    return;
  }

  gchar *new_path = g_strdup (g_list_first (self->next_paths)->data);

  self->next_paths = g_list_remove (self->next_paths, g_list_first (self->next_paths)->data);
  self->previous_paths = g_list_append (self->previous_paths, g_strdup (self->current_path));

  free (self->current_path);

  self->current_path = new_path;
  refresh_view (self);
}

static void
files_window_init (FilesWindow *self)
{
  hdy_init ();
  gtk_widget_init_template (GTK_WIDGET (self));
  self->current_path = malloc (2);
  sprintf (self->current_path, "/");
  refresh_view (self);

  GtkCssProvider *cssProvider = gtk_css_provider_new();
  gtk_css_provider_load_from_resource (cssProvider, "/com/plenjos/Files/theme.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                               GTK_STYLE_PROVIDER (cssProvider),
                               GTK_STYLE_PROVIDER_PRIORITY_USER);

  g_signal_connect (self->main_flap_menu_button, "clicked", toggle_flap, self);
  g_signal_connect (self->nav_back_button, "clicked", nav_back, self);
  g_signal_connect (self->nav_forward_button, "clicked", nav_forward, self);
}

void free_file (GtkWidget *widget, gpointer user_data) {
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (widget)), widget);
  //gtk_widget_destroy (widget);
}

typedef struct {
  FilesWindow *self;
  ProcessedFileInfo *file;
} ChangeDirArgs;

void change_dir (GtkButton *button, ChangeDirArgs *args) {
  size_t newlen = strlen (args->self->current_path) + 1 /* <- this is for the first "/" */ + strlen (args->file->name) + 1 /* <- this is for the null terminator */;

  g_list_free_full (args->self->next_paths, free);
  args->self->next_paths = NULL;

  gchar *old_path = g_strdup (args->self->current_path);

  args->self->previous_paths = g_list_append (args->self->previous_paths, old_path);


  free (args->self->current_path);args->self->current_path = malloc (newlen);
  snprintf (args->self->current_path, newlen, "%s/%s", old_path, args->file->name);

  refresh_view (args->self);
}

void add_file (ProcessedFileInfo *data, FilesWindow *self) {
  GtkWidget *button = gtk_button_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  char *name_uncut = data->name;

  GtkWidget *label = NULL;

  if (strlen (name_uncut) > MAX_NAME_LEN) {
    char *name = malloc (MAX_NAME_LEN + 1);
    snprintf (name, MAX_NAME_LEN + 1, "%s", name_uncut);
    // TODO: cut off the name in the middle so the extension is still visible
    name[MAX_NAME_LEN - 1] = '.';
    name[MAX_NAME_LEN - 2] = '.';
    name[MAX_NAME_LEN - 3] = '.';
    label = gtk_label_new (name);
    free (name);
  } else {
    label = gtk_label_new (name_uncut);
  }

  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_CHAR);

  gtk_widget_set_margin_start (box, 4);
  gtk_widget_set_margin_end (box, 4);
  gtk_widget_set_margin_top (box, 4);
  gtk_widget_set_margin_bottom (box, 2);
  gtk_widget_set_size_request (button, 80, 48);
  gtk_widget_set_name (button, "files_button");
  gtk_widget_set_hexpand (button, FALSE);
  gtk_widget_set_vexpand (button, FALSE);
  GtkWidget *image = gtk_image_new_from_icon_name ((data->base_type == PROCESSED_FILE_TYPE_DIRECTORY || data->base_type == PROCESSED_FILE_TYPE_DIRECTORY_LINK) ? "folder" : "emblem-documents", GTK_ICON_SIZE_DIALOG);
  gtk_container_add (GTK_CONTAINER (box), image);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_container_add (GTK_CONTAINER (button), box);

  ChangeDirArgs *cdArgs = malloc (sizeof (ChangeDirArgs));

  cdArgs->self = self;
  cdArgs->file = data;

  g_signal_connect (G_OBJECT (button), "clicked", (data->base_type == PROCESSED_FILE_TYPE_DIRECTORY || data->base_type == PROCESSED_FILE_TYPE_DIRECTORY_LINK) ? change_dir : NULL, cdArgs);

  /*self->current_files = g_list_append (self->current_files, label);
  self->current_files = g_list_append (self->current_files, image);
  self->current_files = g_list_append (self->current_files, box);*/
  self->current_files = g_list_append (self->current_files, button);
  size_t prevChildren = g_list_length (gtk_container_get_children (GTK_CONTAINER (self->main_view_grid)));
  gtk_grid_attach (self->main_view_grid, GTK_WIDGET (button), (int)(prevChildren % 5), (int)(prevChildren / 5), 1, 1);
}

static int sort_files (ProcessedFileInfo *first,
                       ProcessedFileInfo *second)
{
  return strcmp (first->name, second->name);
}

static void
refresh_view (FilesWindow *self)
{
  g_list_foreach (self->current_files, (GFunc) free_file, NULL);
  g_list_free (self->current_files);

  self->current_files = NULL;

  GList *new_files = directory_get_files (self->current_path);

  new_files = g_list_sort (new_files, (GCompareFunc)sort_files);

  g_list_foreach (new_files, (GFunc) add_file, self);

  gtk_widget_show_all (self->main_view_grid);
}
