import os
import sys
import time
import socket
from pyradiosockets.daemon import Daemon
from radiosocketgui.webserver import Webserver

if __name__ == '__main__':
    os.chdir(os.path.dirname(os.path.realpath(__file__)))
    os.system('make')

    is_pi = 'pi' in os.uname()[1]
    if len(sys.argv) > 1 and sys.argv[1] == "fake-pi":
        is_pi = True

    sleep_s = 0.01
    if is_pi:
        arg_own = "0xDD"
        arg_other = "0xAA"
    else:
        arg_own = "0xAA"
        arg_other = "0xDD"

    daemon = Daemon("./basic.conf", arg_own, arg_other)
    daemon.start()

    print("UDP datarate: %5.2fMbps" % (0.008 / sleep_s))

    webserver = None
    if len(sys.argv) > 1 and sys.argv[1] == "gui":
        server = Webserver(daemon)
        server.run()

    try:
        time.sleep(2)
        data_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        data_addr = ('127.0.0.1', 8885) if is_pi else ('127.0.0.1', 8881)
        data_msg = "a" * 1024

        cnt = 0
        while True:
            data_socket.sendto(data_msg.encode('ascii'), data_addr)

            cnt += 1
            if cnt % int(1 / sleep_s) == 0:
                print("-----------------------------------")
                stat = daemon.cmd_report()
                for st in stat:
                    print("%10s: %s" % (st['title'], st['stats']))

            time.sleep(sleep_s)
    finally:
        daemon.close()
        if server is not None:
            server.stop()
