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

    mod = 1000
    sleep_s = 0.001

    if is_pi:
        mod = 500
        sleep_s = 0.002

    try:
        time.sleep(2)
        data_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        addr = ('127.0.0.1', 8885) if is_pi else ('127.0.0.1', 8881)
        msg = "a" * 1024

        c = 0
        while True:
            data_socket.sendto(msg.encode('ascii'), addr)

            c += 1
            if c%mod == 0:
                # print("\e[1;1H\e[2J\n");
                print("-----------------------------------")
                stat = d.cmd_report()
                for st in stat:
                    print("%10s: %s" % (st['title'], st['stats']))

            time.sleep(sleep_s)
    finally:
        d.close()
