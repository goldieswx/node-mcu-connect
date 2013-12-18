/*
    node-arduino interface. 
    Copyright (C) 2014 David Jakubowski

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008
#include <string.h>


byte mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x92, 0xAF };
IPAddress ip(192, 168, 0, 178);

char dbg[200];

unsigned int localPort = 8888;      // local port to listen on

// buffers for receiving and sending data
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
char  replyBuffer[UDP_TX_PACKET_MAX_SIZE];       // a string to send back

// duration buffers

unsigned long durations[16];  // if 16 digital Output
int durationsSet[16];  // if 16 digital Output


IPAddress remoteAddress;
int       remotePortNumber;

#define DIGITAL 1
#define ANALOG 2

typedef struct _sAnalogNotification{
   short treshold;
   short pinId;
   short hiRange;
   short loRange;
   short lastTrigger;
   short enabled;
} SAnalogNotification;

EthernetUDP Udp;

  int listenning[] = {0,1,2,3,4};
  int output[] = {5,6,7,8,9};

  int listenningStates[10] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1 };
  int digitalNotif[10] = {4,0,0,0,0,0,0,0,0,0}; // notification pins
 
  SAnalogNotification analogTresholdNotif[10];  //{123,10,5,123,   456,20,50,454}; // treshold, pinId,range down, range up, value that triggered last notification (1 hi, 0 lo)., .. next

void setup() {

  Ethernet.begin(mac,ip);
  Udp.begin(localPort);
  
  setPinMode(listenning,sizeof(listenning)/sizeof(int),INPUT_PULLUP);
  setPinMode(output,sizeof(output)/sizeof(int),OUTPUT);
  
  memset(analogTresholdNotif,0,sizeof(analogTresholdNotif));
  
  memset(&(durations[0]),0,16*sizeof(unsigned long));  // if 16 digital Output
  memset(&(durationsSet[0]),0,16*sizeof(int));  // if 16 digital Output
  
}


void loop() {
  int packetSize = Udp.parsePacket();
  int pinId;
  int notificationId;
  int analogTreshold;
  int state;
  
  if(packetSize)
  {
    Udp.read(packetBuffer,UDP_TX_PACKET_MAX_SIZE);

    if (0 == strncmp(packetBuffer,"ST/A",4)) {
       pinId = packetBuffer[4];
       sendState(ANALOG,pinId);
    }

    if (0 ==  strncmp(packetBuffer,"ST/D",4)) {
       pinId = packetBuffer[4];
       sendState(DIGITAL,pinId);
    }

    if (0 ==  strncmp(packetBuffer,"SE/D",4)) {
       pinId = packetBuffer[4];
       state = packetBuffer[5];
       setState(pinId,state);
    }

    if (0 ==  strncmp(packetBuffer,"SD/D",4)) {
       pinId = packetBuffer[4];
       state = packetBuffer[5];
       checkDuration(pinId,state,*((short *)(&packetBuffer[6])));
    }

    if (0 ==  strncmp(packetBuffer,"LISN",4)) {
       remoteAddress =  Udp.remoteIP();
       remotePortNumber = 8888;  //Udp.remotePort();
    }

    if (0 == strncmp(packetBuffer,"NC/D",4)) {
       // notification of change of digital input.
       notificationId = packetBuffer[4];
       pinId = packetBuffer[5];
       digitalNotif[notificationId] = pinId;
       listenningStates[notificationId] = -1;
       //sendState(DIGITAL,pinId);
    }
    if (0 == strncmp(packetBuffer,"NC/A",4)) {
       // notification treshold of analog input.
       notificationId = packetBuffer[4];
       memcpy(&(analogTresholdNotif[notificationId]),&(packetBuffer[5]),sizeof(SAnalogNotification));
       
      SAnalogNotification * s = &(analogTresholdNotif[notificationId]);
       //sendState(ANALOG,pinId);
    }
    
  }
  
  checkAnalogNotifications();
  checkDigitalNotifications();
  checkDuration(0xFF,0,0);
  
  delay(10);
};


void setPinMode(int* input, int len, int type) {
   for (int i=0;i< len;i++) {
      pinMode(input[i],type); 
   }
}

void sendState(int type,int pinId) {

    if(type==DIGITAL) {
      sendState(type,pinId,digitalRead(pinId));
    }
  
    if(type==ANALOG) {
      sendState(type,pinId,analogRead(pinId));
    }
  
};

void setState(int pinId,int state) {

   if (state) {
     digitalWrite(pinId,HIGH);
   } else {
     digitalWrite(pinId,LOW);
   }
   if ((pinId < 16) &&  (pinId >=0)) {
     durationsSet[pinId] = 0;
   }
};

void checkDuration(int pinId,int state,short duration) {

    unsigned long currentTime = millis();
    
    if (pinId == 0xFF) {
      for (int i= 0;i<16;i++) {
         if (durationsSet[i]) {
             if (durations[i] <= currentTime) {
                setState(i,LOW);
                sendState(DIGITAL,i);
                durationsSet[i] = 0; 
             } 
         }  
      }
    }  else {
       setState(pinId,HIGH);
       sendState(DIGITAL,pinId); 
       durations[pinId] = currentTime;
       durations[pinId] += (((unsigned long)duration)*1000);
       durationsSet[pinId] = 1; 
    }
  
};

void sendState(int type,int pinId,int value) {

    int len = 4;
  
    if(type==DIGITAL) {
      //strcpy(replyBuffer,"ST/D");
      //replyBuffer[len++] = (char) pinId;
      //replyBuffer[len++] = (char) value;      
      sprintf(replyBuffer,"ST/D%d/%d",pinId,value);
      len = strlen(replyBuffer);
    }
  
    if(type==ANALOG) {
      sprintf(replyBuffer,"ST/A%d/%d",pinId,value);
      len = strlen(replyBuffer);
    }
  
    Udp.beginPacket(remoteAddress,remotePortNumber);
    Udp.write((const unsigned char*)replyBuffer,len);
    Udp.endPacket();
 

};

void checkDigitalNotifications() {
  
  int pinId;
  int state;
  
  for(int i=0;i<10;i++){
      pinId = digitalNotif[i];
      
      if (pinId) {
       state = digitalRead(pinId);
       if (state != listenningStates[i]) {
          
          listenningStates[i] = state; 
          sendState(DIGITAL,pinId);
       }
      }
  }     

};


void checkAnalogNotifications() {
  
   int analogRd;
   SAnalogNotification * notif;
   int newState,oldState;
  
   for(int i=0;i<10;i++){
     notif = &(analogTresholdNotif[i]);
     if(notif->enabled) {
       analogRd = analogRead(notif->pinId);
       oldState = notif->lastTrigger;
       newState = oldState;

       if (oldState != 0) {
         if(analogRd <= notif->treshold-notif->loRange) {
            newState = 0;
         }      
       } 
       if ((oldState == 0) || (oldState != 1)) {
          if(analogRd >= notif->treshold+notif->hiRange) {
              newState = 1;
          }     
       }    

       if(newState !=  oldState) {
          notif->lastTrigger = newState;
          sendState(ANALOG,notif->pinId,analogRd);
       }
     }
     
   }
  
  
};
