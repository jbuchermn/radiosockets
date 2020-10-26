import os
import sys
import time
import socket
from py.daemon import Daemon

if __name__ == '__main__':
    os.chdir(os.path.dirname(os.path.realpath(__file__)))
    os.system('make')

    is_pi = 'pi' in os.uname()[1]
    if len(sys.argv) > 1 and sys.argv[1] == "fake-pi":
        is_pi = True

    if is_pi:
        arg_own = "0xDD"
        arg_other = "0xAA"
    else:
        arg_own = "0xAA"
        arg_other = "0xDD"

    d = Daemon("./basic.conf", arg_own, arg_other)
    d.start()

    try:
        time.sleep(2)
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        addr = ('127.0.0.1', 8885)
        msg = "a" * 1024
        while True:
            s.sendto(msg.encode('ascii'), addr)

            # print("\e[1;1H\e[2J\n");
            print("-----------------------------------")
            stat = d.cmd_report()
            for st in stat:
                print("%10s: %s" % (st['title'], st['stats']))

            time.sleep(0.5)
    finally:
        d.close()
