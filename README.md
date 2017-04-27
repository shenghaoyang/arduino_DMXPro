# arduino_DMXPro
arduino_DMXPro is an Arduino Library that allows an Arduino to act (partially) as an ENTTEC DMX Pro V1 (No RDM support). 
Accepts an Object that supports the Arduino Serial APIs and parses incoming data on that Object as 
DMX Pro commands.

# Supported Commands 
So far, most of the commands supported by a DMX Pro V1 device are supported, except the following: 
1. Receive DMX on Change (Label = 9)  
2. Received DMX Change of State (Label = 9)  


However, some commands are specific to the DMX Pro hardware and this implementation simply ignores them:
1. Reprogram Firmware Request (Label = 1)  
     -No action is taken  
2. Reprogram Flash Page Request (Label = 2)  
     -The flash page is received but discarded  


Partial support is provided for these commands:  
1. Get Widget Parameters Request (Label = 3)   
     -Valid firmware version, break time data, mark after break time data, and DMX output rate data are sent back to the host  
     -But user defined configuration data is sent back to the host as all zeroes   
2. Set Widget Parameters Request (Label = 4)  
     -New parameters are accepted, but not written to memory   
     -User defined configuration data is not stored  

# Usage 
Include 'arduino_DMXPro.h' 
# API
All symbols are under the 'DMXPro' namespace 
##### Create a new parser (Processor)

'DMXPro::Processor\<typename ser_typ>::Processor(ser_typ& ser_obj,uint32_t serial,uint8_t* data,uint16_t max_channels)'

Where 'ser_obj'      is the Serial object to attach to  
      'serial'       is the serial number to use when replying to queries  
      'data'         is the location where received DMX data will be written to  
and   'max_channels' is the maximum number of channels of received DMX data to write (e.g. if max_channels is 13, then the DMX data value for the 14th channel will be discarded - also implies that the buffer pointed to by data must be at least max_channels large)

```C++
//Create a new Processor attached to 'Serial' with serial number 0xdeadbeef
#include "arduino_DMXPro.h"
...
uint8_t buffer[10];
DMXPro::Processor\<decltype(Serial)> p1(Serial,0xdeadbeef,buffer,10);
...
```
##### Process incoming data 

'bool DMXPro::Processor\<typename ser_typ>::process(void)'

```C++
//Parse incoming data from the serial port 
...
void loop(){
  p1.process();
}
...
```
The process() function returns 'true' when the buffer has been updated with new DMX data
otherwise, 'false' is returned.

It must also be called regularly in order to avoid a serial buffer overflow.

##### Send DMX data to a host 

'void DMXPro::Processor\<typename ser_typ>::upload_dmx(bool valid,const uint8_t* data,uint16_t length)'

Where 'valid'  represents whether the DMX data being uploaded to the host is valid  
      'data'   points to a buffer containing the DMX data to be sent  
and   'length' is the length of the buffer (Number of channels worth of data to send)  

```C++
//Upload DMX data to a host
...
uint8_t dmx_data[4]={0xde,0xad,0xbe,0xef};
void loop(){
  p1.upload_dmx(true,dmx_data,4);
}
...
```
# Compatibility 

Tested to be working on an Arduino Micro with a host computer running OLA. 
Other Arduino-compatible boards should work with OLA so long as it has built-in USB hardware (e.g. boards based on 32u4, ...)

QLC does not seem to work without modifications:
  1. VID and PID is not whitelisted 
  2. QLC attempts to use the FTDI library to access the device (unless forced to use QtSerialPort) 

Performance on other Arduino devices where the main MCU has no built-in USB hardware is difficult to test, OLA (and other DMX control
software) provide no way of setting the baud rate of the serial link. 
