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
char ssid[] = "SSID";
char pass[] = "password";

// Declare and initialise variable for radio status
int status = WL_IDLE_STATUS;

// Create a web server on port 80
WiFiEspServer server(80);

// Wifi client
WiFiEspClient client;

// A Ring buffer
RingBuffer buf(12);

// small welcome text for lcd
const String text = "Welcome!";

// list variable
String tom = " ";
String *tests[3] = {&tom, &tom, &tom};
bool clearList = true;

// change box grocery variable
String line;

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
  int force;
  long int counter;
  bool listed, bought;
  String grocery, htmlCode;

  // Constructor
  Box(byte L, byte P, int F, long int C, bool Li, bool B, String G, String Html)
  {
    led = L;
    pressure = P;
    force = F;
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
Box box1(2, A3, 0, 0, false, false, "Beer", "<tr><td>Beer</td></tr>");
Box box2(3, A2, 0, 0, false, false, "Peanuts", "<tr><td>Peanuts</td></tr>");
Box box3(4, A1, 0, 0, false, false, "Meatballs", "<tr><td>Meatballs</td></tr>");

void setup()
{
  Serial.begin(115200); // debug serial

  // Initialize serial for ESP module
  Serial1.begin(9600);

  //LCD setups
  lcd.begin(20, 2);
  lcd.backlight();

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

  // Debugging
  //Serial.println(list);

  // ----------***Pressure Resistors***----------
  // Beer
  pressurePad(box1.led, box1.pressure, box1.force, box1.counter, box1.listed, box1.bought, box1.htmlCode);
  // Peanuts
  pressurePad(box2.led, box2.pressure, box2.force, box2.counter, box2.listed, box2.bought, box2.htmlCode);
  // Meatballs
  pressurePad(box3.led, box3.pressure, box3.force, box3.counter, box3.listed, box3.bought, box3.htmlCode);

  // LCD Grocery status
  lcdScreen();

  /*  Changing this also requires all the counters to change 
      value for the statements to correspond correctly - !!do not change willy-nilly
  */
  delay(100);

  // Wifi function - main loop only has a 100ms delay for better arduino-html updates
  wifi();
}

// ------- Checks the force on the resistor to see if the groccery should be added to the list
void pressurePad(byte &led, byte &pressure, int &force, long int &counter, bool &listed, bool &bought, String &grocHtml)
{
  force = analogRead(pressure);

  // -----------------force between x - 100
  if (force < 100 && !listed)
  {
    digitalWrite(led, LOW);

    // counts up to 2 hours. 72000, 1sec = 10
    if (counter < 72000)
    {
      counter++;
    }
    if (counter == 72000)
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
  else if (force > 175 && force < 475 && !listed)
  {
    digitalWrite(led, HIGH);

    // counts up 5sec
    if (counter < 50)
    {
      counter++;
    }
    if (counter == 50)
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
    else if (counter >= 50)
    {
      counter = 0;
    }
  }

  // ------------------force between 550 - x
  else if (force > 550 && listed && bought)
  {
    digitalWrite(led, LOW);

    // counts up 5sec
    if (counter < 50)
    {
      counter++;
    }
    if (counter == 50)
    {
      listed = false;
      bought = false;
    }
    else if (counter >= 50)
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
  bool test = false;
  // Listen for incoming clients
  client = server.available();

  if (client)
  {
    buf.init();
    Serial.println(F("New client"));

    // An HTTP request ends with a blank line
    boolean currentLineIsBlank = true;

    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        Serial.write(c);
        buf.push(c);

        if (buf.endsWith("GET /?BOX1="))
        {
          boxCont(line);

          // Debug
          //Serial.print("Linjen :: ");
          //Serial.println(line);

          box1.set_groc(line);
          box1.set_htmlC(line);
          line = "";
        }
        else if (buf.endsWith("GET /?BOX2="))
        {
          boxCont(line);

          // Debug
          // Serial.print("Linjen :: ");
          // Serial.println(line);

          box2.set_groc(line);
          box2.set_htmlC(line);
          line = "";
        }
        else if (buf.endsWith("GET /?BOX3="))
        {
          boxCont(line);

          // Debug
          // Serial.print("Linjen :: ");
          // Serial.println(line);

          box3.set_groc(line);
          box3.set_htmlC(line);
          line = "";
        }
        else if (buf.endsWith("GET /C"))
        {
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
          // else
          // {
          //   // Opens up the list to be cleared again
          //   clearList = false;
          // }
        }
        else if (buf.endsWith("GET /"))
        {
          // Opens up the list to be cleared again
          clearList = false;
        }

        // If you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank)
        {

          Serial.println(F("Sending response"));

          // Send a standard HTTP response header
          client.print(F(
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/html\r\n"
              "Connection: close\r\n"
              "\r\n"));
          client.print(F("<!DOCTYPE HTML>\r\n"));
          client.print(F("<html>\r\n"));
          client.print(F("<head>\r\n"));
          client.print(F("<title>Smart list</title>\r\n"));
          client.print(F("<link rel=\"stylesheet\" href=\"https://hassegithubber.github.io/main.css?\">"));
          client.print(F("<link rel=\"shortcut icon\" href=\"about:blank\">"));
          client.print(F("</head>\r\n"));
          client.print(F("<body>"));
          client.print(F("<div class=\"grid-container\">"));
          client.print(F("<div class=\"grid-item1\">To Buy!</div>"));
          client.print(F("<div class=\"grid-item12\">"));
          client.print(F("<form action='/' method=get >"));
          client.print(F("Box1 item: <input type=text name='BOX1' value='"));
          client.print(box1.grocery);
          client.print(F("' size='8' maxlength='9'><br>"));
          client.print(F("<input type=submit name='submit' value='Change Box1'>"));
          client.print(F("</form>"));
          client.print(F("<form action='/' method=get >"));
          client.print(F("Box2 item: <input type=text name='BOX2' value='"));
          client.print(box2.grocery);
          client.print(F("' size='8' maxlength='9'><br>"));
          client.print(F("<input type=submit name='submit' value='Change Box2'>"));
          client.print(F("</form>"));
          client.print(F("<form action='/' method=get >"));
          client.print(F("Box3 item: <input type=text name='BOX3' value='"));
          client.print(box3.grocery);
          client.print(F("' size='8' maxlength='9'><br>"));
          client.print(F("<input type=submit name='submit' value='Change Box3'>"));
          client.print(F("</form>"));
          client.print(F("</div>"));
          client.print(F("<div class=\"grid-item2\">"));
          client.print(F("<table>"));
          for (int i = 0; i < 3; i++)
          {
            client.print(*tests[i]);
          }
          client.print(F("</table>"));
          client.print(F("</div>"));
          client.print(F("<div class=\"grid-item3\"><a href=\"/C\">Clear</a></div>"));
          client.print(F("<div class=\"grid-item4\"><a href=\"/\">Refresh</a></div>"));
          client.print(F("</div>"));
          client.print(F("</body>"));
          client.print(F("</html>"));
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

    // Give the web browser time to receive the data
    delay(2000);

    // Close the connection:
    client.stop();
    Serial.println(F("Client disconnected"));
  }
}

void boxCont(String &temp)
{
  while (!buf.endsWith("&"))
  {
    char c = client.read();
    if (c != '&')
    {
      temp += c;
    }
    buf.push(c);
  }
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
  lcd.print(F("ip:"));
  lcd.print(ip);
}
