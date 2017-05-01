#ifndef ARDUINO_DMXPRO_H_INCLUDED
#define ARDUINO_DMXPRO_H_INCLUDED
/*!
   \file arduino_DMXPro.h
   \brief Header file for the arduino DMX Pro Widget Emulation Library
   \author Shenghao Yang

   This library only implements Firmware Version 1 (DMX Pro, TX only)
*/

#include <Arduino.h>

namespace DMXPro{
  /**
    \brief Lists of all message types supported by a DMX Pro Widget (Firmware Version 1)
  */
  enum labels:uint8_t{
    invalid=0x00,            /*!< Used to represent a data packet with an invalid label -i.e. a DMX Pro v1 Shouldn't support this command */
    reprogram_firmware=0x01, /*!< Unsupported, reprogram firmware - but we can't really reprogram PROGMEM on the fly */
    program_flash_page=0x02, /*!< Unsupported, programs pages of device's flash - can't really do that on the Arduino easily */
    program_flash_page_reply=0x02, /*!< Unsupported, no programming of flash pages is supported by this library */
    get_widget_parameters=0x03, /*!< Supported, but no programming of parameters is supported */
    get_widget_parameters_reply=0x03, /*!< Supported, but no storing of parameters is supported */
    store_widget_parameters=0x04, /*!< Unsupported, no storing of parameters is supported */
    receive_dmx_data=0x05, /*!< Supported, send DMX data to PC from widget */
    send_dmx_data=0x06, /*!< Supported, send DMX data from widget onto physical link*/
    get_widget_serial=0x0a, /*!< Supported, get serial number */
    get_widget_serial_reply=0x0a, /*!< Supported, reply with serial number */
    label_max, /*!< DO NOT MOVE. End marker */
    /*** All other 'labels' beyond 0x0a are unsupported. The emulator will take no action
    \todo Instead of ignoring the data bytes sent in, actually read in and process the packets, but do nothing with the data in them
    */
  };

  /**
    \brief Message delimiters and their values
  */
  enum delimiters:uint8_t{
    start=0x7e, /*!< Message start delimiter */
    end=0xe7,   /*!< Message end delimiter */
  };

  /**
    \brief Structure representing widget parameters that are not defined arbitarily
  */
  struct widget_parameters_non_user_defined{
    const uint16_t firmware_version=0x0100;       /*!< Firmware version, defaulted to one */
    uint8_t  break_time=9;                  /*!< DMX break time */
    uint8_t  mark_after_break_time=1;       /*!< DMX mark-after-break time */
    uint8_t  dmx_output_rate=40;            /*!< DMX output rate */
    static constexpr uint8_t size(){
      return (sizeof(firmware_version)+sizeof(break_time)+sizeof(mark_after_break_time)+sizeof(dmx_output_rate));
    }
    void write(uint8_t* target){
      memcpy(target,&firmware_version,sizeof(firmware_version));
      target+=sizeof(firmware_version);
      *target=break_time; ++target;
      *target=mark_after_break_time; ++target;
      *target=dmx_output_rate; ++target;
    }
    void read(uint8_t* source){
      break_time=*(source++);
      mark_after_break_time=*(source++);
      dmx_output_rate=*(source++);
    }
  };

  template<typename ser_typ>
  class Processor{
private:
    enum class States{
      none, /*!< No message being processed */
      type_wait, /*!< Waiting for message type */
      length_wait, /*!< Waiting for length */
      data_wait, /*!< Waiting for data */
      end_wait, /*!< Waiting for message end */
    };
    States   state=States::none;      /*!< Processor state */
    labels   message_label;           /*!< Label of message being processed */
    uint16_t message_data_length;     /*!< Length of message data being processed */
    uint16_t message_data_received;   /*!< Length of message data that has been received */
    uint8_t* dmx_data;                /*!< DMX buffer data location */
    uint16_t dmx_max_channels;        /*!< Maximum number of DMX channels */
    widget_parameters_non_user_defined parameters; /*!< DMX widget parameters */
    uint16_t widget_user_configuration_size=0;     /*!< Widget user configuration size */
    ser_typ* ser;                     /*!< Pointer to a serial object used to communicate with the PC */

    uint32_t serial_number;           /*!< Widget serial number */
    /*!
       \brief Sets a DMX channel to a certain value
       \param[in] channel_number the DMX channel to set, relative to zero
       \param[in] value DMX channel data value
       \note  Bounds checking is performed. When a request is made to set
       the DMX data value of a channel whose storage is not reserved, the
       request is ignored, silently.
    */
   void set_channel(uint16_t channel_number,uint8_t value){
      if(channel_number >= dmx_max_channels){

      } else {
        dmx_data[channel_number]=value;
      }
    }

    /*!
       \brief Processor state machine function. Checks that the next incoming
       byte is indeed the start byte

       If the next incoming byte is not the start byte, the state machine
       is reset, such that it waits for a start byte again.

       \side Alters the state of the state machine

       \pre The state machine has been reset
       \post The state machine is now waiting for the type byte (label byte in ENTTEC nomenclature)
    */
    void obtain_start_byte(){
      if(ser->read()!=start){
        state=States::none;
      } else {
        state=States::type_wait;
      }
    }

    /*!
       \brief Processor state machine function. Stores the next incoming
       value as the type byte.

       \note The message_label value is updated with this type byte.

       \side Alters the state of the state machine, and updates one variable of
       non-local scope.

       \pre The state machine has already acquired a start byte
       \post The state machine is now waiting for the two length bytes
    */
    void obtain_type_byte(){
      message_label=ser->read();
      if(message_label >= label_max){
        message_label=invalid;
      }
      state=States::length_wait;
    }

    /*!
       \brief Processor state machine function. Reads in the next two length
       bytes, and checks that the resulting length value derived from those
       two bytes is valid.

       \note The message_data_length variable is also updated with this received length value.

       If the payload length is not valid, the state machine is reset. Otherwise,
       the state machine is advanced, such that it is now acquiring the payload
       of the message.

       \side Alters the state of the state machine, and updates one variable of
       non-local scope.

       \pre The state machine has already acquired the label byte
       \post The state machine is now acquiring the payload of the message
    */
    void obtain_length_bytes(){
      if(ser->available() < sizeof(message_data_length)){

      } else {
        uint8_t buffer[sizeof(message_data_length)];
        ser->readBytes(buffer,sizeof(message_data_length));
        memcpy(&message_data_length,buffer,sizeof(message_data_length));

        if(message_data_length > 600){
          /* Invalid message data length. Set state back to original */
          state=States::none;
        } else {
          /* Valid message data length. Set state to next, and reset counter for bytes received */
          state=States::data_wait;
          message_data_received=0;
        }
      }
      return;
    }

    /*!
       \brief Processor state machine function. Checks that the next incoming
       byte is indeed the end delimiter byte.

       No matter the validity of the next incoming byte as a end delimiter byte,
       the state machine is reset.

       \side Alters the state of the state machine

       \pre The state machine has already acquired a the payload of the message
       \post The state machine is now waiting a start byte again

       \retval true A valid message has been received
       \retval false A non-valid message was received
    */
    bool obtain_end_byte(){
      if(ser->read()!=end){
        state=States::none;
        return false;
      } else {
        state=States::none;
        return true;
      }
    }

    /*!
       \brief One of the actions taken when the state machine is tasked to
       obtain the payload data. This function represents the action to
       discard the payload data. (Run for message types that are not supported,
       or not implemented, where the data does not concern us)

       One byte of serial data is read in, and subsequently discarded.

       \note Increments the message_data_received variable.

       \side Alters one variable of non-local scope
    */
    void discard_data_segment(){
      ser->read();
      ++message_data_received;
    }

    void store_widget_parameters_acq_data(){
      if(!message_data_received){
        if(ser->available()<5){

        } else {
          uint8_t buffer[5];
          ser->readBytes(buffer,5);
          parameters.read(buffer+2);
          message_data_received+=5;
        }
      } else {
        discard_data_segment();
      }
    }

    void get_widget_parameters_acq_data(){
      if(ser->available()<sizeof(widget_user_configuration_size)){
      } else {
        uint8_t buffer[sizeof(widget_user_configuration_size)];
        ser->readBytes(buffer,sizeof(widget_user_configuration_size));
        memcpy(&widget_user_configuration_size,buffer,sizeof(widget_user_configuration_size));
        state=States::end_wait;
      }
    }

    void send_dmx_data_acq_data(){
      if(message_data_received){
        while((ser->available()) && (message_data_received!=message_data_length)){
          set_channel(message_data_received-1,ser->read());
          ++message_data_received;
        }
      } else {
        ser->read();
        ++message_data_received;
      }
    }

    void obtain_data_bytes(){
      if(message_data_received != message_data_length){
        /* When we have not obtained all the data bytes - run
         * the functions that will acquire them for us
         */
        switch(message_label){
          case invalid:
            discard_data_segment();
            break;
          case program_flash_page:
            discard_data_segment();
            break;
          case get_widget_parameters:
            get_widget_parameters_acq_data();
            break;
          case store_widget_parameters:
            store_widget_parameters_acq_data();
            break;
          case send_dmx_data:
            send_dmx_data_acq_data();
            break;
        }
      } else {
        /* When we have acquired all the data, advance to the next
         * state of acquiring the message end marker
         */
        state=States::end_wait;
      }
    }

    void send_data(uint8_t* data,labels label,uint16_t data_size,uint16_t append_zeroes){
      ser->write(start);
      ser->write(label);
      uint8_t size_buf[2];
      data_size+=append_zeroes;
      memcpy(size_buf,&data_size,sizeof(data_size));
      ser->write(size_buf,sizeof(data_size));
      ser->write(data,data_size-append_zeroes);
      for(uint16_t i=0;i<append_zeroes;i++){
        ser->write((uint8_t)0x00);
      }
      ser->write(end);
    }

    bool process_message(){
      switch(message_label){
        case invalid:
          return false;
          break;
        case reprogram_firmware:
          return false;
          break;
        case program_flash_page:
          return false;
          break;
        case get_widget_parameters:
          {
            uint8_t buffer[widget_parameters_non_user_defined::size()];
            parameters.write(buffer);
            send_data(buffer,get_widget_parameters_reply,widget_parameters_non_user_defined::size(),widget_user_configuration_size);
            return false;
            break;
          }
        case store_widget_parameters:
          {
            return false;
            break;
          }
        case send_dmx_data:
          {
            return true;
            break;
          }
        case get_widget_serial:
          {
            uint8_t buffer[sizeof(serial_number)];
            memcpy(buffer,&serial_number,sizeof(serial_number));
            send_data(buffer,get_widget_serial_reply,sizeof(serial_number),0);
            return false;
            break;
          }
      }
      return false;
    }

public:
    Processor(ser_typ& ser_obj,uint32_t serial,uint8_t* data,uint16_t max_channels){
      ser=&ser_obj;
      serial_number=serial;
      dmx_max_channels=max_channels;
      dmx_data=data;
      memset(dmx_data,0,dmx_max_channels);
    }

    bool process(void){

      if(ser->available()){

        switch(state){
          case States::none:
            obtain_start_byte();
            break;

          case States::type_wait:
            obtain_type_byte();
            break;

          case States::length_wait:
            obtain_length_bytes();
            break;

          case States::data_wait:
            obtain_data_bytes();
            break;

          case States::end_wait:
            if(obtain_end_byte()){
              return process_message();
            }
            break;
        }
      }
      return false;
    }

    /*!
       \brief Obtains the DMX data value for a particular DMX channel
       \param[in] c the DMX channel to obtain the DMX data value for
       \warning Bounds checking is not performed. If RAM storage has not been
       reserved for this DMX channel's data, then the access may result in a
       dereference of an invalid pointer.
       \return DMX data value [0,255] for that particular DMX channel
    */

    uint8_t operator[](uint16_t c){
      return dmx_data[c-1];
    }

    void upload_dmx(bool valid,const uint8_t* data,uint16_t length){
      ser->write(start);
      ser->write(receive_dmx_data);
      uint8_t size_buf[2];
      length+=2;
      memcpy(size_buf,&length,sizeof(length));
      ser->write(size_buf,sizeof(length));
      ser->write(valid);
      ser->write((uint8_t)0x00);
      ser->write(data,length-2);
      ser->write(end);
    }

  };
}
#endif
