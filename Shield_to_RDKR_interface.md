# How to connect the shield to the FläktWoods RDKR control board?

The (low voltage side of the) control board of the ventilation unit looks like this:

![](/images/RDKR_board.png)

The two important parts for us are

1. The LEDs D6, D7, and D8 at the top centre of the image. These indicate low, medium, and high ventilation state respectively.
2. The connector marked "Door SWC" on the bottom left. It is a Phoenix connector with 3.5mm or 3.81mm pitch. I have also seen it as "KF2EDG" but I'm not sure whether that is a generic name or connected to some brand. The 3.5 mm onses seem to be less common so I haven't been able to buy one yet. But there are other ways of connecting...

![](/images/RDKR_switchplug.png)

The middle pin holds 20V against the other two (which seem to be identical). When the middle pin is connected to either of the outer pins (via a resistor!), the ventilation state goes one state up (continuously circulating thorugh low, normal, high, low, normal high, ...), just like when pressing the button on the official FläktWoods control panel.

## Changing the ventilation state

* Connect the middle pin to pin 1 of the J4 connector on the shield. That's the one with the resistor (R5). 
* Connect one of the other pins to the other J4 pin. 

## Reading the current ventilation state

In order for the arduino to reliably set the ventilation state, it needs to know what the current ventilation state is. Because I don't know where on the RDKR board I might be able to read the current state, I am using the LED indicators instead.

By glueing the photoresistor to the board as shown beloa and connecting it to the Arduino pins A0 and A1 (or 5V), the Arduino can reliably [distinguish which of the three LEDs](https://github.com/tophee/FTXcontrol-Shield/blob/master/FTXcontrol-shield.ino#L396-L412) is on and hence [determine the current ventilation state](https://github.com/tophee/FTXcontrol-Shield/blob/master/FTXcontrol-shield.ino#L415-L442). I'm using pin A1 as a power supply instead of constant 5V because I thought it was a waste to have current flowing through those reistors all the time when the actual measurements are needed much more rarely (I'm [checking the ventilation state once a minute](https://github.com/tophee/FTXcontrol-Shield/blob/d6335a8353760f6a1a2ead0d1810f92687e4eb36/FTXcontrol-shield.ino#L199-L203), but even that is not really necessary). 

![](/images/RDKR_photoresistor.jpg)
