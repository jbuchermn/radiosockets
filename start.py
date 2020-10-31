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
    if len(sys.argv) > 1 and "fake-pi" in " ".join(sys.argv[1:]):
        is_pi = True

    sleep_s = 1. / 120. if is_pi else 1. / 60.
    kbits = 50000 if is_pi else 1000
    frame_size = int(1000 * kbits / 8 * sleep_s)

    if is_pi:
        arg_own = "0xDD"
        arg_other = "0xAA"
    else:
        arg_own = "0xAA"
        arg_other = "0xDD"
    data_addr = ('127.0.0.1', 8885) if is_pi else ('127.0.0.1', 8881)

    if len(sys.argv) > 1 and "vg" in " ".join(sys.argv[1:]):
        daemon = Daemon("./basic.conf", arg_own, arg_other,
                        "valgrind --leak-check=full --track-origins=yes -s")
    else:
        daemon = Daemon("./basic.conf", arg_own, arg_other)
    daemon.start()

    server = None
    if len(sys.argv) > 1 and "gui" in " ".join(sys.argv[1:]):
        server = Webserver(daemon)
        server.start()

    try:
        time.sleep(2)

        data_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            data_socket.connect(data_addr)
        except Exception as e:
            print("Could not connect to TCP socket: %s" % e)

        print("%dkbps => frame size %db" % (kbits, frame_size))
        data_msg = "a" * frame_size

        cnt = 0
        while True:
            try:
                s = time.time()
                data_socket.send(data_msg.encode('ascii'))
                s = time.time() - s
                if s > 0.001:
                    print("WARNING: Sending frame took %fms" % (s * 1000))
            except Exception as e:
                print("Could not send to TCP socket: %s" % e)
                time.sleep(1)
                try:
                    data_socket = socket.socket(
                        socket.AF_INET, socket.SOCK_STREAM)
                    data_socket.connect(data_addr)
                except Exception as e:
                    print("Could not connect to TCP socket: %s" % e)

            if server is None:
                cnt += 1
                if cnt % int(1 / sleep_s) == 0:
                    b = 0
                    print("-----------------------------------")
                    stat = daemon.cmd_report()
                    for st in stat:
                        print("%10s: %s" % (st['title'], st['stats']))

            time.sleep(sleep_s)
    finally:
        daemon.close()
        if server is not None:
            server.stop()
