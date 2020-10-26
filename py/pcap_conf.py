from subprocess import Popen, PIPE


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


def _retrieve_physical_devices():
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


def pcap_conf_compile():
    phys = _retrieve_physical_devices()

    print("\n\n")
    print("Available devices:")
    phys = [p for p in phys if not p.is_connected]
    for i, p in enumerate(phys):
        print("%d: phys=%d %s %s driver=%s" %
              (i, p.idx, ', '.join(p.interfaces), p.chipset,
               p.driver))

    if len(phys) == 0:
        print("No available devices...")
        exit(1)
    if len(phys) == 1:
        print("Selecting only available device...")
        phys = phys[0]
    else:
        phys = phys[int(input("Index? "))]

    arg_ifname = "wlan%dmon" % phys.idx
    if "8188eu" in phys.driver or "rtl88XXau" in phys.driver:
        print("Detected aircrack-ng driver => using existing interface...")
        if len(phys.interfaces) != 1:
            print("Unexpected: %s" % phys.interfaces)
            exit(1)
        arg_ifname = phys.interfaces[0]

    for l in cmd("ps ax").split("\n"):
        if 'networkman' in l.lower():
            print("WARNING! NetworkManager is running! This is usually not a " + 
                  "good idea, as randomly monitor mode will stop working on " +
                  "some cards. Be sure to either stop NetworkManager or have " +
                  "it ignore the devices in use")
            input("Okay? ")

    return {"<phys/>": phys.idx, "<ifname/>": arg_ifname}
