# arduino_DMXPro
arduino_DMXPro is an Arduino Library that allows an Arduino to act (partially) as an ENTTEC USB DMX Pro V1. 

Receiving downstream DMX data from the PC and uploading DMX data to the PC are both supported.

The Arduino compatible board must have a real USB device and not simply a Serial-to-USB converter installed, since most software do not let you change the baud rate and send out data so fast that a Serial-to-USB converter cannot keep up.

# Compatibility 

Tested to be working on an Arduino Micro with a host computer running OLA. 
Other Arduino-compatible boards should work with OLA so long as it has built-in USB hardware (e.g. boards based on 32u4, ...)

QLC does not seem to work without modifications:
  1. VID and PID is not whitelisted 
  2. QLC attempts to use the FTDI library to access the device (unless forced to use QtSerialPort) 

# Limitations 

Only some relevant commands are supported at this time, as most features of the ENTTEC protocol are not used by most third party software supporting those devices - such as OLA, Jinx, etc.

# Supported Commands 
A listing of all possible commands can be found in:
<https://dol2kh495zr52.cloudfront.net/pdf/misc/dmx_usb_pro_api_spec.pdf>

So far, most of the commands supported by a DMX Pro V1 device are supported, except the following: 
1. Receive DMX on Change (Label = 8)  
2. Received DMX Change of State (Label = 9)  

However, some commands are specific to the DMX Pro hardware and this implementation simply ignores them:
1. Reprogram Firmware Request (Label = 1)  
     -Received data is discarded
2. Reprogram Flash Page Request (Label = 2)  
     -Received data is discarded  

Partial support is provided for these commands:  
1. Get Widget Parameters Request (Label = 3)   
     -DMX output data rates are sent back to the host 
     -User defined configuration data is not stored, so all zeroes are sent in response to a request
2. Set Widget Parameters Request (Label = 4)  
     -New parameters are accepted, but only stored in SRAM   
     -User defined configuration data is not stored  

# Usage 
Include 'arduino_DMXPro.h' in your Arduino sketch

# Examples 
1. Basic usage, receiving DMX data from host computer
```c++
...
#include "arduino_DMXPro.h" // All symbols are under the DMXPro namespace

void setup(){
     Serial.begin(115200); // Assuming that Serial is the USB HardwareSerial object 
}

void loop() {
     /* Declare an array to hold the received DMX data, 512-bytes wide. Can be smaller if 
      * needed, but the size needs to be specified to the constructor later.
      */
     uint8_t dmx_data_array[512];
     
     /* 
      * Create a new protocol processor operating on the Serial object, 
      * with: 
      * 0xdeadbeef as the serial number (Any 32-bit unsigned integer will do)
      * dmx_data_array as the array holding the received data, and 
      * 512 as the size of the DMX data array - a smaller size can be specified if not all channels
      * are needed for your application, to save on SRAM space (to buffer RGB LED data?)
      */
     DMXPro::Processor<decltype(Serial)> processor { Serial, 0xdeadbeef, dmx_data_array, 512}; 
     
     /* 
      * Process the received protocol units coming from the computer 
      * The process method must be called periodically to prevent serial buffer overflow.
      * If you seem to be missing data, you might want to investigate how frequently you're calling
      * the method.
      *
      * The method returns enumeration values from DMXPro::Event. For a description of what events can 
      * be returned, have a look at the header file.
      */ 
     while(1) {
	     switch(processor.process()) {
	          case DMXPro::Event::dmx_data: 
	               /* New data is available from the PC, which would be written to the 
	                * dmx_data_array buffer provided. Do with it as your application requires.
	                */
	          break;
	          
	          case DMXPro::Event::none:
	               /* No event returned - no new data is available from the PC */
	          break;
	          
	          case DMXPro::Event::parameters_changed:
	               /* The DMX output parameters have been changed, i.e. the MAB-time, DMX output rate, etc 
	                * has been changed by the host PC. 
	                *
	                * To retrieve the new parameters, create a new structure to hold them, 
	                * then call the params() method on the structure.
	                *
	                * The structure's members can be examined for the updated data.
	                */
	               DMXPro::widget_parameters_non_user_defined params_new {} 
	               processor.params(params_new);
	          break;
	          
	          case DMXPro::Event::parameters_requested:
	               /* The host PC has requested for our DMX output parameters.
	                * no action is required on your part, the process() method takes 
	                * care of replying to the host. 
	                * 
	                * This event is exposed in case some special action needs to be triggered 
	                */
	          break;
	          
	          case DMXPro::Event::serial_requested:
	               /* The host PC has requested for our serial number.
	                * no action is required, the process() method takes care of replying.
	                *
	                * This event is exposed in case some special action needs to be triggered
	                */
	          break;
	     }
	}
}
```

2. Sending DMX data to your host computer 
```c++
void loop() {
     // Creation of the protocol processor, setup stuff, etc.
  
     /* To send DMX data to the host computer, possibly to synchronize some DMX channels with ADC 
      * data (hint hint?), call the upload_dmx() method.
      *
      * The example below calls the upload_dmx() method to send two channels of DMX 
      * data, while specifying that the data is valid. 
      */
      uint8_t data[2]; // Array holding data to be sent, can be any size, up to 512 bytes.
      data[0] = (millis() & 0x000000ff);           // DMX channel 1 received by PC will contain lsb of millis()
      data[1] = ((millis() >> 0x08) & 0x000000ff); // Channel 2 will contain next most significant byte of millis() 
      
      processor.upload_dmx(true, data, 2);         // true means data is valid, can use false to signal fault condition
      
      // Unrelated stuff
}
```

