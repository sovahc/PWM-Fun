# PWM-Fun
PWM fan controller for water cooled PC
![screenshot](photo.jpg)

I didn't find the fan controller cheap and cute, so I made it myself.

For building minimal LCD version of this project you need:
* PCB (use Kicad to export gerber files for manufacturing)
* 170x320 SPI LCD (they are cheap and slow)
* ATMega328 microcontroller
* 2.54mm single row 8 pin male + female connector (I recommend short L7.5 version)
* 2x SMD 10k resistor
* 7x SMD 220 Ohm resistor
* SMD 100 nF capacitor
* SMD(or not) 4.7 uF capacitor
* Any AVR programmer (You can use any usb arduino board with sketch ArduinoISP, don't forget to press "Upload Using Programmer")
* Good soldering iron recomended.
* M2.5 screws and nuts for mounting.
* SATA to Molex adapter.

You can add buzzer and 500 Ohm resistor if you like.
