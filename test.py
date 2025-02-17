#!/usr/bin/env python3
"""Printing out gnome multi media keys via dbus-python.
"""
import gobject
import dbus
import dbus.service
import dbus.mainloop.glib


def on_mediakey(comes_from, what):
    """ gets called when multimedia keys are pressed down.
    """
    print(f'comes from:{comes_from}  what:{what}')
    if what in ['Stop','Play','Next','Previous']:
        print('Got a multimedia key!')
    else:
        print('Got a multimedia key...')

# set up the glib main loop.
dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.Bus(dbus.Bus.TYPE_SESSION)
bus_object = bus.get_object('org.gnome.SettingsDaemon.MediaKeys', '/org/gnome/SettingsDaemon/MediaKeys')

print(dir(bus_object))

# this is what gives us the multi media keys.
dbus_interface='org.gnome.SettingsDaemon.MediaKeys'
bus_object.GrabMediaPlayerKeys("MyMultimediaThingy", 0, 
                               dbus_interface=dbus_interface)

# connect_to_signal registers our callback function.
bus_object.connect_to_signal('MediaPlayerKeyPressed', 
                             on_mediakey)

# and we start the main loop.
mainloop = gobject.MainLoop()
mainloop.run()

