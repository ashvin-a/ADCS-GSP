#include <Wire.h>
#include "vctrmnpltn.h"
#include "sensor_params.h"

const uint8_t gyro_add = 0x68;
const uint8_t magmeter_add = 0x1E;

double Bx,By,Bz;
double B[3];
double Bx_e,By_e,Bz_e;
double Wx,Wy,Wz;
double W[3];
double Wx_e,Wy_e,Wz_e;
double W_prev[3] = {0,0,0};
double B_prev[3] = {0,0,0};
double current[3];
double voltage[3];
double muB[3];
double VscaleFctr = 11.59057;
double rsltn = 255;
double analogOut[3];

int c = 0;                                  //in error checking fn

int rodX = 8;
int rodX_ = 7;
int rodY = 6;
int rodY_ = 5;
unsigned long t_in_S;
int rom_add = 0;
unsigned long last_mill = 0;
void read_eeprom();



void setup() {
  // put your setup code here, to run once:

  pinMode(coil_LED, OUTPUT);
  pinMode(idle_LED, OUTPUT);
  pinMode(idle_Pin,INPUT_PULLUP);

  if (digitalRead(idle_Pin))  read_eeprom();
  while(digitalRead(idle_Pin))
  {                                                           //switch when on prevents the code from running
    digitalWrite(idle_LED, HIGH);
    delay(500);                                               //idle mode 
    digitalWrite(idle_LED, LOW);
    delay(500);
    
  }
  
  
  pinMode(calib_LED,OUTPUT);
  for(int j = 0;j < 5;j++)
  {
    digitalWrite(calib_LED,LOW);
    delay(1000);
    digitalWrite(calib_LED,HIGH);                             //Indication to hold the CubeSat still for calibration of gyro
    delay(1000);
  }
  digitalWrite(calib_LED,LOW);


  Serial.begin(9600);
  Wire.begin();
  
  write2reg(gyro_add,0x6B,0x00);           //device reset
  
  write2reg(magmeter_add,0x00,0x78);       //output rate 75 per sec
  write2reg(magmeter_add,0x01,0xE0);       //range slctn +/-8.1Gauss
  write2reg(magmeter_add,0x02,0x00);       //cont measuremnt mode

  write2reg(gyro_add,0x6B,0x00);                              //gyro reset
  Wire.beginTransmission(gyro_add);
  Wire.write(0x1B);
  Wire.endTransmission(false);
  Wire.requestFrom(gyro_add,1,true);
  byte def_reg_val = Wire.read();                             //read register data for range
  byte new_reg_val = (def_reg_val&0x7)|0x10;
  write2reg(gyro_add,0x1B,new_reg_val);                       // Set range at 1000deg/s
  
  find_error();
  delay(50);
}

void write2reg(uint8_t add, uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(add);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission(true);
}

void find_error()
{
  while (c < 200) {
    Wire.beginTransmission(gyro_add);
    Wire.write(0x43);                             //0x43,44,45,46,47,48 LSB and MSB values
    Wire.endTransmission(false);
    Wire.requestFrom(gyro_add, 6, true);
    Wx = Wire.read() << 8 | Wire.read();
    Wy = Wire.read() << 8 | Wire.read();
    Wz = Wire.read() << 8 | Wire.read();
    // Sum all readings
    Wx_e = Wx_e + (Wx / 32.8);
    Wy_e = Wy_e + (Wy / 32.8);                  //LSB per unit is 131.0 for +/-250deg/sec range // changed to 32.8
    Wz_e = Wz_e + (Wz / 32.8);
    c++;
  }
  //Divide the sum by 200 to get the error value
  Wx_e = Wx_e / 200;
  Wy_e = Wy_e / 200;
  Wz_e = Wz_e / 200;
  // Print the error values on the Serial Monitor
  Serial.print("GyroErrorX: ");
  Serial.println(Wx_e);
  Serial.print("GyroErrorY: ");
  Serial.println(Wy_e);
  Serial.print("GyroErrorZ: ");
  Serial.println(Wz_e);
}

void navigation(double B[3],double W[3])
{
  if(abs(B_prev[0]) + abs(B_prev[1]) + abs(B_prev[2]) + abs(W_prev[0]) + abs(W_prev[1]) + abs(W_prev[2]) == 0)
  {
    W_prev[0] = W[0];
    W_prev[1] = W[1];
    W_prev[2] = W[2];
    B_prev[0] = B[0];
    B_prev[1] = B[1];
    B_prev[2] = B[2];
  }

  else
  {
    for(int i = 0;i < 3;i++)
    {
      W[i] = W_prev[i]*(1.0 - Sw) + Sw*W[i];
      B[i] = B_prev[i]*(1.0 - Sb) + Sb*B[i];
    } 
  }
}

void read_eeprom(){                                           //Function to read EEPROM
  rom_add = 0;
  float omegaZ;
  unsigned long t_in_s;
  Serial.begin(9600);
  digitalWrite(idle_LED, HIGH);
  while(rom_add < 4096)
    {
      Serial.print(rom_add);
      Serial.print("\t");
      Serial.print(EEPROM.get(rom_add,omegaZ));
      Serial.print("\t");
      Serial.println(EEPROM.get(rom_add+sizeof(omegaZ),t_in_s));

      rom_add = rom_add + sizeof(omegaZ)+sizeof(t_in_s);
      delay(10);
    }
  Serial.end();
  digitalWrite(idle_LED, LOW);
  rom_add = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
  Wire.beginTransmission(gyro_add);
  Wire.write(0x43);
  Wire.endTransmission(false);
  Wire.requestFrom(gyro_add,6,true);
  Wx = (Wire.read()<<8)|(Wire.read());
  Wy = (Wire.read()<<8)|(Wire.read());
  Wz = (Wire.read()<<8)|(Wire.read());
  Wx = Wx/32.8 - Wx_e;                                   //lsb per unit rad = 32.8
  Wy = Wy/32.8 - Wy_e;
  Wz = Wz/32.8 - Wz_e;
  Wx = (Wx*pi)/180.00;                                    //into radians
  Wy = (Wy*pi)/180.00;
  Wz = (Wz*pi)/180.00;
  W[0] = Wx;
  W[1] = Wy;
  W[2] = Wz;
  
  //-----------------------------------------------------//

  Wire.beginTransmission(magmeter_add);
  Wire.write(0x03);
  Wire.endTransmission(false);
  Wire.requestFrom(magmeter_add,6,true);                    //0x03,04,05,06,07,08 LSB and MSB values
  Bx = ((Wire.read()<<8)|(Wire.read()))/1090.0;
  Bz = ((Wire.read()<<8)|(Wire.read()))/1090.0;
  By = ((Wire.read()<<8)|(Wire.read()))/1090.0;             //Lsb per unit = 1090 for +/-1.3Gauss
  Bx = Bx*1E-4;
  By = By*1E-4;
  Bz = Bz*1E-4;                                             //into Teslas
  B[0] = Bx;
  B[1] = By;
  B[2] = Bz;
  
  navigation(B,W);                                          //filter
  
  //----------------------------------------------------//

  vctrmnpltn vctr1;
  vctr1.cross(W,B);
  double WxB[] = {vctr1.resultant[0],vctr1.resultant[1],vctr1.resultant[2]};
  double k = 166.67;
  for(int i = 0;i < 3;i++)
  {
    muB[i] = k*WxB[i];                                      //control
  }

  double sum_abs_muB = abs(muB[0])+abs(muB[1])+abs(muB[2]);
  double norm_muB = sqrt(pow(muB[0],2) + pow(muB[1],2) + pow(muB[2],2));
  if (sum_abs_muB > muBmax)
  {
    for(int i = 0;i < 3;i++)
    {
      muB[i] = (muB[i]*muBmax)/norm_muB;
    }
  }

  for(int i = 0;i < 3;i++)
  {
    current[i] = muB[i]/(n*A*aFctr);                      //0 - 0.2 Am2
    voltage[i] = current[i]*Rnet*VscaleFctr;              //0 - 0.4v mapped to 0 - 5v
    analogOut[i] = voltage[i]*(rsltn/5);                  //Into 0 - 255 values
  }

  //-------------------------------------------------------//
  
  Serial.print("Voltage: ");
  for(int i = 0;i < 3;i++)
  {
    Serial.print(round(analogOut[i]));
    Serial.print("  ");

  }
  
  analogWrite(rodY\X, analogOut[i])


  if (rom_add<4096 && (millis()-last_mill)>100)
  {
      float omgZ = W[2];
      t_in_S = millis();
                                                                
      EEPROM.put(rom_add,omgZ);  //Write the values of omega and time in the ROM '
      EEPROM.put(rom_add+sizeof(omgZ),t_in_S);

      rom_add = rom_add + sizeof(omgZ)+sizeof(t_in_S);
      last_mill = millis();   
  }
  
  if (rom_add>=4096)
  {
    digitalWrite(calib_LED,HIGH);                                //ROM full indicator
  }

  
  Serial.println();
//  delay(1000);
  
}
