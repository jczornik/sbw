#include <dbus/dbus.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define CANNOT_CONN_TO_DBUS_FATAL 1

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
send_notification ()
{
  return true;
}

bool
get_battery_capacity (int *capacity)
{
  const char *path = "/sys/class/power_supply/BAT0/capacity";
  bool res = true;
  FILE *file = fopen (path, "r");

  if (file == NULL)
    {
      fprintf (stderr, "Cannot open file: %s\n", path);
      return false;
    }

  char *line = NULL;
  size_t len = 0;

  int ret = getline (&line, &len, file);

  if (ret == -1)
    {
      fprintf (stderr, "Error whie reading from file: %s\n", path);
      res = false;
      goto capacity_free;
    }

  *capacity = atoi (line);

capacity_free:
  free (line);
  fclose (file);
  return res;
}

int
main ()
{
  DBusConnection *connection = NULL;
  if (connect_to_dbus (&connection))
    {
      fprintf (stderr, "Cannot connect to dbus. Exiting...");
      exit (CANNOT_CONN_TO_DBUS_FATAL);
    }

  int capacity = 0;
  get_battery_capacity (&capacity);
  printf ("Capacity: %d\n", capacity);

  dbus_connection_unref (connection);
  return 0;
}
