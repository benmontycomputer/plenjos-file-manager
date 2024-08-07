/* files-window.h
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

#pragma once

#include <gtk/gtk.h>
#include <libhandy-1/handy.h>
#include <dirent.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>

G_BEGIN_DECLS

#define FILES_TYPE_WINDOW (files_window_get_type())

G_DECLARE_FINAL_TYPE (FilesWindow, files_window, FILES, WINDOW, HdyApplicationWindow)

G_END_DECLS

enum ProcessedFileType {
  PROCESSED_FILE_TYPE_DIRECTORY,
  PROCESSED_FILE_TYPE_DIRECTORY_LINK,
  PROCESSED_FILE_TYPE_FILE_LINK,
  PROCESSED_FILE_TYPE_FILE,
};

typedef struct ProcessedFileInfo {
  gchar *name;
  enum ProcessedFileType base_type;
} ProcessedFileInfo;
