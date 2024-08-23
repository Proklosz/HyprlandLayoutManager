#include <gtk/gtk.h>
#include <math.h> // For fmax()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_SCALE_FACTOR 0.2

typedef struct {
  char name[50];
  int x, y, width, height; // Original dimensions
  float scale;             // Scaling factor
  gboolean dragging;
  double drag_start_x,
      drag_start_y;  // Drag start position in scaled coordinates
  gboolean snapping; // Flag to indicate if snapping is occurring
} Monitor;

Monitor monitors[10];
int monitor_count = 0;
Monitor *dragged_monitor = NULL;
double drag_start_x = 0, drag_start_y = 0;
double scale_factor = INITIAL_SCALE_FACTOR;

void apply_css(GtkWidget *widget, GtkStyleProvider *provider) {
  gtk_style_context_add_provider(gtk_widget_get_style_context(widget), provider,
                                 GTK_STYLE_PROVIDER_PRIORITY_USER);
}

void fetch_monitor_data() {
  FILE *fp;
  char path[1035];
  Monitor monitor;
  int in_monitor_block = 0;

  fp = popen("hyprctl monitors", "r");
  if (fp == NULL) {
    printf("Failed to run command\n");
    exit(1);
  }

  while (fgets(path, sizeof(path), fp) != NULL) {
    if (strstr(path, "Monitor ") && strstr(path, "ID")) {
      if (in_monitor_block) {
        monitors[monitor_count++] = monitor;
      }
      in_monitor_block = 1;
      memset(&monitor, 0, sizeof(Monitor));
      sscanf(path, "Monitor %49s", monitor.name);
    }
    if (in_monitor_block) {
      if (strstr(path, "@")) {
        int width, height, x, y;
        if (sscanf(path, "%dx%d@%*f at %dx%d", &width, &height, &x, &y) == 4) {
          monitor.width = width;
          monitor.height = height;
          monitor.x = x;
          monitor.y = y;
        }
      } else if (strstr(path, "scale:")) {
        sscanf(path, " scale: %f", &monitor.scale);
      }
    }
  }
  if (in_monitor_block) {
    monitors[monitor_count++] = monitor;
  }

  pclose(fp);
}

gboolean check_intersection(Monitor *a, Monitor *b) {
  return a->x < b->x + b->width && a->x + a->width > b->x &&
         a->y < b->y + b->height && a->y + a->height > b->y;
}

void adjust_position(Monitor *dragged) {
  int adjusted = 0;
  for (int i = 0; i < monitor_count; i++) {
    if (&monitors[i] == dragged)
      continue;

    Monitor *other = &monitors[i];

    if (check_intersection(dragged, other)) {
      int dx = 0, dy = 0;
      if (dragged->x < other->x) {
        dx = other->x - (dragged->x + dragged->width);
      } else if (dragged->x + dragged->width > other->x) {
        dx = other->x + other->width - dragged->x;
      }

      if (dragged->y < other->y) {
        dy = other->y - (dragged->y + dragged->height);
      } else if (dragged->y + dragged->height > other->y) {
        dy = other->y + other->height - dragged->y;
      }

      if (abs(dx) < abs(dy)) {
        dragged->x += dx;
      } else {
        dragged->y += dy;
      }

      adjusted = 1;
    }
  }
  if (adjusted) {
    dragged->snapping = TRUE;
  } else {
    dragged->snapping = FALSE;
  }
}

void center_bounding_box(GtkWidget *widget) {
  if (monitor_count == 0)
    return;

  int min_x = monitors[0].x, max_x = monitors[0].x + monitors[0].width;
  int min_y = monitors[0].y, max_y = monitors[0].y + monitors[0].height;

  for (int i = 1; i < monitor_count; i++) {
    int cur_min_x = monitors[i].x;
    int cur_max_x = monitors[i].x + monitors[i].width;
    int cur_min_y = monitors[i].y;
    int cur_max_y = monitors[i].y + monitors[i].height;

    if (cur_min_x < min_x)
      min_x = cur_min_x;
    if (cur_max_x > max_x)
      max_x = cur_max_x;
    if (cur_min_y < min_y)
      min_y = cur_min_y;
    if (cur_max_y > max_y)
      max_y = cur_max_y;
  }

  int bbox_width = max_x - min_x;
  int bbox_height = max_y - min_y;

  GtkWindow *window = GTK_WINDOW(gtk_widget_get_toplevel(widget));
  int window_width, window_height;
  gtk_window_get_size(window, &window_width, &window_height);

  int new_x = (window_width / scale_factor - bbox_width) / 2;
  int new_y = (window_height / scale_factor - bbox_height) / 2;

  int offset_x = new_x - min_x;
  int offset_y = new_y - min_y;

  for (int i = 0; i < monitor_count; i++) {
    monitors[i].x += offset_x;
    monitors[i].y += offset_y;
  }
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event,
                                gpointer user_data) {
  for (int i = 0; i < monitor_count; i++) {
    Monitor *monitor = &monitors[i];
    double scaled_x = monitor->x * scale_factor;
    double scaled_y = monitor->y * scale_factor;
    double scaled_width = monitor->width * scale_factor;
    double scaled_height = monitor->height * scale_factor;

    if (event->x >= scaled_x && event->x <= scaled_x + scaled_width &&
        event->y >= scaled_y && event->y <= scaled_y + scaled_height) {
      dragged_monitor = monitor;
      drag_start_x = event->x;
      drag_start_y = event->y;
      dragged_monitor->snapping = FALSE;
      return TRUE;
    }
  }
  return FALSE;
}

static gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event,
                                 gpointer user_data) {
  if (dragged_monitor) {
    double dx = (event->x - drag_start_x) / scale_factor;
    double dy = (event->y - drag_start_y) / scale_factor;

    if (!dragged_monitor->snapping) {
      for (int i = 0; i < monitor_count; i++) {
        if (&monitors[i] != dragged_monitor) {
          monitors[i].x -= (int)dx;
          monitors[i].y -= (int)dy;
        }
      }
    }

    dragged_monitor->x += (int)dx;
    dragged_monitor->y += (int)dy;

    adjust_position(dragged_monitor);

    drag_start_x = event->x;
    drag_start_y = event->y;

    gtk_widget_queue_draw(widget);
    return TRUE;
  }
  return FALSE;
}

static gboolean on_button_release(GtkWidget *widget, GdkEventButton *event,
                                  gpointer user_data) {
  center_bounding_box(widget);
  gtk_widget_queue_draw(widget);
  if (dragged_monitor) {
    dragged_monitor = NULL;
    return TRUE;
  }

  return FALSE;
}

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {

  if (monitor_count == 0)
    return FALSE;

  // Find the monitor with the smallest x + y
  Monitor *top_left_monitor = &monitors[0];
  int min_sum = monitors[0].x + monitors[0].y;

  for (int i = 1; i < monitor_count; i++) {
    int current_sum = monitors[i].x + monitors[i].y;
    if (current_sum < min_sum) {
      min_sum = current_sum;
      top_left_monitor = &monitors[i];
    }
  }

  for (int i = 0; i < monitor_count; i++) {
    double scaled_x = monitors[i].x * scale_factor;
    double scaled_y = monitors[i].y * scale_factor;
    double scaled_width = monitors[i].width * scale_factor;
    double scaled_height = monitors[i].height * scale_factor;

    int relative_x = monitors[i].x - top_left_monitor->x;
    int relative_y = monitors[i].y - top_left_monitor->y;

    // Set the color for the rectangle's border
    // cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    // cairo_rectangle(cr, scaled_x, scaled_y, scaled_width, scaled_height);
    // cairo_stroke(cr);

    // Set the color for the rectangle's fill
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_rectangle(cr, scaled_x, scaled_y, scaled_width, scaled_height);

    cairo_fill(cr);

    // Set the color for the rectangle's fill
    cairo_set_source_rgb(cr, 0.2 * (i % 3), 0.2 * (i % 3), 0.2 + 0.2 * (i % 3));
    cairo_rectangle(cr, scaled_x + 2, scaled_y + 2, scaled_width - 4,
                    scaled_height - 4);

    cairo_fill(cr);

    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_font_size(cr, 20.0);
    cairo_move_to(cr, scaled_x + 20, scaled_y + 40);
    cairo_show_text(cr, monitors[i].name);
    cairo_stroke(cr);

    if (scale_factor > 0.15) {
      char text[100];
      snprintf(text, sizeof(text), "Position : %d X %d", relative_x,
               relative_y);

      cairo_set_source_rgb(cr, 1, 1, 1);
      cairo_set_font_size(cr, 15.0);
      cairo_move_to(cr, scaled_x + 20, scaled_y + 60);
      cairo_show_text(cr, text);
      cairo_stroke(cr);

      char text_2[100];
      snprintf(text_2, sizeof(text), "Resolution : %d X %d", monitors[i].width,
               monitors[i].height);

      cairo_set_source_rgb(cr, 1, 1, 1);
      cairo_set_font_size(cr, 15.0);
      cairo_move_to(cr, scaled_x + 20, scaled_y + 80);
      cairo_show_text(cr, text_2);
      cairo_stroke(cr);
    }
  }
  return FALSE;
}

static void on_window_resize(GtkWidget *widget, GtkAllocation *allocation,
                             gpointer user_data) {
  center_bounding_box(widget);
  gtk_widget_queue_draw(widget);
}

static void on_save_button_clicked(GtkButton *button, gpointer user_data) {
  if (monitor_count == 0)
    return;

  // Find the monitor with the smallest x + y
  Monitor *top_left_monitor = &monitors[0];
  int min_sum = monitors[0].x + monitors[0].y;

  for (int i = 1; i < monitor_count; i++) {
    int current_sum = monitors[i].x + monitors[i].y;
    if (current_sum < min_sum) {
      min_sum = current_sum;
      top_left_monitor = &monitors[i];
    }
  }

  // Print positions with the top-left monitor as (0, 0)
  for (int i = 0; i < monitor_count; i++) {
    int relative_x = monitors[i].x - top_left_monitor->x;
    int relative_y = monitors[i].y - top_left_monitor->y;

    printf("Monitor %s: x=%d, y=%d, width=%d, height=%d\n", monitors[i].name,
           relative_x, relative_y, monitors[i].width, monitors[i].height);

    char command[256];
    snprintf(command, sizeof(command),
             "hyprctl keyword monitor %s,%dx%d,%dx%d,1", monitors[i].name,
             monitors[i].width, monitors[i].height, relative_x, relative_y);

    // Execute the command
    system(command);
  }
}

static void on_copy_config_button_clicked(GtkButton *button,
                                          gpointer user_data) {
  if (monitor_count == 0)
    return;

  // Find the monitor with the smallest x + y
  Monitor *top_left_monitor = &monitors[0];
  int min_sum = monitors[0].x + monitors[0].y;

  for (int i = 1; i < monitor_count; i++) {
    int current_sum = monitors[i].x + monitors[i].y;
    if (current_sum < min_sum) {
      min_sum = current_sum;
      top_left_monitor = &monitors[i];
    }
  }

  char config[1024]; // Increased size to handle larger configurations
  config[0] = '\0';  // Print positions with the top-left monitor as (0, 0)
  for (int i = 0; i < monitor_count; i++) {
    int relative_x = monitors[i].x - top_left_monitor->x;
    int relative_y = monitors[i].y - top_left_monitor->y;

    char command[256];
    snprintf(command, sizeof(command), "monitor=%s,%dx%d,%dx%d,1\n",
             monitors[i].name, monitors[i].width, monitors[i].height,
             relative_x, relative_y);

    strncat(config, command, sizeof(config) - strlen(config) - 1);
  }

  // Copy the config string to clipboard
  GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text(clipboard, config, -1);

  printf("Config copied to clipboard:\n%s", config);
}

static gboolean on_scroll_event(GtkWidget *widget, GdkEventScroll *event,
                                gpointer user_data) {
  if (event->direction == GDK_SCROLL_UP && scale_factor < 0.3) {
    scale_factor *= 1.1; // Increase scale factor
  } else if (event->direction == GDK_SCROLL_DOWN && scale_factor > 0.1) {
    scale_factor /= 1.1; // Decrease scale factor
  }

  center_bounding_box(widget);

  gtk_widget_queue_draw(widget); // Redraw the drawing area with the new scale
  return TRUE;
}

int main(int argc, char *argv[]) {
  gtk_init(&argc, &argv);

  GtkCssProvider *provider;

  fetch_monitor_data();

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "Hyprland Display Arranger");
  // gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

  // Create a vertical box to contain the drawing area and the button
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  // gtk_widget_set_vexpand(vbox, TRUE);
  GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_hexpand(hbox, TRUE);

  GtkWidget *drawing_area = gtk_drawing_area_new();
  // gtk_widget_set_hexpand(drawing_area, TRUE);
  // gtk_widget_set_vexpand(drawing_area, TRUE);

  GtkWidget *save_button = gtk_button_new_with_label("Apply");

  gtk_box_pack_start(GTK_BOX(vbox), drawing_area, TRUE, TRUE, 0);
  gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  GtkWidget *copy_config_button =
      gtk_button_new_with_label("Copy config to clipboard");

  gtk_box_pack_start(GTK_BOX(hbox), save_button, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(hbox), copy_config_button, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(on_draw), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "button-press-event",
                   G_CALLBACK(on_button_press), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "motion-notify-event",
                   G_CALLBACK(on_motion_notify), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "button-release-event",
                   G_CALLBACK(on_button_release), NULL);
  g_signal_connect(G_OBJECT(drawing_area), "scroll-event",
                   G_CALLBACK(on_scroll_event), NULL);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  // g_signal_connect(window, "size-allocate", G_CALLBACK(on_window_resize),
  // NULL);
  //
  // on_copy_config_button_clicked

  g_signal_connect(G_OBJECT(window), "size-allocate",
                   G_CALLBACK(on_window_resize), NULL);

  void on_window_resize(GtkWidget * widget, GdkRectangle * allocation,
                        gpointer data) {
    gtk_widget_queue_draw(widget);
  }

  g_signal_connect(save_button, "clicked", G_CALLBACK(on_save_button_clicked),
                   NULL);
  g_signal_connect(copy_config_button, "clicked",
                   G_CALLBACK(on_copy_config_button_clicked), NULL);

  gtk_widget_add_events(drawing_area,
                        GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK |
                            GDK_BUTTON_RELEASE_MASK | GDK_SCROLL_MASK);

  center_bounding_box(drawing_area); // Initial centering
  gtk_widget_queue_draw(drawing_area);

  // Load the CSS file
  provider = gtk_css_provider_new();
  gchar *css_path =
      g_build_filename("/usr/local/share/hyprlayout/h_d_a_css.css",
                       NULL); // Construct the full path to index.css

  gtk_css_provider_load_from_path(provider, css_path, NULL);
  // gtk_css_provider_load_from_path(provider, "index.css", NULL);

  // Apply CSS to the widgets
  apply_css(window, GTK_STYLE_PROVIDER(provider));
  apply_css(save_button, GTK_STYLE_PROVIDER(provider));
  apply_css(copy_config_button, GTK_STYLE_PROVIDER(provider));

  gtk_widget_show_all(window);

  // Connect the destroy signal to gtk_main_quit to close the app when the
  // window is closed
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

  gtk_main();

  // Free the provider when done
  g_object_unref(provider);

  return 0;
}
