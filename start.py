import os
import socket
import sys
import time
import ctypes
from subprocess import Popen, PIPE

CONFIG_SOCKET = "/tmp/radiosocketd.sock"


def cmd(cmd):
    p = Popen(cmd.split(), stdout=PIPE)
    return p.communicate()[0].decode()


class PhysicalDevice:
    def __init__(self, idx):
        self.idx = idx
        self.interfaces = []
        self.is_connected = False
        self.chipset = ""
        self.driver = ""


def retrieve_physical_devices():
    phys = []

    iw_dev = cmd("iw dev").split("\n")
    cur_phys = None
    for l in iw_dev:
        if len(l) < 2:
            continue

        if l[0] != '\t':
            if cur_phys is not None:
                phys += [cur_phys]
            cur_phys = PhysicalDevice(int(l.split("#")[1]))
        elif l[0] == '\t' and l[1] != '\t':
            ifname = l.strip().split(" ")
            ifname = ifname[len(ifname) - 1]
            cur_phys.interfaces += [ifname]
        else:
            if 'ssid' in l:
                cur_phys.is_connected = True
    if cur_phys:
        phys += [cur_phys]

    for p in phys:
        for i in p.interfaces:
            try:
                p.driver = cmd(
                    "ls /sys/class/net/%s/device/driver/module/drivers" % i)
            except:
                pass

    return phys


CMD_PAYLOAD_MAX = 100
CMD_PORT_STAT = 1
CMD_EXIT = 13


class CommandPayload(ctypes.Structure):
    _pack_ = 1
    _fields_ = [("id", ctypes.c_uint32),
                ("command", ctypes.c_uint32),
                ("payload_int", ctypes.ARRAY(ctypes.c_int, CMD_PAYLOAD_MAX)),
                ("payload_char", ctypes.ARRAY(ctypes.c_char, CMD_PAYLOAD_MAX)),
                ("payload_double", ctypes.ARRAY(ctypes.c_double, CMD_PAYLOAD_MAX))]

    def __init__(self, command, payload_int, payload_char, payload_double):
        self.id = 0
        self.command = command

        while len(payload_int) < CMD_PAYLOAD_MAX:
            payload_int += [0]
        self.payload_int = (ctypes.c_int * 100)(*payload_int)

        self.payload_char = payload_char.encode("ascii")

        while len(payload_double) < CMD_PAYLOAD_MAX:
            payload_double += [0.0]
        self.payload_double = (ctypes.c_double * 100)(*payload_double)


class ResponsePayload(ctypes.Structure):
    _pack_ = 1
    _fields_ = [("id", ctypes.c_uint32),
                ("payload_int", ctypes.ARRAY(ctypes.c_int, CMD_PAYLOAD_MAX)),
                ("payload_char", ctypes.ARRAY(ctypes.c_char, CMD_PAYLOAD_MAX)),
                ("payload_double", ctypes.ARRAY(ctypes.c_double, CMD_PAYLOAD_MAX))]

    def get_payload_int(self):
        return [self.payload_int[i] for i in range(CMD_PAYLOAD_MAX)]

    def get_payload_char(self):
        return self.payload_char.decode("ascii")

    def get_payload_double(self):
        return [self.payload_double[i] for i in range(CMD_PAYLOAD_MAX)]


class SocketConnection:
    def __init__(self):
        self._id = 0

    def command(self, command):
        self._id += 1
        command.id = self._id

        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            s.connect(CONFIG_SOCKET)
            s.send(command)
            buff = s.recv(ctypes.sizeof(ResponsePayload))
            response = ResponsePayload.from_buffer_copy(buff)

            if response.id != command.id:
                print("ERROR: Id mismatch")
                return None

            return response
        except Exception as e:
            print("ERROR: %s" % e)
        finally:
            s.close()

        return None


if __name__ == '__main__':
    os.chdir(os.path.dirname(os.path.realpath(__file__)))
    os.system('make')

    phys = retrieve_physical_devices()

    print("\n\n")
    print("Available devices:")
    phys = [p for p in phys if not p.is_connected]
    for i, p in enumerate(phys):
        print("%d: phys=%d %s %s driver=%s" %
              (i, p.idx, ', '.join(p.interfaces), p.chipset,
               p.driver))

    if len(phys) == 0:
        print("No available devices...")
        exit(0)
    if len(phys) == 1:
        print("Selecting only available device...")
        phys = phys[0]
    else:
        phys = phys[int(input("Index? "))]

    arg_p = phys.idx
    arg_default_channel = "0x1006"
    arg_own = "0xDD00" if 'pi-up' in os.uname()[1] else "0xFF00"
    arg_other = "0xFF00" if 'pi-up' in os.uname()[1] else "0xDD00"
    arg_ifname = "wlan%dmon" % phys.idx
    if "8188eu" in phys.driver:
        print("Detected patched 8188eu driver => using existing interface...")
        if len(phys.interfaces) != 1:
            print("Unexpected: %s" % phys.interfaces)
            exit(1)
        arg_ifname = phys.interfaces[0]

    cmd = "sudo ./radiosocketd -p %d -i %s -a %s -b %s -c %s" % (
        arg_p, arg_ifname, arg_own, arg_other, arg_default_channel)
    print("Executing %s" % cmd)
    input("Yes? ")

    daemon = Popen(cmd.split())

    time.sleep(1)
    connection = SocketConnection()

    try:
        while True:
            response = connection.command(
                CommandPayload(CMD_PORT_STAT, [0], "", []))
            if response is not None:
                p = response.get_payload_double()
                print("TX=%7.2fkbps (reported: %7.2fkbps with loss: %2d%%) RX=%7.2fkbps with loss: %2d%%" %
                      (p[0]/1000, p[1]/1000, p[2] * 100, p[3]/1000, p[4] * 100))

            time.sleep(0.5)
    finally:
        try:
            connection.command(CommandPayload(CMD_EXIT, [0], "", []))
        except:
            pass


