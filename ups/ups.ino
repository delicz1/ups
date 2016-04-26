/*
Web Server

A simple web server that shows the value of the analog input pins.
using an Arduino Wiznet Ethernet shield.

Circuit:
* Ethernet shield attached to pins 10, 11, 12, 13
* Analog inputs attached to pins A0 through A5 (optional)

created 18 Dec 2009
by David A. Mellis
modified 9 Apr 2012
by Tom Igoe
modified 02 Sept 2015
by Arturo Guadalupi

*/

#include <EEPROM.h>
#include <string.h>
#include <Ethernet.h>

//=============================//
//== KONSTANTY ================//


const unsigned char EEPROM_PASSWORD = 0;
const unsigned char EEPROM_IP = 30;
const unsigned char EEPROM_MASK = 35;

const char DEFAULT_PASSWORD[] = "1234";                // Defaultni heslo
char DEFAULT_IP[5] = { 192,168,137,177 };		   // Defaultni IP


//=============================//
//== Promenne =================//

char password[20];

int counter = 0;
const int passCount = 5;
unsigned long passIDstart[passCount];  // array to record start time of user sessions
unsigned long passIDlast[passCount];   // array to record last time of user sessions
unsigned long sessionId = 0;         // set default as passID : 0 = missing or invalid
unsigned long passExpireMil = 60000;   // expiry time for automatic expiry of valid passID

String readString;                     // used to clear away incoming data that is not required
char buffer[512];                      // buffer for debugging
char xx1[20];                          // temp char for the long converted to char  
char JoinedChar[100];                  // char to hold the joined strings


									   // Enter a MAC address and IP address for your controller below.
									   // The IP address will be dependent on your local network:
uint8_t mac[] = {	0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
EthernetServer server(80);
EthernetClient client;                 // Define a client object

//== EEPROM =================//

/**
* Cte z eeprom od daneho indexu.
* @var index    pocatecni index
* @var output   vysledny text
* @var defaultText pokud v eeprom nic neni kopiruje se do output
* @return 0 pokud v eeprom nic neni jinak delku retezce.
*/
unsigned char readEEPROM(unsigned char index, char *output, char *defaultText) {
	unsigned char len = 0;
	unsigned char val = EEPROM.read(index);
	Serial.println("Read eeprom index");
	Serial.println(index);
	if (val != '\0') {
		while (val != '\0') {
			Serial.print(val);
			output[len] = val;
			len++;
			val = EEPROM.read(++index);
		}
		output[len] = '\0';
	}
	else {
		Serial.print("Pouziti vychozi hodnoty");
		Serial.println(defaultText);
		strcpy(output, defaultText);
	}
	Serial.print("Vysledek nacitani eeprom");
	Serial.println(output);
	return len;
}

void writeEEPROM(unsigned char index, char *input) {
	char *tmp = input;
	while (*tmp) {
		EEPROM.write(index++, *tmp++);
	}
	EEPROM.write(index, '\0');
}

void clearEEPROM() {
	EEPROM.write(EEPROM_PASSWORD, '\0');
	EEPROM.write(EEPROM_IP, '\0');
	EEPROM.write(EEPROM_MASK, '\0');
}


 /*----------------------------------------------------------------------------*/
/* incoming data : Get a single client char                                   */
/*----------------------------------------------------------------------------*/
char gchr(void) {
	while (!client.available());         /* Await data from client           */
	return client.read();               /* Return input character            */
}                                      /* end: gchr()                        */

/*----------------------------------------------------------------------------*/
/* incoming data : Get an entire line from the client                         */
/*----------------------------------------------------------------------------*/
char *glin(char *buf) {
	char c, *p = buf;                    /* Input char, input buffer pointer   */
	while (' ' > (c = gchr()));         /* Discard (leading) control chars    */
	do *p++ = c;                        /* Move input char to line buffer     */
	while (' ' <= (c = gchr()));        /* Until control char encountered     */
	*p = '\0';                          /* Terminate line in buffer           */
	return buf;                         /* Return pointer to input string     */
}   

void initIp() {

	char charIp[5];
	readEEPROM(EEPROM_IP, charIp, DEFAULT_IP);
	Serial.print("nactena ip ");
	uint8_t resultIp[4] = { uint8_t(charIp[0]), uint8_t(charIp[1]), uint8_t(charIp[2]), uint8_t(charIp[3])};
	IPAddress ip(resultIp);
	Ethernet.begin(mac, ip);
	Serial.println("Ip nastavena");
}

void setup() {
	// Open serial communications and wait for port to open:
	Serial.begin(9600);

	while (!Serial) {
		; // wait for serial port to connect. Needed for native USB port only
	}
	Serial.println("Start UPS System");
	Serial.println("clear passCount");
	for (int i = 0; i < passCount; i++) {
		passIDstart[i] = 0;
	}
//	Serial.println("clear eeprom");
//	clearEEPROM();
//
	Serial.println("init ip");
	initIp();
	Serial.println("Start server");
	server.begin();
	Serial.println("Server started");
	Serial.println(Ethernet.localIP());
}

void htmlHeader() {
	client.println("HTTP/1.1 200 OK");
	client.println("Content-Type: text/html");
	client.println("Connection: close");
	client.println();
	client.println("<!DOCTYPE HTML>");
	client.println("<html>");
	client.println("<header>");
	client.println("<meta charset=\"windows1250\">");
	client.println("<title>UPS</title>");
	client.println("</header>");
	client.println("<body>");
}

void htmlFooter() {
	client.println("</body>");
	client.println("</html>");
}

uint8_t findKey(const char *str, char *strbuf, uint8_t maxlen, const char *key)
{
	uint8_t found = 0;
	uint8_t i = 0;
	const char *kp;
	kp = key;
	while (found == 0 && *str != '\0') {
		if (*str == *kp) {
			kp++;
			Serial.print(*str);
			if (*kp == '\0') {
				str++;
				kp = key;
				if (*str == '=') {
					found = 1;
				}
			}
		}
		else {
			kp = key;
		}
		str++;
	}

	if (found == 1) {
		// copy the value to a buffer and terminate it with '\0'
		while (*str &&  *str != ' ' && *str != '\n' && *str != '&' && i<maxlen - 1) {
			//            Serial.print(*str);
			*strbuf = *str;
			i++;
			str++;
			strbuf++;
		}
		*strbuf = '\0';
	}
	// return the length of the value
	return(i);
}

String addParamToUrl(String url, String paramValue) {
	url += (url.lastIndexOf('?') > 0) ? '&' : '?';
	url += paramValue;
	Serial.println("Pridavame do url " + paramValue);
	Serial.println("Vysledna url je " + url);
	return url;
}

String getSessionUrl(String url, unsigned long sessionId) {

	return addParamToUrl(url, "sessionID=" + sessionId);
}

boolean isIp_v4(char* ip, char *setIp) {

	int num;
	int flag = 1;
	int counter = 0;
	uint8_t bIP[] = { 0,0,0,0 };
	char *t = setIp;

	char* p = strtok(ip, ".");

	while (p && flag) {
		num = atoi(p);
		if (num >= 0 && num <= 255 && (counter++<4)) {
			flag = 1;
			p = strtok(NULL, ".");
			*t++ = uint8_t(num);
			if (counter == 1 && num == 0) {
				flag = 0;
			}
		}
		else {
			flag = 0;
		}
	}
	*t = '\0';
	return (flag && (counter == 4));
}


void changeIpPage() {
	char paramData[50];
	if (strstr(buffer, "newIP")) {
		char ipAddr[5];
		findKey(buffer, paramData, 50, "newIP");
		if (isIp_v4(paramData, ipAddr) == false) {
			client.println("<div style=\"font-size:bold; font-color: red;\">Špatná IP</div>");
		} else {
			client.println("<div style=\"font-size:bold; font-color: green;\">IP adressa uložena</div>");
			writeEEPROM(EEPROM_IP, ipAddr);

		}
	}

	client.print("<form method='get' action='/changeIP'>");
	client.print("<input type='text' name='newIP' size='12'>");
	client.print("<input type='hidden' name='sessionID' value='" + String(sessionId) + "' />");
	client.print("<input type='submit' value='Odeslat'>");
	client.print("</form>");
}

void loop() {
	// listen for incoming clients

//	Serial.println("loop");
	unsigned long currentMillis = millis();
	unsigned long tmp = 0;
	char paramData[50];
	client = server.available();
	if (client) {
		htmlHeader();
		Serial.println("new client");
		// an http request ends with a blank line
		while (client.connected()) {
			if (client.available()) {
				glin(buffer);
				Serial.println(buffer);
				// send a standard http response header
				
				Serial.print("Received: '");  // Show what we received from
				Serial.print(buffer);         // the current client
				Serial.println("'");
				sessionId = 0;              // set the passIDvalid to 0

											  // clear expired passIDs from array - housekeeping each time a server connection is made
				for (int i = 0; i < passCount; i++) { // projde pole session
					if (passIDstart[i] != 0) { // nastavena session
						if (currentMillis >= passExpireMil) { //program musi bezet delsi dobu nez je doba vyprseni session
							if (currentMillis - passIDlast[i] >= passExpireMil) { //vyprseni session
								passIDstart[i] = 0;
								Serial.print("Cleared passIDstart ");
								Serial.println(i);
							}
						}
					}
				}


				if (strstr(buffer, "logform")) {    /* Is logform keyword present?         */
					Serial.println("Found logform");
					findKey(buffer, paramData, 50, "logform");
					if (strcmp(paramData, DEFAULT_PASSWORD) == 0) {    /* Is password present?                */
						Serial.println("Found myPass");

						// clear the rest of the incoming buffer
						char c = client.read();
						if (readString.length() < 100) {         // read char by char HTTP request
							readString += c;                       // store characters to string 
						}
						if (c == '\n') {                         // if HTTP request has ended - blank line received
							Serial.println("Header Flushed");
						}

						/* Login form processing */

						// get the first open passID storage space and create a new session ID.
						for (int i = 1; i < passCount; i++) {
							if (sessionId == 0 && passIDstart[i] == 0) {
								passIDstart[i] = currentMillis;  // set the variable to use as the passID
								passIDlast[i] = currentMillis;   // set the last time the passID was used
								sessionId = passIDstart[i];
								Serial.print("New alocated passID = ");
								Serial.print(sessionId);
								Serial.print(" in array space ");
								Serial.println(i);
							}
						}                     // end of loop for get the first open passID storage space
					}
					else {
						client.println("Chyba prihlaseni</br>");
					}
				}
				else {                   // end: if logform in line
					if (strstr(buffer, "sessionID")) { //TODO nacist id session
						findKey(buffer, paramData, 50, "sessionID");
						Serial.print("Found sessionID ");
						Serial.print(paramData);
						Serial.println();
						for (int i = 0; i < passCount; i++) {
							if (sessionId == 0 && passIDstart[i] != 0) {
								ltoa(passIDstart[i], xx1, 10);
								Serial.print("current session id ");
								Serial.print("converted session id ");
								Serial.println(xx1);

								if (strstr(paramData, xx1)) {
									Serial.print("sessionID je v poradku");
									passIDlast[i] = currentMillis;   //renew the last time that the passID was used
									sessionId = passIDstart[i];
									break;
								}
							}
						}
					}
				}
				Serial.println("Processing input");
				
				if (sessionId == 0) {  // this is not a valid passID - ask for the password
					client.print("<form method='get'>");
					client.print("<input type='password' name='logform' size='10'>");
					client.print("<input type='submit' value='Login'>");
					client.print("</form>");
				}

				if (sessionId != 0) {  // this IS a valid passID - display the web page
					client.println("Uživatel pøihlášen</br>");
					if (strstr(buffer, "/changeIP"))
					{
						changeIpPage();
					}
					String link = "<a href='/changeIP?sessionID=" + String(sessionId) + "'>Zmìna IP</a>";
					client.println(link);
				}
				break;
			}
		}
		htmlFooter();
//		// give the web browser time to receive the data
		delay(1);
		// close the connection:
		client.stop();
		Serial.println("client disconnected");
		Ethernet.maintain();
	}
}
