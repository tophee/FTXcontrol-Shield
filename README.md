# FTXcontrol-Shield
An Arduino shield for controlling the FläktWoods RDKR ventilation unit

In this project, I'm monitoring and controlling the ventilation of my house. My ventilation unit is a **FläktWoods RDKR** (which as a rotating heat exchanger, hence FTX) but it should be possible to use at least parts of this project with other systems.

**Please note** that this repository does not yet include all the information that it should include. I will remove this line once at least a first version of everything is uploaded.

## Features
* Monitor and log temperature and humidity of 
  * outside air (_uteluft_)
  * incoming air (_tilluft_)
  * inside air (_frånluft_)
  * outgoing air (_avluft_)
* Monitor the efficiency of the heat (and humidity) exchanger.
* Automatically change ventilation state (low, middle, high) based on temperature and/or humidity values. For example: 
  * Set ventilation to low when outside temperature is higher than inside temperature (Don't have the ventilation heat up the house on hot summer days.)
  * Set venilation to high when oustide temperature is lower than inside temperature and insider temperature is over 25°C. (Cool down the house during the night.)
* Log all values 
  * to either a csv file (using [processing](https://github.com/processing/processing) to read the data from the Arduino serial port) or
  * to an [InfluxDB](https://github.com/influxdata/influxdb) database (using [Petr Pudlak's python script](https://github.com/ppetr/arduino-influxdb) to read the serial port and send the data to the database).
* Display values on an LCD display on the ventilation unit (currently not quite implemented)
* Manually control the ventilation state via a button

## Hardware used
* 1 Arduino Uno
* 3-4 [Sonoff Si7021](https://www.itead.cc/wiki/Sonoff_Sensor_Si7021) temperature and humidity sensors
* 1 PCB prototyping board with at least 14 x 21 holes (to start with, you can of course use a breadboard)
* Various electronic components to build the shield (see schematic). Most importantly:
  * 1 photoresistor for reading the current ventilation state via the LED on the ventilation unit's control board
  * 1 optocoupler to switch the ventilation state on the control board
* 1 LCD display


## The schematic
This is the schematic of the final board:
![](/images/schematic.jpg)

There are five basic sets of components:

### The photoresistor 
On the left: R1, J1, R2

**R1**: photoresistor [PGM5506](https://www.electrokit.com/uploads/productfile/40850/ldr_en5cds.pdf) 18-50 kohm. 
**J1**: female pinheader (2 pins)
**R2**: 1k or 10k resistor (in the schematic and the photos you see 1k but I think 10k works better because it uses a larger voltage range.

The two resistors form a voltage divider and the Arduino reads the voltage corresponding to the photresistors current resistance (which varies based on the amount of light it receives. It receives more light when the LED closest to it is lit and least light when the LED furthest away from it is lit).


### The button
On the bottom: SW1 C1, R3

**SW1**: some push button
**C1**: 0.1µF capacitor for [debouncing](https://www.thegeekpub.com/246471/debouncing-a-switch-in-hardware-or-software/) the button. The capacitor can be ommitted if debouncing is done softwarewise, i.e. in [the Arduino sketch](https://github.com/tophee/FTXcontrol-Shield/blob/master/FTXcontrol-shield.ino).
**R3**: 10k pull-down resistor to prevent pin 2 from [floating](https://www.arduino.cc/en/Tutorial/DigitalPins).

The button is not strictly necessary. I used it mainly for testing and debugging purposes and left it there to eventually control the display and possibly some more with it. Currently it just switches to the next ventilation state, just like when pressing the button on the ventilation system's control panel.

### The sensors
On the lower right: J3, U2, U3, U4 (U1 is missing because I'm currently only using 3 sensors)

**J3**: female pinheader (12 pins)
**U2-4**: [Sonoff Si7021](https://www.itead.cc/wiki/Sonoff_Sensor_Si7021). Each sensor is connected to ground, 3.3V and a digital pin. If I'd do it again, I'd put the 3.3V pin in the middle, not the data pin.

The Sonoff sensor comes with a cable and a 2.5mm headphone plug but I'm not using those because the cable is too thick to lead through the door of the ventilation unit. I soldered thinner wires onto the sensors board. The sensors should be calibrated against each other, but I haven't finished [that procedure](https://thecavepearlproject.org/2016/03/05/ds18b20-calibration-we-finally-nailed-it/) yet.

### The display connector
Further up on the right: J2

At some point I had a 16x2 LCD display connected to show me the sensor values and the code is still in the [Arduino sketch](https://github.com/tophee/FTXcontrol-Shield/blob/master/FTXcontrol-shield.ino) but I'm currenltly not using it but the pins are there. They can also be used for something else.

### The optocoupler
Top right: R4, R5, U5, J4

R4: 1k resistor to prevent short-circuiting the control board of the RDKR unit.
R5: 1k resistor to prevent short-circuiting the Arduino board
U5: [PC817X DIP-4](https://www.electrokit.com/uploads/productfile/40300/sf-00061657.pdf) optocoupler

This connects the Arduino to the ventilation control board while keeping the two galvanically separate. In order to switch to the next ventilation state, two pins on the control board need to be connected to each other. It can be done with a mechanical switch but since the whole point here is to allow the Arduino to control the ventilation, an optocoupler is used. When voltage is applied to one side of the optocoupler, the pins on the other side are shortened.

## The PCB

This is how I fitted it on my PCB with 14 x 21 holes:

### Front:

![](/images/board_front.jpg)

_Legend:_
* Green lines are connections on the back (bootom) of the board
* Red lines are connections on the front (top) of the board
* White circles are vias, i.e. connections between the front and the back of the board
* White lines can be ignored
* The yellow circles at the top and bottom represent the Arduino pins.
* The yellow rectangle represents the edge of the PCB. So the board is not covering all pins (because that's the size board I happened to have).

To connect the shield to the arduino, header pins are soldered onto the back of the PCB, making it easy to connect to these on the front.

![](/images/board_photo1.jpg)

![](/images/board_photo2.jpg)

### Back:
![](/images/board_back.jpg)


