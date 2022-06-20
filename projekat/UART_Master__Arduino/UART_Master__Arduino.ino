#include <LiquidCrystal.h>
#define DELAY 10
byte readTaster()
{
  int val = analogRead(0);
 
  if(val > 401 && val < 600){
    return 1; //LEFT
  }
  if(val > 61 && val < 200){
    return 2; //UP
  }
  if(val > 201 && val < 400){
    return 3; //DOWN
  }
  if(val < 60){
    return 4; //RIGHT  
  }
  return 0;
}
void setup() {
  Serial.begin(1200);
}
void loop() {
  switch(readTaster())
  {
    case 1:
      Serial.write('a'); //left
      break;
    case 2:
      Serial.write('w'); //up
      break;
    case 3:
      Serial.write('s'); //down
      break;
    case 4:
      Serial.write('d'); //right
      break; 
  }
  delay(20);
}
