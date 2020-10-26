# RadioSockets

Based on (WFB)[https://github.com/svpcom/wifibroadcast], intended as an abstraction to encompass a wider variety of
transmission methods

## Devices and drivers
* rtl8188eus: (8188eu)[https://github.com/aircrack-ng/rtl8188eus] (patched)
    - Be careful to only use the one virtual interface that is initally created, in order to prevent kernel-freeze upon
      deletion of other interfaces
* rtl8812au:  (rtl88xxau)[https://github.com/aircrack-ng/rtl8812au] (patched)
    - Be careful to only use the one virtual interface that is initally created, in order to prevent kernel-freeze upon
      deletion of other interfaces
* rtl8192cu:  rtl8xxxu (original kernel module)
* rtl8192eu:  rtl8xxxu (original kernel module)
* mt7601u:    mt7601u (original kernel module)
* rt5572:     rt2800usb (original kernel module)
    - Creating a new interface works well, the originally created one tends to return "Device or resource busy"
