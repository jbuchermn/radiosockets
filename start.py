import os
from subprocess import Popen, PIPE


def cmd(cmd):
    p = Popen(cmd.split(), stdout=PIPE)
    return p.communicate()[0].decode()


class PhysicalDevice:
    def __init__(self, idx):
        self.idx = idx
        self.interfaces = []
        self.is_connected = False
        self.chipset = None
        self.driver = None


if __name__ == '__main__':
    os.chdir(os.path.dirname(os.path.realpath(__file__)))
    os.system('make')

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

    lshw = cmd("lshw -c net").split("\n")
    cur_logname = None
    cur_conf = None
    cur_bus = None
    for l in [*lshw, "  *"]:
        if len(l) < 2:
            continue
        if l[2] == "*":
            if cur_logname:
                ph = [p for p in phys if cur_logname in p.interfaces]
                if len(ph) == 1:
                    ph = ph[0]
                    ph.driver = cur_conf
                    ph.chipset = cur_bus

            cur_logname = None
            cur_conf = None
            cur_bus = None
        else:
            if 'logical name' in l:
                cur_logname = ':'.join(l.split(':')[1:]).strip()
            elif 'configuration' in l:
                cur_conf = ':'.join(l.split(':')[1:])
            elif 'bus info' in l:
                cur_bus = ':'.join(l.split(':')[1:])

    print("\n\n")
    print("Available devices:")
    phys = [p for p in phys if not p.is_connected]
    for p in phys:
        print("* %d: %s %s%s" %
              (p.idx, ', '.join(p.interfaces), p.chipset,
               '\n\t'.join(p.driver.split(" "))))

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
    if "driver=8188eu" in phys.driver:
        print("Detected patched 8188eu driver => using existing interface...")
        if len(phys.interfaces) != 1:
            print("Unexpected: %s" % phys.interfaces)
            exit(1)
        arg_ifname = phys.interfaces[0]

    cmd = "sudo ./radiosocketd -p %d -i %s -a %s -b %s -c %s" % (
        arg_p, arg_ifname, arg_own, arg_other, arg_default_channel)
    print("Executing %s" % cmd)
    input("Yes? ")
    os.system(cmd)
