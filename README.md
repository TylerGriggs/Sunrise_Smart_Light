# Sunrise Smart Light
Arduino WIFI Clock to animate LEDs with the sunrise.
Based off the NTP Client sketch provided by Arduino, and using the Dusk2Dawn library - modified to run on ESP12E.

![A 3D -printed rendering of the Sunrise Smart Light on a table at the beach during a sunrise.](https://github.com/TylerGriggs/Sunrise_Smart_Light/blob/main/SunriseSmartLight1.jpg?raw=true)

# Materials
- ESP-12E (and Micro-USB cable)
- LED Square 8x8
- OLED IC2 32x128 Screen
- Push-Button Microswitch
- 3D Printing Filament

###### Startup Sequence - When powered on
1. Connect to WIFI 
   - Flashes yellow while connecting, then Flashes green when connected.
2. Get NTP Time Online
   - Requests the number of seconds since the Epoch. We know what day it is now.
3. Calculate Time of Sunrise Today
   - Using Dusk2Dawn library, availible through Arduino project manager
4. Get Time from Midnight Today
   - Check if it's past time to turn on for today, if so, begin the sequence.

###### Sunrise Sequence - Once a minute, each minute of the sequence
   - (255 / Total Minutes in Sequence) * Current Minute in Sequence
   - Brightness of RED is increased linearly, (0 < RED < 255)
   - Brightness of GREEN (our yellow) increases with 0.002x^2, (0 < GREEN < 130)
     - Resulting color shifts from red to bright yellow over the sequence
     ![A graph showing the linear increase of the color red, and an exponential but scaled down increase of the color green.](https://github.com/TylerGriggs/Sunrise_Smart_Light/blob/main/ColorGraph1.jpg?raw=true)

# Print Settings and Instructions
###### Top (for translusent effect using just white)
- Wall Count:         1
- Top Layer Count:    12
Since the top is printed upside down, the top will be connecting to the base.
- Bottom Layer Count: 4
- Infill:             10%
- Infill Pattern:     Cubic
- Brim (In/Out):      8
Trim the outer brim, and optionally leave the inner brim for softly lit edge.

###### Base
Whatever settings you prefer, I used:
- Layer Height: 0.3mm
- Infill: 30%
