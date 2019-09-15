#include "Keyboard.h"
#include <LiquidCrystalFast.h>
#include "Adafruit_Keypad.h"

const byte ROWS = 4; // rows
const byte COLS = 4; // columns
//define the symbols on the buttons of the keypads
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {5, 4, 3, 2}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {11, 10, 9, 8}; //connect to the column pinouts of the keypad

//initialize an instance of class NewKeypad
Adafruit_Keypad customKeypad = Adafruit_Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS);

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystalFast lcd(rs, en, d4, d5, d6, d7);

void setup() {
  lcd.begin(16, 2);
  // open the serial port:
  Serial.begin(115200);
  // initialize control over the keyboard:
  Keyboard.begin();
  customKeypad.begin();
  lcd.clear();
}

void loop() {
  // check for incoming serial data:
  if (Serial.available() > 0) {
    // read incoming serial data:
    char inChar = Serial.read();
    // Type the next ASCII value from what you received:
    Keyboard.write(inChar + 1);
    lcd.write(inChar);
  }
  customKeypad.tick();

  while(customKeypad.available()){
    keypadEvent e = customKeypad.read();
    Serial.print((char)e.bit.KEY);
    if(e.bit.EVENT == KEY_JUST_PRESSED) Serial.println(" pressed");
    else if(e.bit.EVENT == KEY_JUST_RELEASED) Serial.println(" released");
  }

  delay(10);
}
 /*
 All commands are terminated by a newline character ('\n' or 0x0A),
 and respond with "OK\n" if successful, or "?\n" when not.
 hello 	Connection initialisation command.  This currently doesn't do anything, but can be used to check if the display is present.
cls 	   Clears the screen.  For displays that support different colours, this command will clear the display to to the current background colour.

fg R,G,B 	Sets the current foreground colour on devices that support colour.  R/G/B are the Red/Green/Blue components of the colour and range from 0-255.  For example, to set the foreground colour to red,
fg 255,0,0

bg R,G,B 	Sets the current background colour on devices that support colour.  R/G/B are the Red/Green/Blue components of the colour and range from 0-255.  For example, to set the background colour to black,
bg 0,0,0

text x, y, data 	Displays text on the screen at character position x,y.  data is a quoted string.  For graphic based devices, the current foreground and background colours are used. For example,
text 0,1, "Test string"

textp x, y, data 	Displays text on the screen at pixel position x,y.  data is a quoted string.  For graphic based devices, the current foreground and background colours are used. This will only work on devices that support pixel positioning.  For example,
textp 92,27, "Test string"

font id 	Changes the font to 'id'.  This only works on devices that support fonts.  For example,
font 0

backlight intensity 	Sets the LCD backlight to 'intensity' from 0-100.  0 is off.  Only works with devices that support changing the backlight intensity.  For eaxmple...
backlight 50

rect x,y,w,h 	Draw a rectangle with its top corner at (x,y), width w, and height h.  The rectangle is drawn with the current foreground colour. Only works with devices that support rectangles.  For example...
rect 0,0,320,240

fill x,y,w,h 	Draw a filled rectangle with its top corner at (x,y), width w, and height h.  The rectangle is filled with the current foreground colour. Only works with devices that support rectangles.  For example...
fill 0,0,320,240
*/
