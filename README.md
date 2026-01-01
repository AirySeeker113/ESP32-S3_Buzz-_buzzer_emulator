# ESP32-S3_Buzz-_buzzer_emulator
A usb emulator that emulates the PS2 version of Buzzers

## How to use
After you built or flashed the project (search for tutorials online since im not going to go into detail) you will connect on your device of choice to the AP created by the ESP32 which is **"Buzz_Emulator_AP"** and then go to **192.168.4.1** you should see the buzzer ui. 

If you don't see it, check if your cable or device that the ESP is plugged into can supply enough current. You can use it even without connecting to wifi.

## How to connect to wifi
If you want to connect to wifi, go to the adress bar and add "/debug" to the url. It should look like this: `http://192.168.4.1/debug`. 

Now you've entered the debug menu where you should have the two wifi text fields called **SSID** and **PASS**. In the ssid field you will enter the network name, and pass the password. 

After you've done that you will unplug and plug the esp32 again or reset via the button. Now connect to the AP hosted by the ESP32 and go back to the debug menu where you should see the ip. Now connect back to your home wifi and enter that ip address in the adress bar of the browser. Voila now you have a buzzer system.

## Compatability
It works on all devices, even on original consoles. I tried it on my PS2 and it worked. So the possibilities are endless. 

I made this cause i needed 8 players but I only have 4 buzzers, so I built this and for 8 playes it's recomended to use 2 esp's since i only have support for 4 players per esp for compatibility.
### Let me know if you guys want new features!
