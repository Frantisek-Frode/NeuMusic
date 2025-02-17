from dbus_next.aio import MessageBus
from dbus_next.service import (ServiceInterface,
                               method, dbus_property, signal)
from dbus_next import Variant, DBusError
import dbus_next.constants

import asyncio

import subprocess
import os
import random


class MP2(ServiceInterface):
    def __init__(self):
        super().__init__('org.mpris.MediaPlayer2')
        self._bar = 105

    @dbus_property()
    def Identity(self) -> 's':
        return "Python player"

####
    @method()
    def Frobate(self, foo: 'i', bar: 's') -> 'a{us}':
        print(f'called Frobate with foo={foo} and bar={bar}')

        return {
            1: 'one',
            2: 'two'
        }

    @method()
    async def Bazify(self, bar: '(iiu)') -> 'vv':
        print(f'called Bazify with bar={bar}')

        return [Variant('s', 'example'), Variant('s', 'bazify')]

    @method()
    def Mogrify(self, bar: '(iiav)'):
        raise DBusError('com.example.error.CannotMogrify',
                        'it is not possible to mogrify')

    @signal()
    def Changed(self) -> 'b':
        return True

    @Identity.setter
    def Bar(self, val: 'y'):
        if self._bar == val:
            return

        self._bar = val

        self.emit_properties_changed({'Bar': self._bar})
####


class MP2_Player(ServiceInterface):
    def __init__(self):
        super().__init__('org.mpris.MediaPlayer2.Player')
        self.files = [f for f in os.listdir("data") if os.path.isfile(f"data/{f}")]
        random.shuffle(self.files)
        self.index = 0
        self.playing = 0

    def play():
        pass

    def stop():
        pass

    @dbus_property(dbus_next.constants.PropertyAccess.READ)
    def Metadata(self) -> 'a{sv}':
        return {
            "mpris:trackid": Variant('o', "/"),
            "xesam:title": Variant('s', "gg")
        }

    @method()
    def Next(self):
        print(">>")

    @method()
    def Previous(self):
        print("<<")

    @method()
    def Pause(self):
        print("||")

    @method()
    def PlayPause(self):
        print(">|")

    @method()
    def Stop(self):
        print("STOP")

    @method()
    def Play(self):
        print("|>")


async def main():
    bus = await MessageBus().connect()
    mp2 = MP2()
    bus.export('/org/mpris/MediaPlayer2', mp2)
    player = MP2_Player()
    bus.export('/org/mpris/MediaPlayer2', player)
    await bus.request_name('org.mpris.MediaPlayer2.asdfgh')

    await bus.wait_for_disconnect()

asyncio.get_event_loop().run_until_complete(main())

