#include <dbus/dbus.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define CANNOT_CONN_TO_DBUS_FATAL 1
#define CANNOT_READ_BAT_CAPACITY 2

bool
connect_to_dbus (DBusConnection **conn)
{
  bool res = true;
  DBusError error;

  dbus_error_init (&error);
  *conn = dbus_bus_get (DBUS_BUS_SESSION, &error);

  if (dbus_error_is_set (&error))
    {
      fprintf (stderr, "Error while connecting to DBUS: %s\n", error.message);
      res = false;
      goto conn_free;
    }

  if (*conn == NULL)
    {
      res = false;
      goto conn_free;
    }

conn_free:
  dbus_error_free (&error);

  return res;
}

bool
read_line_from_file (const char *path, char **line, size_t *len)
{
  bool res = true;
  FILE *file = fopen (path, "r");

  if (file == NULL)
    {
      fprintf (stderr, "Cannot open file: %s\n", path);
      return false;
    }

  int ret = getline (line, len, file);

  if (ret == -1)
    {
      fprintf (stderr, "Error whie reading from file: %s\n", path);
      free (*line);
      *line = NULL;
      res = false;
    }

  fclose (file);
  return res;
}

bool
get_battery_capacity (int *capacity)
{
  const char *path = "/sys/class/power_supply/BAT0/capacity";
  char *line = NULL;
  size_t len = 0;

  if (!read_line_from_file (path, &line, &len))
    {
      fprintf (stderr, "Cannot read battery capacity\n");
      return false;
    }

  *capacity = atoi (line);
  if (*capacity == 0)
    {
      fprintf (stderr, "Cannot covert battery capacity\n");
      return false;
    }

  return true;
}

bool
send_user_notification (DBusConnection *conn)
{
  DBusMessage *message = dbus_message_new_method_call (
      "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
      "org.freedesktop.Notifications", "Notify");
  DBusMessageIter iter[4];
  dbus_message_iter_init_append (message, iter);
  char *application = "sbw";
  dbus_message_iter_append_basic (iter, 's', &application);
  unsigned id = 0;
  dbus_message_iter_append_basic (iter, 'u', &id);
  char *icon = "dialog-information";
  dbus_message_iter_append_basic (iter, 's', &icon);
  char *summary = "Hello world!";
  dbus_message_iter_append_basic (iter, 's', &summary);
  char *body = "This is an example notification.";
  dbus_message_iter_append_basic (iter, 's', &body);
  dbus_message_iter_open_container (iter, 'a', "s", iter + 1);
  dbus_message_iter_close_container (iter, iter + 1);
  dbus_message_iter_open_container (iter, 'a', "{sv}", iter + 1);
  dbus_message_iter_open_container (iter + 1, 'e', 0, iter + 2);
  char *urgency = "urgency";
  dbus_message_iter_append_basic (iter + 2, 's', &urgency);
  dbus_message_iter_open_container (iter + 2, 'v', "y", iter + 3);
  enum
  {
    LOW,
    NORMAL,
    CRITICAL
  };
  unsigned char level = CRITICAL;
  dbus_message_iter_append_basic (iter + 3, 'y', &level);
  dbus_message_iter_close_container (iter + 2, iter + 3);
  dbus_message_iter_close_container (iter + 1, iter + 2);
  dbus_message_iter_close_container (iter, iter + 1);
  int timeout = 5 * 1000;
  dbus_message_iter_append_basic (iter, 'i', &timeout);
  dbus_connection_send (conn, message, 0);
  dbus_connection_flush (conn);
  dbus_message_unref (message);
  return true;
}

int
main ()
{
  DBusConnection *connection = NULL;
  if (!connect_to_dbus (&connection))
    {
      fprintf (stderr, "Cannot connect to dbus. Exiting...");
      exit (CANNOT_CONN_TO_DBUS_FATAL);
    }

  int capacity = 0;
  if (!get_battery_capacity (&capacity))
    {
      exit (CANNOT_READ_BAT_CAPACITY);
    }

  printf ("Capacity: %d\n", capacity);
  send_user_notification (connection);

  dbus_connection_unref (connection);
  return 0;
}
