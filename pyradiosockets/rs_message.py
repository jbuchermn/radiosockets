import ctypes

RS_MESSAGE_CMD_REPORT = 1
RS_MESSAGE_CMD_REPORT_N = 11

RS_MESSAGE_CMD_SWITCH_CHANNEL = 2
RS_MESSAGE_CMD_UPDATE_PORT = 3
RS_MESSAGE_CMD_EXIT = 13


class Message:
    def __init__(self, id_, cmd, payload_int=[], payload_char="", payload_double=[]):
        self.id = id_
        self.cmd = cmd
        self.payload_int = payload_int
        self.payload_char = payload_char
        self.payload_double = payload_double


class _MessageHeader(ctypes.Structure):
    _pack_ = 1
    _fields_ = [("id", ctypes.c_int),
                ("cmd", ctypes.c_int),
                ("len_payload_int", ctypes.c_uint16),
                ("len_payload_char", ctypes.c_uint16),
                ("len_payload_double", ctypes.c_uint16)]


def rs_message_recv(socket_fd):
    try:
        buf = socket_fd.recv(ctypes.sizeof(_MessageHeader))
        if len(buf) < ctypes.sizeof(_MessageHeader):
            return None
        header = _MessageHeader.from_buffer_copy(buf)

        payload_int = []
        payload_char = ""
        payload_double = []

        if header.len_payload_int > 0:
            payload_int_t = ctypes.ARRAY(ctypes.c_int, header.len_payload_int)
            buf = socket_fd.recv(ctypes.sizeof(payload_int_t))
            payload_int = [i for i in payload_int_t.from_buffer_copy(buf)]

        if header.len_payload_char > 0:
            payload_char_t = ctypes.ARRAY(
                ctypes.c_char, header.len_payload_char)
            buf = socket_fd.recv(ctypes.sizeof(payload_char_t))
            tmp = payload_char_t.from_buffer_copy(buf)
            payload_char = ""
            for c in tmp:
                payload_char += c.decode("ascii")

        if header.len_payload_double > 0:
            payload_double_t = ctypes.ARRAY(
                ctypes.c_double, header.len_payload_double)
            buf = socket_fd.recv(ctypes.sizeof(payload_double_t))
            payload_double = [
                i for i in payload_double_t.from_buffer_copy(buf)]

        return Message(header.id, header.cmd,
                       payload_int, payload_char, payload_double)

    except Exception as e:
        print(e)
        return None


def rs_message_send(socket_fd, msg):
    try:
        payload_int = (ctypes.c_int * len(msg.payload_int))(*msg.payload_int)
        payload_double = (ctypes.c_double *
                          len(msg.payload_double))(*msg.payload_double)
        payload_char = msg.payload_char.encode("ascii")

        hdr = _MessageHeader(
            id=msg.id, cmd=msg.cmd,
            len_payload_int=len(msg.payload_int),
            len_payload_double=len(msg.payload_double),
            len_payload_char=len(msg.payload_char))

        socket_fd.send(hdr)
        socket_fd.send(payload_int)
        socket_fd.send(payload_char)
        socket_fd.send(payload_double)
    except Exception as e:
        print(e)
