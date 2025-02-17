#include <gio/gio.h>
#include <stdio.h>

static void
on_signal (GDBusProxy *proxy,
           gchar      *sender_name,
           gchar      *signal_name,
           GVariant   *parameters,
           gpointer    user_data)
{
	printf("asd\n");
  gchar *parameters_str;
  parameters_str = g_variant_print (parameters, TRUE);
  g_print (" *** Received Signal: %s: %s\n", signal_name, parameters_str);
  g_free (parameters_str);
}

int
main (int argc, char *argv[])
{
  GMainLoop *loop;
  GError *error;
  GDBusProxyFlags flags;
  GDBusProxy *proxy;

  loop = NULL;
  error = NULL;
  loop = g_main_loop_new (NULL, FALSE);
  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         flags,
                                         NULL, /* GDBusInterfaceInfo */
                                         "org.gnome.SettingsDaemon",
                                         "/org/gnome/SettingsDaemon/MediaKeys",
                                         "org.gnome.SettingsDaemon.MediaKeys",
                                         NULL, /* GCancellable */
                                         &error);
  g_signal_connect (proxy,
                    "g-signal",
                    G_CALLBACK (on_signal),
                    NULL);
  g_dbus_proxy_call (proxy,
          "GrabMediaPlayerKeys",
          g_variant_new ("(su)", "ExampleCCode", 0),
          G_DBUS_CALL_FLAGS_NO_AUTO_START,
          -1,
          NULL, NULL, NULL);

  g_main_loop_run (loop);

 out:
   // Release may not be necessary
   g_dbus_proxy_call (proxy,
               "ReleaseMediaPlayerKeys",
               g_variant_new ("(s)", "ExampleCCode"),
               G_DBUS_CALL_FLAGS_NO_AUTO_START,
               -1,
               NULL, NULL, NULL);

  if (proxy != NULL)
    g_object_unref (proxy);
  if (loop != NULL)
    g_main_loop_unref (loop);
  g_free ("org.gnome.SettingsDaemon");
  g_free ("/org/gnome/SettingsDaemon/MediaKeys");
  g_free ("org.gnome.SettingsDaemon.MediaKeys");

  return 0;
}
