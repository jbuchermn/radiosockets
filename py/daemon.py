import os
import socket
import ctypes
import signal
from subprocess import Popen, PIPE

from .pcap_conf import pcap_conf_compile

CMD_PAYLOAD_MAX = 100

CMD_REPORT = 1
CMD_SWITCH_CHANNEL = 2
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


class Daemon:
    def __init__(self, conf_template, own_id, other_id):
        self.conf_template = conf_template
        self.own_id = own_id
        self.other_id = other_id

        self.socket = "/tmp/radiosocketd_%d.sock" % os.getpid()
        self.conf = "/tmp/radiosocketd_%d.conf" % os.getpid()

        self.daemon = None
        self.cmd_id = 0

    def start(self):
        args = pcap_conf_compile()
        args['<own/>'] = self.own_id
        args['<other/>'] = self.other_id

        with open(self.conf_template, "r") as f:
            conf = f.read()
            for a in args:
                conf = conf.replace(a, str(args[a]))

            print("------------ Configuration ------------\n")
            print(conf)
            print("---------------------------------------\n")

            with open(self.conf, "w") as outf:
                outf.write(conf)

        input("Confirm? ")

        # TODO: Check if zombies are running
        self.daemon = Popen(("sudo ./radiosocketd -v -c %s -s %s" %
                             (self.conf, self.socket)).split())

    def close(self):
        self.cmd_close()
        self.daemon.communicate()

    def cmd(self, cmd_id, payload_int=[], payload_char="", payload_double=[]):
        self.cmd_id += 1

        command = CommandPayload(
            cmd_id, payload_int, payload_char, payload_double)
        command.id = self.cmd_id

        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        resp = None
        try:
            s.connect(self.socket)
            s.send(command)
            buff = s.recv(ctypes.sizeof(ResponsePayload))
            response = ResponsePayload.from_buffer_copy(buff)

            if response.id != command.id:
                print("ERROR: Id mismatch")
            else:
                resp = response
        except Exception as e:
            print("ERROR: %s" % e)
        finally:
            s.close()

        try:
            return resp.get_payload_int(), resp.get_payload_char(), resp.get_payload_double()
        except:
            return [], "", []

    def cmd_report(self):
        n, s, d = self.cmd(CMD_REPORT)
        return [{
            'title': '%s%i' % (s[i], n[i]),
            'stats': d[10*i:10*(i+1)]
        } for i, _ in enumerate(s)]

    def cmd_close(self):
        self.cmd(CMD_EXIT)
