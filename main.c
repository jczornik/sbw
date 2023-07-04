#include <dbus/dbus.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

#define CANNOT_CONN_TO_DBUS_FATAL 1
#define CANNOT_READ_BAT_CAPACITY_FATAL 2
#define CANNOT_READ_BAT_STATUS_FATAL 3
#define UNKNOWN_BATTERY_STATUS_FATAL 4

#define BATTERY_STATUS_CHARGING "Charging"
#define BATTERY_STATUS_CHARGING_LEN 8
#define BATTERY_STATUS_DISCHARGING "Discharging"
#define BATTERY_STATUS_DISCHARGING_LEN 11
#define BATTERY_STATUS_FULL "Full"
#define BATTERY_STATUS_FULL_LEN 4

enum NotificationType
{
  INFORM = 0,
  WARN,
  CRITICAL,
  NONE
};

enum BatteryStatus
{
  CHARGING,
  DISCHARGING,
  UNKNOWN
};

struct NotificationStatus
{
  bool inform;
  bool warn;
  bool critical;
};

struct NotificationMessageInfo
{
  const char *title;
  const char *message;
  const enum NotificationType type;
  const int timeout;
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
  free (line);
  line = NULL;

  if (*capacity == 0)
    {
      fprintf (stderr, "Cannot covert battery capacity\n");
      return false;
    }

  return true;
}

bool
get_battery_status (enum BatteryStatus *status)
{
  const char *path = "/sys/class/power_supply/BAT0/status";
  char *line = NULL;
  size_t len = 0;

  if (!read_line_from_file (path, &line, &len))
    {
      fprintf (stderr, "Cannot read battery status\n");
      return false;
    }

  if (strncmp (BATTERY_STATUS_CHARGING, line, BATTERY_STATUS_CHARGING_LEN)
      == 0)
    {
      *status = CHARGING;
    }

  else if (strncmp (BATTERY_STATUS_DISCHARGING, line,
                    BATTERY_STATUS_DISCHARGING_LEN)
           == 0)
    {
      *status = DISCHARGING;
    }

  else if (strncmp (BATTERY_STATUS_FULL, line, BATTERY_STATUS_FULL_LEN) == 0)
    {
      *status = CHARGING;
    }

  else
    {
      *status = UNKNOWN;
    }

  free (line);
  return true;
}

bool
send_user_notification (DBusConnection *conn,
                        const struct NotificationMessageInfo *info)
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
  const char *icon = "dialog-information";
  dbus_message_iter_append_basic (iter, 's', &icon);

  dbus_message_iter_append_basic (iter, 's', &info->title);
  dbus_message_iter_append_basic (iter, 's', &info->message);
  dbus_message_iter_open_container (iter, 'a', "s", iter + 1);
  dbus_message_iter_close_container (iter, iter + 1);
  dbus_message_iter_open_container (iter, 'a', "{sv}", iter + 1);
  dbus_message_iter_open_container (iter + 1, 'e', 0, iter + 2);
  char *urgency = "urgency";
  dbus_message_iter_append_basic (iter + 2, 's', &urgency);
  dbus_message_iter_open_container (iter + 2, 'v', "y", iter + 3);

  unsigned char level = info->type;
  dbus_message_iter_append_basic (iter + 3, 'y', &level);
  dbus_message_iter_close_container (iter + 2, iter + 3);
  dbus_message_iter_close_container (iter + 1, iter + 2);
  dbus_message_iter_close_container (iter, iter + 1);
  int timeout = info->timeout * 1000;
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
      exit (CANNOT_READ_BAT_CAPACITY_FATAL);
    }

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
      res = INFORM;
    }

  return res;
}

int
main ()
{
  enum NotificationType type;
  enum BatteryStatus bstatus;
  DBusConnection *connection = NULL;
  struct NotificationStatus nstatus = { false, false, false };

  struct NotificationMessageInfo information_message
      = { "Battery getting low",
          "Your battery level is getting low. Please consider charging it.",
          INFORM, INFORM_BAT_LEVEL_INFO_TIMEOUT_SEC };

  struct NotificationMessageInfo warn_message
      = { "Battery getting very low",
          "Your battery level is getting low. Please consider charging it.",
          WARN, WARN_BAT_LEVEL_INFO_TIMEOUT_SEC };

  struct NotificationMessageInfo critical_message
      = { "Battery getting critically low",
          "Your battery level is getting critically low. Your computer will "
          "switch off shortly.",
          CRITICAL, CRITICAL_BAT_LEVEL_INFO_TIMEOUT_SEC };

  if (!connect_to_dbus (&connection))
    {
      fprintf (stderr, "Cannot connect to dbus. Exiting...");
      exit (CANNOT_CONN_TO_DBUS_FATAL);
    }

  while (1)
    {
      if (!get_battery_status (&bstatus))
        {
          exit (CANNOT_READ_BAT_STATUS_FATAL);
        }

      if (bstatus == DISCHARGING)
        {
          type = get_notification_type ();
          if (type == INFORM && !nstatus.inform)
            {
              send_user_notification (connection, &information_message);
              nstatus.inform = true;
            }

          else if (type == WARN && !nstatus.warn)
            {
              send_user_notification (connection, &warn_message);
              nstatus.warn = true;
            }

          else if (type == CRITICAL && !nstatus.critical)
            {
              send_user_notification (connection, &critical_message);
              nstatus.critical = true;
            }
        }

      else if (bstatus == CHARGING)
        {
          nstatus.inform = false;
          nstatus.warn = false;
          nstatus.critical = false;
        }

      else
        {
          exit (UNKNOWN_BATTERY_STATUS_FATAL);
        }

      sleep (CHECK_INTERVAL);
    }

  dbus_connection_unref (connection);
  return 0;
}
