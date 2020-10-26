Based on WFB [https://github.com/svpcom/wifibroadcast]

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
