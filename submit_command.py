import socket
import sys
import ctypes

class CommandPayload(ctypes.Structure):
    _fields_ = [("id", ctypes.c_uint32),
                ("command", ctypes.c_uint32)]

class ResponsePayload(ctypes.Structure):
    _fields_ = [("id", ctypes.c_uint32)]


CONFIG_SOCKET = "socket"

def send_command(id_, command):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    if s is None:
        print("ERROR: Could not create socket")
        return None

    try:
        s.connect(CONFIG_SOCKET)
        payload_out = CommandPayload(id_, command)
        print("Sending id=%d, command=%d..." % (payload_out.id,
                                                payload_out.command))

        nsent = s.send(payload_out)
        print("...sent %d bytes" % nsent)

        buff = s.recv(ctypes.sizeof(ResponsePayload))
        payload_in = ResponsePayload.from_buffer_copy(buff)
        print("...received payload")
        
        if payload_in.id != payload_out.id:
            print("ERROR: Id mismatch")
            return True
    except Exception as e:
        print("ERROR: %s" % e)
        return None
    finally:
        s.close()



def main():
    for i in range(15):
        send_command(i, i)



if __name__ == "__main__":
    main()
