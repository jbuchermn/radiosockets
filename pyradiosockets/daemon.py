import os
import socket
import ctypes
import signal
from subprocess import Popen, PIPE

from .pcap_conf import pcap_conf_compile
from .rs_message import (
    rs_message_recv, rs_message_send, Message,
    RS_MESSAGE_CMD_EXIT, RS_MESSAGE_CMD_REPORT, RS_MESSAGE_CMD_SWITCH_CHANNEL)

CMD_REPORT_N = 11

def cmd(cmd):
    p = Popen(cmd.split(), stdout=PIPE)
    return p.communicate()[0].decode()


class Daemon:
    def __init__(self, conf_template, own_id, other_id, debug=""):
        self.conf_template = conf_template
        self.own_id = own_id
        self.other_id = other_id

        # valgrind / gdb
        self.debug = debug

        self.socket = "/tmp/radiosocketd_%d.sock" % os.getpid()
        self.conf = "/tmp/radiosocketd_%d.conf" % os.getpid()

        self.daemon = None
        self._cmd_id = 0

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

        for l in cmd("ps ax").split("\n"):
            if './radiosocketd' in l:
                pid = int(l.split(None, 1)[0])
                print("Found zombie process: %d" % pid)
                if input("Kill? [Y/n] ").strip() not in ["n", "N"]:
                    os.kill(pid, signal.SIGINT)

        input("Confirm? ")
        self.daemon = Popen(("sudo %s ./radiosocketd -v -c %s -s %s" %
                             (self.debug, self.conf, self.socket)).split())

    def close(self):
        self.cmd_close()
        self.daemon.communicate()

    def _cmd(self, cmd, payload_int=[], payload_char="", payload_double=[]):
        self._cmd_id += 1
        msg = Message(self._cmd_id, cmd, payload_int,
                      payload_char, payload_double)

        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            s.connect(self.socket)

            rs_message_send(s, msg)
            answer = rs_message_recv(s)

            if answer is None:
                return None

            if msg.id != answer.id:
                print("ERROR: Id mismatch")
            else:
                return answer
        except Exception as e:
            print("ERROR: %s" % e)
        finally:
            s.close()

        return None

    def cmd_json(self, json):
        if 'cmd_id' in json:
            msg = self._cmd(json['cmd_id'])
            return {
                'int': msg.payload_int,
                'char': msg.payload_char,
                'double': msg.payload_double
            }
        elif json['cmd'] == 'report':
            return self.cmd_report()
        elif json['cmd'] == 'switch':
            return self.cmd_switch_channel(json['port'], json['new_channel'])

    def cmd_report(self):
        msg = self._cmd(RS_MESSAGE_CMD_REPORT)
        if msg is None:
            return []

        n = msg.payload_int
        s = msg.payload_char
        d = msg.payload_double
        return [{
            'title': '%s%i' % (s[i], n[i]),
            'stats': d[CMD_REPORT_N*i:CMD_REPORT_N*(i+1)]
        } for i, _ in enumerate(s)]

    def cmd_switch_channel(self, port, new_channel):
        msg = self._cmd(RS_MESSAGE_CMD_SWITCH_CHANNEL, [port, new_channel])
        return msg.cmd if msg is not None else -1

    def cmd_close(self):
        self._cmd(RS_MESSAGE_CMD_EXIT)
