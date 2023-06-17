#include <dbus/dbus.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CANNOT_CONN_TO_DBUS_FATAL 1
#define CANNOT_READ_BAT_CAPACITY 2

#define CHECK_INTERVAL 60

#define CRITICAL_BAT_LEVEL 5
#define WARN_BAT_LEVEL 20
#define INFORM_BAT_LEVEL 30

enum NotificationType
{
  NONE = 1,
  INFORM,
  WARN,
  CRITICAL
};

struct NotificationStatus
{
  bool inform;
  bool warn;
  bool critical;
};

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

enum NotificationType
get_notification_type ()
{
  int capacity = 0;

  if (!get_battery_capacity (&capacity))
    {
      exit (CANNOT_READ_BAT_CAPACITY);
    }

  printf ("Battery level %d\n", capacity);

  enum NotificationType res = NONE;

  if (capacity <= CRITICAL_BAT_LEVEL)
    {
      res = CRITICAL;
    }

  else if (capacity <= WARN_BAT_LEVEL)
    {
      res = WARN;
    }

  else if (capacity <= INFORM_BAT_LEVEL)
    {
      res = CRITICAL;
    }

  return res;
}

int
main ()
{
  enum NotificationType type;
  DBusConnection *connection = NULL;
  struct NotificationStatus status = { false, false, false };

  if (!connect_to_dbus (&connection))
    {
      fprintf (stderr, "Cannot connect to dbus. Exiting...");
      exit (CANNOT_CONN_TO_DBUS_FATAL);
    }

  while (1)
    {
      type = get_notification_type ();
      if (type == INFORM && !status.inform)
        {
          send_user_notification (connection);
          status.inform = true;
        }

      else if (type == WARN && !status.warn)
        {
          send_user_notification (connection);
          status.warn = true;
        }

      else if (type == CRITICAL && !status.critical)
        {
          send_user_notification (connection);
          status.critical = true;
        }

      sleep (CHECK_INTERVAL);
    }

  dbus_connection_unref (connection);
  return 0;
}
