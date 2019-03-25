// LCD screen
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <SoftwareSerial.h>
#include "WiFiEsp.h"

// Temp and humid sensor
#include "Adafruit_Sensor.h"
#include "Adafruit_AM2320.h"

// LCD adress
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// TempHumid sensor
Adafruit_AM2320 am2320 = Adafruit_AM2320();

// Create WiFi module object on GPIO pin 6 to (TX) and 7 to (RX)
SoftwareSerial Serial1(6, 7);

// Declare and initialise global arrays for WiFi settings
const char ssid[] = "SSID";
const char pass[] = "password";

// Declare and initialise variable for radio status
int status = WL_IDLE_STATUS;

// Create a web server on port 80
WiFiEspServer server(80);

// Wifi client
WiFiEspClient client;

// A Ring buffer
//RingBuffer buf(6);

// small welcome text for lcd
const String text = "Welcome!";

// list variable
String tom = " ";
String *tests[3] = {&tom, &tom, &tom};
bool clearList = true;

// wifi collect adress counter
int wifiCount;

// lcd variables
byte lcdCounter;
const String lcdListed = "-Listed";
const String lcdBought = "-Bought";
const String lcdFull = "-Full";

// Class for the 3 physical boxes put on the pressure resistors
class Box
{
public:
  byte led, pressure;
  long int counter;
  bool listed, bought;
  String grocery, htmlCode;

  // Constructor
  Box(byte L, byte P, long int C, bool Li, bool B, String G, String Html)
  {
    led = L;
    pressure = P;
    counter = C;
    listed = Li;
    bought = B;
    grocery = G;
    htmlCode = Html;
  };

  // Functions for changing the name of the grocery in the box
  void set_groc(String G)
  {
    grocery = G;
  }
  void set_htmlC(String H)
  {
    htmlCode = "<tr><td>" + H + "</td></tr>";
  }
};

// 3 boxes hardcoded for its own grocery
Box box1(2, A3, 0, false, false, "Apples", "<tr><td>Apples</td></tr>");
Box box2(3, A2, 0, false, false, "Peanuts", "<tr><td>Peanuts</td></tr>");
Box box3(4, A1, 0, false, false, "Tomatoes", "<tr><td>Tomatoes</td></tr>");

void setup()
{
  Serial.begin(115200); // debug serial

  // Initialize serial for ESP module
  Serial1.begin(9600);

  //LCD setups
  lcd.begin(16, 2);
  lcd.backlight();

  //Temp&Humid setup
  am2320.begin();

  // prints out "Welcome!"
  for (int i = 0; i < 8; i++)
  {
    lcd.setCursor(i + 3, 1);
    lcd.print(text[i]);
    delay(125);
  }

  // led pin declarations
  pinMode(box1.led, OUTPUT);
  pinMode(box2.led, OUTPUT);
  pinMode(box3.led, OUTPUT);

  // Initialize ESP module
  WiFi.init(&Serial1);

  // Check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD)
  {
    Serial.println(F("WiFi shield not present"));
    // Don't continue
    while (true)
      ;
  }

  // Attempt to connect to WiFi network
  while (status != WL_CONNECTED)
  {
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.println(ssid);

    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);
  }

  Serial.println(F("You're connected to the network"));
  printWifiStatus();

  // Start the web server
  server.begin();
}

void loop()
{
  // ----------***Pressure Resistors***----------
  // Beer
  pressurePad(box1.led, box1.pressure, box1.counter, box1.listed, box1.bought, box1.htmlCode);
  // Peanuts
  pressurePad(box2.led, box2.pressure, box2.counter, box2.listed, box2.bought, box2.htmlCode);
  // Meatballs
  pressurePad(box3.led, box3.pressure, box3.counter, box3.listed, box3.bought, box3.htmlCode);

  // LCD Grocery status
  lcdScreen();

  /*  Changing this also requires all the counters to change 
      value for the statements to correspond correctly - !!do not change willy-nilly
  */
  delay(100);

  // Wifi function - main loop only has a 100ms delay for better arduino-html updates
  wifi();
  wifiCount = 0;
}

// ------- Checks the force on the resistor to see if the groccery should be added to the list
void pressurePad(byte &led, byte &pressure, long int &counter, bool &listed, bool &bought, String &grocHtml)
{
  int force = analogRead(pressure);

  // -----------------force between x - 100
  if (force < 100 && !listed)
  {
    digitalWrite(led, LOW);

    // counts up to 2 hours. 72000, 1sec = 10
    if (counter < 72000)
    {
      counter++;
    }
    // if 2hours has passed find a
    else if (counter == 72000)
    {
      for (int i = 0; i < 3; i++)
      {
        if (*tests[i] == " ")
        {
          tests[i] = &grocHtml;
          i = 3;
        }
      }
      listed = true;
      counter = 0;
    }
  }

  // ------------------force between 175 - 475
  else if (force > 175 && force < 775 && !listed)
  {
    digitalWrite(led, HIGH);

    // counts up 5sec
    if (counter < 50)
    {
      counter++;
    }
    else if (counter == 50)
    {
      for (int i = 0; i < 3; i++)
      {
        if (*tests[i] == " ")
        {
          tests[i] = &grocHtml;
          i = 3;
        }
      }
      listed = true;
      counter = 0;
    }
    else if (counter > 50)
    {
      counter = 0;
    }
  }

  // ------------------force between 550 - x
  else if (force > 850 && listed && bought)
  {
    digitalWrite(led, LOW);

    // counts up 5sec
    if (counter < 50)
    {
      counter++;
    }
    else if (counter == 50)
    {
      listed = false;
      bought = false;
    }
    else if (counter > 50)
    {
      counter = 0;
    }
  }
  else
  {
    digitalWrite(led, LOW);
    counter = 0;
  }
}

// LCD screen that shows the ip of the arduino and cycles through grocery status
void lcdScreen()
{
  // Counts up to 6sec
  if (lcdCounter < 60)
  {
    lcdCounter++;
  }

  // Writes out each box grocery status every second
  if (lcdCounter == 15)
  {
    lcdWrite(box1.grocery, box1.listed, box1.bought);
  }
  else if (lcdCounter == 30)
  {
    lcdWrite(box2.grocery, box2.listed, box2.bought);
  }
  else if (lcdCounter == 45)
  {
    lcdWrite(box3.grocery, box3.listed, box3.bought);
    //lcdCounter = 0;
  }
  else if (lcdCounter == 60)
  {
    // Sets cursor and makes a blank 16 spaces line
    lcd.setCursor(0, 0);
    lcd.print(F("                "));
    lcd.setCursor(0, 0);

    // Prints out the current temperature and humidity values around the arduino
    lcd.print(F("Te:"));
    lcd.print(am2320.readTemperature());
    lcd.print(F(" Hu:"));
    lcd.print(am2320.readHumidity());
    lcdCounter = 0;
  }
}

// Writes out on the lcd the status of the grocery
void lcdWrite(String &grocery, bool &listed, bool &bought)
{
  // Sets cursor and makes a blank 16 spaces line
  lcd.setCursor(0, 0);
  lcd.print(F("                "));
  lcd.setCursor(0, 0);

  // Contents of the referenced box
  lcd.print(grocery);

  // Prints out grocery status
  if (listed && !bought)
  {
    lcd.print(lcdListed);
  }
  else if (listed && bought)
  {
    lcd.print(lcdBought);
  }
  else if (!listed && !bought)
  {
    lcd.print(lcdFull);
  }
}

// Webserver code
void wifi()
{
  String line;
  wifiCount = 0;

  // Listen for incoming clients
  client = server.available();

  if (client)
  {
    //buf.init();
    //Serial.println(F("New client"));

    // An HTTP request ends with a blank line
    boolean currentLineIsBlank = true;
    wifiCount = 0;
    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        wifiCount++;
        Serial.print(c);
        //buf.push(c);

        if (wifiCount == 6)
        {
          switch (c)
          {
          case 'C':
            // Clears and resets the list variables
            // Protects the list from getting cleared, without user knowledge, in the background if refreshing with the same adress
            if (!clearList)
            {
              for (int i = 0; i < 3; i++)
              {
                tests[i] = &tom;
              }
              clearList = true;

              if (box1.listed)
              {
                box1.bought = true;
              }
              if (box2.listed)
              {
                box2.bought = true;
              }
              if (box3.listed)
              {
                box3.bought = true;
              }
            }
            break;
          case 'R':
            // Opens up the list to be cleared again
            clearList = false;
            break;
          default:
            break;
          }
        }
        else if (wifiCount == 11)
        {
          switch (c)
          {
          case '1':
            boxCont(line, c);
            box1.set_groc(line);
            box1.set_htmlC(line);
            line = "";

            break;
          case '2':
            boxCont(line, c);
            box2.set_groc(line);
            box2.set_htmlC(line);
            line = "";

            break;
          case '3':
            boxCont(line, c);
            box3.set_groc(line);
            box3.set_htmlC(line);
            line = "";

            break;
          default:
            break;
          }
        }

        // Code for checking if an input has been sent and change the grocery variable of a box
        // Removed for trouble with Ringbuffer and timeouts on the wifi Esp
        // if (buf.endsWith("BOX1="))
        //  {
        //    boxCont(line);

        //    // Debug
        //    //Serial.print("Linjen :: ");
        //    //Serial.println(line);

        //    box1.set_groc(line);
        //    box1.set_htmlC(line);
        //    line = "";
        //  }
        // else if (buf.endsWith("BOX2="))
        // {
        //   boxCont(line);

        //   // Debug
        //   // Serial.print("Linjen :: ");
        //   // Serial.println(line);

        //   box2.set_groc(line);
        //   box2.set_htmlC(line);
        //   line = "";
        // }
        // else if (buf.endsWith("BOX3="))
        // {
        //   boxCont(line);

        //   // Debug
        //   // Serial.print("Linjen :: ");
        //   // Serial.println(line);

        //   box3.set_groc(line);
        //   box3.set_htmlC(line);
        //   line = "";
        // }

        // if (buf.endsWith("GET /C"))
        // {
        //   // Clears and resets the list variables
        //   // Protects the list from getting cleared, without user knowledge, in the background if refreshing with the same adress
        //   if (!clearList)
        //   {
        //     for (int i = 0; i < 3; i++)
        //     {
        //       tests[i] = &tom;
        //     }
        //     clearList = true;

        //     if (box1.listed)
        //     {
        //       box1.bought = true;
        //     }
        //     if (box2.listed)
        //     {
        //       box2.bought = true;
        //     }
        //     if (box3.listed)
        //     {
        //       box3.bought = true;
        //     }
        //   }
        // }
        // else if (buf.endsWith("GET /R"))
        // {
        //   // Opens up the list to be cleared again
        //   clearList = false;
        // }

        // If you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank)
        {

          //Serial.println(F("Sending response"));

          // Send a standard HTTP response header
          client.print(F(
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/html\r\n"
              "Connection: close\r\n"
              "\r\n"));
          client.print("<!DOCTYPE HTML>\r\n");
          client.print(F("<html>\r\n"));
          client.print(F("<head>\r\n"));
          client.print(F("<title>Smart list</title>\r\n"));
          client.print(F("<link rel=\"stylesheet\" href=\"https://hassegithubber.github.io/main.css?\">\r\n"));
          client.print(F("<link rel=\"shortcut icon\" href=\"about:blank\">\r\n"));
          client.print(F("</head>\r\n"));
          client.print(F("<body>\r\n"));
          client.print(F("<div class=\"grid-container\">\r\n"));
          client.print(F("<div class=\"grid-item1\">To Buy!</div>\r\n"));

          // Code removed: Trouble with sending all this extra code over the wifi, webpage not loading and the wifi getting a timeout.
          // client.print(F("<form action='/' method=get >\r\n"));
          // client.print(F("Box1 item: <input type=text name='BOX1' value='"));
          // client.print(box1.grocery);
          // client.print(F("' size='8' maxlength='9'><br>\r\n"));
          // client.print(F("<input type=submit name='submit' value='Change Box1'>\r\n"));
          // client.print(F("</form>\r\n"));
          // client.print(F("<form action='/' method=get >\r\n"));
          // client.print(F("Box2 item: <input type=text name='BOX2' value='"));
          // client.print(box2.grocery);
          // client.print(F("' size='8' maxlength='9'><br>\r\n"));
          // client.print(F("<input type=submit name='submit' value='Change Box2'>\r\n"));
          // client.print(F("</form>\r\n"));
          // client.print(F("<form action='/' method=get >\r\n"));
          // client.print(F("Box3 item: <input type=text name='BOX3' value='"));
          // client.print(box3.grocery);
          // client.print(F("' size='8' maxlength='9'><br>\r\n"));
          // client.print(F("<input type=submit name='submit' value='Change Box3'>\r\n"));
          // client.print(F("</form>\r\n"));

          client.print(F("<div class=\"grid-item12\">\r\n"));
          client.print(F("<form action='/' method=get >\r\n"));
          client.print(F("Choose a box: 1. <input type=radio name='BOX' value='1'> 2. <input type=radio name='BOX' value='2'> 3. <input type=radio name='BOX' value='3'>\r\n"));
          client.print(F("<input type=text name='CONT' value='type' size='8' maxlength='9'><br/>\r\n"));
          client.print(F("</form>\r\n"));
          client.print(F("</div>\r\n"));

          client.print(F("<div class=\"grid-item2\">\r\n"));
          client.print(F("<table>\r\n"));
          for (int i = 0; i < 3; i++)
          {
            client.print(*tests[i]);
          }
          client.print(F("</table>\r\n"));
          client.print(F("</div>\r\n"));
          client.print(F("<div class=\"grid-item3\"><a href=\"/C\">Clear</a></div>\r\n"));
          client.print(F("<div class=\"grid-item4\"><a href=\"/R\">Refresh</a></div>\r\n"));
          client.print(F("</div>\r\n"));
          client.print(F("</body>\r\n"));
          client.print(F("</html>\r\n"));
          break;
        }
        if (c == '\n')
        {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r')
        {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    wifiCount = 0;

    // Give the web browser time to receive the data
    delay(2500);

    // Close the connection:
    client.flush();
    client.stop();
    //Serial.println(F("Client disconnected"));
  }
}

// Removed function for earlier attempt
// void boxCont(String &temp)
// {
//   while (!buf.endsWith("&"))
//   {
//     char c = client.read();
//     if (c != '&')
//     {
//       temp += c;
//     }
//     buf.push(c);
//   }
// }

// -----** Function for changing the name of the grocery in the box **----
// It recieves the reference of a temporary string used to take in the text from the html input.
void boxCont(String &temp, char &c)
{
  do
  {
    c = client.read();
    if (c == '=')
    {
      // || c != 'H' || c!= '\n'
      while (c != ' ')
      {
        c = client.read();
        if (c != ' ')
        {
          temp += c;
        }
      }
    }
  } while (c != ' ');
}

void printWifiStatus()
{
  // Print the SSID of the network you're attached to
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // Print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print(F("IP Address: "));
  Serial.println(ip);

  // Prints the ip on the lower lcd row
  lcd.setCursor(0, 1);
  lcd.print(ip);
}
