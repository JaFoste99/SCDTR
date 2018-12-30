#include "Comm_I2C.h"

volatile bool Comm_I2C::sync;
//int Comm_I2C::iter;

Comm_I2C::Comm_I2C(int _addr) {
  addr = _addr;
}

/*void Comm_I2C::getOtherU(float _u2) {
  u2 = _u2;
}*/

void Comm_I2C::msgAnalyse(char id, String data_str) {

  switch (id) {
    case 1:
      //Serial.println("Consensus Flag -> T");
      consensus_flag = true;
      consensus_data = data_str;
      break;

    case 2:
      calib_flag = true;
      break;

    default:
      break;
  }

}

int Comm_I2C::msgSend(char id, int dest_addr, String data_str) {

  Wire.beginTransmission(dest_addr);
  Wire.write(id);
  Wire.write(addr);
  Wire.write(data_str.c_str());

  return Wire.endTransmission(); // Returns 0 if the msg was sent successfully

}

void Comm_I2C::msgBroadcast(char id, String data_str) {

  /*Wire.beginTransmission(RASP_ADDR);
  Wire.write(id);
  Wire.write(' ');
  Wire.write(addr + 48);
  Wire.write(data_str.c_str());*/

  msgSend(id, 0, data_str);

  //return Wire.endTransmission(); // Returns 0 if the msg was sent successfully
}


void Comm_I2C::findNodes(){
  
  int error;
  int _ndev = 0;
  
  for(int address = 1; address < 127; address++){

    if(address == RASP_ADDR)  continue;

    Wire.beginTransmission(address);
    Wire.write('x');
    error = Wire.endTransmission();
    // The data was sent successfully
    if (error == 0){
      Serial.println(address);
      _ndev++;
    } 
  }

  ndev = _ndev + 1;
  Serial.println("Search complete");
}

int Comm_I2C::getAddr() const {

  return addr;
}

/*void Comm_I2C::msgSyncCallback(int num) {
  if(Wire.available() > 0){
      //char c = Wire.read();
      if(Wire.read() == 'a'){
        //Serial.write(c);
        sync = true;
      }
    }
}*/

void Comm_I2C::msgSync(int addr) {

  sync = false;

  while (!sync) {
    //Serial.println(sync);
    //Wire.onReceive(msgSyncCallback);
    delayMicroseconds(1500);
    //Serial.println(sync);
    Wire.beginTransmission(addr);
    Wire.write('a');
    Wire.endTransmission();
  }

  sync = false;

  //Serial.println("Sync!");
  delayMicroseconds(2000);
}

String Comm_I2C::floatToString(float num) {

  char str[8];
  dtostrf(num, 7, 2, str);
  String str2(str);

  return str2;

}
