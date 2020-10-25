Based on WFB

Devices and status
* rtl8188eus: [https://github.com/aircrack-ng/rtl8188eus] (8188eu)
    - Be careful to only use the one virtual interface that is initally created, in order to prevent kernel-freeze upon
      deletion of other interfaces
    - Monitor mode unstable, packet injection yes
* rtl8812au:  [https://github.com/aircrack-ng/rtl8812au] (rtl88xxau)
    - Be careful to only use the one virtual interface that is initally created, in order to prevent kernel-freeze upon
      deletion of other interfaces
    - Monitor mode unstable, packet injection yes
* rtl8192cu:  rtl8xxxu / possibly other driver?
    - Monitor mode yes, packet injection ?
* rtl8192eu:  rtl8xxxu
    - Monitor mode yes, packet injection ?
* rt5572:     rt2800usb
    - Create new interface, the one originally created returns "Device or resource busy"
    - Monitor mode yes, packet injection yes
* mt7601u:    mt7601u
    - Monitor mode yes, packet injection yes

Experiments:
    + up         -> down
    + mt7601u    -> rtl8188eus: downstream broke, upstream worked
    + rtl8192eu  -> rtl8188eus: downstream didn't start, upstream worked
    + rtl8192cu  -> rtl8188eus: downstream didn't start, upstream worked
    + rtl8812au  -> rtl8188eus: downstream broke, upstream broke
    + rt5572     -> rtl8188eus: downstream broke, upstream worked 
    + rtl8192cu  -> rtl8812au:  downstream didn't start, upstream worked
    + mt7601u    -> rt5572:     downstream worked, upstream worked

Hypothesis
    +  8188eu driver: continuous tx capabilities, rx breaks
    +  rtl88XXau driver: ?
    +  rt8xxxu driver does not have tx capabilities, no rx problems - however works with aireplay?!
    +  rt2800usb and mt7601u work both ways
