import os
import sys
import time
import socket
import asyncio
import functools
import signal
from pyradiosockets import Daemon, App


class FeedDummy:
    def __init__(self, tcp_port, frame_size, frames_per_second):
        self.tcp_port = tcp_port
        self.frame_size = frame_size
        self.frames_per_second = frames_per_second

    async def run(self):
        await asyncio.sleep(5.)
        data_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            data_socket.connect(('127.0.0.1', self.tcp_port))
        except Exception as e:
            print("Could not connect to TCP socket: %s" % e)

        while True:
            msg = "a" * (self.frame_size - 2)
            try:
                data_socket.send(bytes([0xFF, 0xD8]) + msg.encode('ascii'))
            except:
                pass
            await asyncio.sleep(1. / self.frames_per_second)

    async def run_gst(self):
        await asyncio.sleep(5.)
        proc = await asyncio.create_subprocess_shell("./gst_commands.sh feed")
        await proc.communicate()

class Prompt:
    def __init__(self, daemon, dummy):
        self.loop = asyncio.get_event_loop()
        self.q = asyncio.Queue()
        self.loop.add_reader(sys.stdin, self.got_input)

        self._daemon = daemon
        self._dummy = dummy

    def got_input(self):
        asyncio.ensure_future(self.q.put(sys.stdin.readline()), loop=self.loop)

    async def run(self):
        while True:
            cmd = (await self.q.get()).rstrip('\n')
            if cmd == "a":
                self._dummy.frame_size *=0.8
            elif cmd == "d":
                self._dummy.frame_size *= 1 / 0.8
            elif cmd == "s":
                self._dummy.frames_per_second *= 0.8
            elif cmd == "w":
                self._dummy.frames_per_second *= 1 / 0.8

            elif cmd == "t":
                print("Updating...")
                self._daemon.cmd_update_port(5, 2.)

            self._dummy.frame_size = int(self._dummy.frame_size)

def handle_exception(loop, context):
    msg = context.get("exception", context["message"])
    print(f"Caught exception: {msg}")
    asyncio.create_task(shutdown(loop))

async def shutdown(loop):
    print("Shutting down...")
    tasks = [t for t in asyncio.all_tasks() if t is not
             asyncio.current_task()]
    [task.cancel() for task in tasks]

    await asyncio.gather(*tasks, return_exceptions=True)
    loop.stop()


if __name__ == '__main__':
    os.chdir(os.path.dirname(os.path.realpath(__file__)))
    os.system('make')

    is_up = 'up' in os.uname()[1]
    if len(sys.argv) > 1 and "fake-up" in " ".join(sys.argv[1:]):
        is_up = True

    frames_per_second = 30. if is_up else 100.
    kbits = 2400 if is_up else 20
    frame_size = int(1000. / 8. * kbits / frames_per_second)

    if is_up:
        arg_own = "0xDD"
        arg_other = "0xAA"
    else:
        arg_own = "0xAA"
        arg_other = "0xDD"

    if len(sys.argv) > 1 and "vg" in " ".join(sys.argv[1:]):
        daemon = Daemon("./basic.conf", arg_own, arg_other,
                        "valgrind --leak-check=full --track-origins=yes -s")
    else:
        daemon = Daemon("./basic.conf", arg_own, arg_other)
    daemon.setup()

    input("Confirm? ")

    server = App(daemon, len(sys.argv) > 1 and "gui" in " ".join(sys.argv[1:]))
    dummy = FeedDummy(8885 if is_up else 8881, frame_size, frames_per_second)
    prompt = Prompt(daemon, dummy)

    loop = asyncio.get_event_loop()
    loop.add_signal_handler(signal.SIGINT, lambda: asyncio.create_task(shutdown(loop)))
    loop.add_signal_handler(signal.SIGHUP, lambda: asyncio.create_task(shutdown(loop)))
    loop.add_signal_handler(signal.SIGTERM, lambda: asyncio.create_task(shutdown(loop)))
    loop.set_exception_handler(handle_exception)

    loop.create_task(daemon.run())
    loop.create_task(server.periodical_update(2.))

    if is_up:
        loop.create_task(dummy.run_gst())
    else:
        loop.create_task(dummy.run())
    loop.create_task(prompt.run())

    try:
        asyncio.get_event_loop().run_forever()
    finally:
        daemon.close()
        print("...shutdown")
