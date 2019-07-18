#include "Includes.h"

// Short cut to std namespace (eg std::string)
using namespace std;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

// A wifi client to let us send emails
WiFiClientSecure client;
std::string SSID;
std::string WiFiPassword;
bool wiFiDisconnected = false;

// Email related variables (may be overwritten by SD card data)
const char mailServer[] = "your mail server name here";
const char* fingerprint = "65fad185d986f93c75ec1b22efab24d899e69046";
unsigned int quietHoursStart = 23;
unsigned int quietHoursEnd = 7;

// Time snapshot, initialised in Setup() so we don't get an email immediately
unsigned int prevEmailHour;
unsigned int prevEmailMin;

// OLED screen associated variables (SD card)
int screenOnSeconds = 15;
bool screenStateOn = true;

// Creating an array of email addresses is tricky in C++
std::string emailAddressList[10] = { };

// Email specific items (overridden from SD card)
unsigned int emailTriggerLevel = 5;
std::string currTimeZone = "GB";
std::string currTimeZoneOffset = "+0000";
unsigned int minutesBetweenEmails = 60;

// An open port we can use for the UDP packets coming back in
unsigned int localPort = 8888;

// WiFi specific defines
#define WIFITIMEOUTSECONDS 20
#define HTTPTIMEOUTSECONDS 10

// this is the "pool" name for any number of NTP servers in the pool.
// If you're not in the UK, use "time.nist.gov"
// Elsewhere: USA us.pool.ntp.org
// Germany: de.pool.ntp.org
// Read more here: http://www.pool.ntp.org/en/use.html
// Overwritten from SD card.
char timeServer[] = "de.pool.ntp.org";

// NTP time stamp is in the first 48 bytes of the message
const int NTP_PACKET_SIZE = 48;

//buffer to hold incoming and outgoing packets
byte packetBuffer[NTP_PACKET_SIZE];

// MOVEMENT RELATED ITEMS
// Keep track of what the PIR state is
bool isDetected = false;
unsigned int movementCnt = 0;
unsigned int emailMovementCnt = 0;

#define BEEP_PIN RX
unsigned int timeOutPeriodSecs;
unsigned int maxActivityPerHour;
unsigned int beepOnMovement = 0;

// PIR module connected to GPIO 0 / D3
#define PIR_PIN 0

// Is person away?
bool isAway = false;

// At least one AWAY upload has been done
bool wasAway = false;

// Exit beeper delay
bool isInExitTimeOutPeriod = false;

// Days of week. Day 1 = Sunday
String DoW[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

// Months
String Months[] { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
					"Oct",
					"Nov", "Dec" };

// How often to resync the time (under normal and error conditions)
// #define _DEBUG_HOMEALONE_

#ifdef _DEBUG_HOMEALONE_
#define RESYNC_UTPSECONDS 60
#define RESYNC_ERROR_SECONDS 10
#else
#define RESYNC_UTPSECONDS 3600
#define RESYNC_ERROR_SECONDS 60
#endif

#define UDP_TIMEOUT_SECONDS 10
bool NtpSyncFailed = true;
bool NtpReadyForUse = false;

// Current time updated every second by clock, used everywhere in this sketch
unsigned int currDay = 0;
unsigned int currMonth = 0;
unsigned int currYear = 0;
unsigned int currWeekday = 0;
unsigned int currHour = 0;
unsigned int currMin = 0;
unsigned int currSec = 0;

// Internal LED is confusing so restate here
#define LED_ON LOW
#define LED_OFF HIGH

// Thingspeak API_SERVER - See SD card for data
const char* API_SERVER = "api.thingspeak.com";
std::string thingSpeakFieldName = "";
unsigned int thingSpeakChannelNo = 0;
std::string thingSpeakWriteAPIKey = "";
unsigned int minutesBetweenUploads = 0;

// Last time we uploaded data to ThingSpeak
unsigned int lastDataUploadHour;
unsigned int lastDataUploadMin;

// Thingspeak path & params on that server
const char* parameters = "/update?api_key=";

// Reboot flag
bool reboot = true;

// forward declarations
#include "ForwardDeclarations.h"

// SCL GPIO5
// SDA GPIO4
#define OLED_RESET 99  // GPIO0 D3
Adafruit_SSD1306 display(OLED_RESET);

// ----------------------------------------------------
// SETUP    SETUP    SETUP    SETUP    SETUP    SETUP
// ----------------------------------------------------
void setup() {

	// Open serial communications and wait for port to open:
	Serial.begin(76800);
	Serial.println(F("Version 4.0 Home Alone May 2019"));

	// PIR is connected to GPIO pin 0 (D3)
	pinMode(PIR_PIN, INPUT);

	// Use RX pin as GPIO (sink current for beeper, so annoying)
	pinMode(RX, FUNCTION_3);
	pinMode(RX, OUTPUT);

	// Initial beep to show the system is running
	for (int cnt = 0; cnt < 3; cnt++) {
		digitalWrite(RX, HIGH);
		delay(100);
		digitalWrite(RX, LOW);
		delay(200);
	}

	// Internal LED (sinks current) also turns on/off twinkle light
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LED_OFF);

	// initialize with the I2C addr 0x3C (for the W64 x H48 OLED)
	display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
	display.setRotation(2);
	display.setTextSize(1);
	display.setCursor(0, 0);
	display.clearDisplay();
	display.display();

	// Get Exit delay when touching Going Out
	timeOutPeriodSecs = getTimeOutPeriod();

	// Get maximum activity per hour that we want to record
	maxActivityPerHour = getActivityMax();

	// Do we want to beep on every movement
	beepOnMovement = getBeepOnMovement();

	// Email trigger level (# of movement in last hour)
	emailTriggerLevel = getEmailTriggerLevel();

	// Get quiet period (defaults to 23:00 to 6:59)
	getQuietPeriodHours();

	// Get current time zone (updates global var 'Timezone')
	currTimeZone = getTimeZone();

	// Get all email addresses (up to 5 supported). Default first email
	// address will be overwritten if there is one on the SD card.
	emailAddressList[0] = "your email address@here.com";
	getEmailAddresses();

	// Get ThingSpeak Channel number, API Key, Field #.
	thingSpeakChannelNo = getThingSpeakChannelNo();

	thingSpeakFieldName = "field";
	char fieldNo[2];
	itoa(getThingSpeakFieldNo(), fieldNo, 10);
	thingSpeakFieldName.append(fieldNo);

	Serial.print("Using ThingSpeak field: '");
	Serial.print(thingSpeakFieldName.c_str());
	Serial.println("'");

	thingSpeakWriteAPIKey = getThingSpeakWriteAPIKey().c_str();
	Serial.print("Using WRITE API Key:");
	Serial.println(thingSpeakWriteAPIKey.c_str());

	// How often to upload data to ThingSpeak?
	minutesBetweenUploads = getMinutesBetweenUploads();

	// How often to send (potential) alert email? Do NOT have too low and
	// MUST be > minutes between uploads
	minutesBetweenEmails = getMinutesBetweenEmails();

	// Get OLED screen on time (seconds)
	screenOnSeconds = getScreenOnSeconds();

	// Get NTP pool (defaults to UK)
	strcpy(timeServer, getNTPServer().c_str());

	// Read WiFi credentials from SD
	getWiFiCredentials();

	// Connect to local wifi, prove this can be done
	int wifiCnt = 0;
	while (WiFi.status() != WL_CONNECTED && wifiCnt < 20) {
		connectToWifi();
		wifiCnt++;
	}

	// If we connected, immediately disconnect otherwise force a reboot
	if (WiFi.status() == WL_CONNECTED) {
		WiFiDisconnect();
	} else {
		while (true) {
			;
		}
	}

	// What port will the UDP/NTP packet respond on?
	Serial.print("Setting UDP port to:");
	Serial.println(localPort);
	Udp.begin(localPort);

	// What is the function that gets the time (in ms since 01/01/1900)?
	// This also synchronises the internal time. MUST succeed for the program to continue.
	Serial.println("Initialising NTP routines");
	int ntpSyncCnt = 0;
	while (NtpSyncFailed && ntpSyncCnt < 20) {
		setSyncProvider(getNTPTime);
		delay(500);
		ntpSyncCnt++;
	}

	// If we cannot sync the NTP server we must force a watch dog restart
	if (NtpSyncFailed) {
		Serial.println("NTP Server did not reply. Forcing a WDT reboot.");
		while (true) {
			;
		}
	}

	// How often should we synchronise the time on this machine (in seconds)?
	// Use 300 for 5 minutes but once an hour (3600) is more than enough usually
	Serial.println("Setting NTP resync seconds");
	setSyncInterval(RESYNC_UTPSECONDS);

	// Built in LED
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LED_OFF);

	//Disconnect WiFi
	WiFiDisconnect();

	// Time snapshot - sets the date/time global variables and prints on screen
//	time_t local_t = getLocalTime();
//	currHour = hour(local_t);
//	currMin = minute(local_t);
//	currSec = second(local_t);
	digitalClockDisplay();

	// Set the previous hour for alert emails so we don't get one too soon.
	// Eg if we start this at 16:59 we would get an alert email at 17:00!
	Serial.println("Setup: setting data upload/email initial times");
	prevEmailMin = currMin;
	prevEmailHour = currHour;

	// We cannot access the time until this is all done (or it crashes
	// when we next try to update the NTP time)
	NtpReadyForUse = true;

	// Ditto for data upload
	lastDataUploadHour = currHour;
	lastDataUploadMin = currMin;

	Serialprint_TimeRightNow();
	Serial.print("Setup: Last hour:minute set to:");
	Serial.print(prevEmailHour);
	Serial.print(":");
	Serial.println(prevEmailMin);

	// Give user a chance to read display
	displaySetup();
	delay(2000);
}

// ----------------------------------------------------
// LOOP    LOOP    LOOP    LOOP    LOOP    LOOP    LOOP
// ----------------------------------------------------
void loop() {
	// Only do things every <period> seconds
	static unsigned long prevMillis = millis();
	static unsigned long awayMillis = millis();
	static unsigned long beepMillis = millis();
	static unsigned int failedEmailCount = 0;
	static unsigned long screenOnTime = millis();

	// Touch (going out) switch
	if (!isAway && readTouch()) {
		isAway = true;
		isInExitTimeOutPeriod = true;
		awayMillis = millis();
		beepMillis = millis();

		// Turn screen on
		screenOnTime = millis();
		digitalWrite(BEEP_PIN, HIGH);
	}

	// Exit beep
	if (isInExitTimeOutPeriod) {
		//Away exit timeout period exceeded? Stop beeper, reset flag, clear movement count
		if (millis() > awayMillis + (timeOutPeriodSecs * 1000)) {
			isInExitTimeOutPeriod = false;
			digitalWrite(BEEP_PIN, LOW);
			movementCnt = 0;
			Serialprint_TimeRightNow();
			Serial.println("Status: AWAY until new movement detected.");
		} else {
			// beep
			if (millis() > beepMillis + 250) {
				digitalWrite(BEEP_PIN, !digitalRead(BEEP_PIN));
				beepMillis = millis();
			}
		}
	}

	// Any movement? Uses global variable 'movementCnt' for the count.
	if (!isInExitTimeOutPeriod) {
		digitalWrite(LED_BUILTIN, LED_OFF);
		checkActivity();

		// Turn on screen on movement
		if (isDetected) {
			screenOnTime = millis();
		}
	} else {
		// Multicoloured LED on during AWAY apartment exit period
		digitalWrite(LED_BUILTIN, LED_ON);
	}

	// This just prints stuff to the OLED every N milliseconds. Note this happens whether the
	// screen is on or off.
	if (millis() > prevMillis + 100) {
		// Keep the display updated with wifi status (will auto reconnect if down)
		displayOLEDClock();

		// Keep note of when we last did this
		prevMillis = millis();
	}

	/*
	 * UPLOAD DATA EVERY Y MINUTES. INDEPENDENT OF X MINUTE EMAIL ROUTINE.
	 */

	// Upload data (activity count) to Cloud?
	if (uploadData(movementCnt)) {

		// Keep/Turn on the screen
		screenOnTime = millis();
		screenVisible(true);

		// If we uploaded then reset the movement count for this period.

		// Data is also uploaded when threshold reached after a "no movement" state
		// or after a successfully uploaded AWAY period

		// Reset the activity count (email has its own count)
		movementCnt = 0;
	}

	// This just prints stuff to the OLED every N milliseconds. Note this happens whether the
	// screen is on or off.
	if (millis() > prevMillis + 100) {
		// Keep the display updated with wifi status (will auto reconnect if down)
		displayOLEDClock();

		// Keep note of when we last did this
		prevMillis = millis();
	}

	/*
	 * SEND EMAIL EVERY X MINUTES IF LOW/NO ACTIVITY OR IMMEDIATELY THERE IS ACTIVITY
	 * AFTER SUCH AS CONDITION
	 */

	// Send email?
	int emailResponse = sendEmailAlert();

	// Did we succeed (in ACTUALLY sending or cancelling)
	// 0 = error in sending, 1 = no need (conditions not met), 2 = mail sent, 3 = cancelled due to new movement
	if (emailResponse > 0) {

		// Keep/Turn on screen if email was ACTUALLY sent. If we cancelled or did not send
		// an email for other reasons the don't activate the screen.
		if (emailResponse == 2) {
			screenOnTime = millis();
			screenVisible(true);
		}

		// If email was sent successfully (or cancelled), reset any email movement value
		failedEmailCount = 0;
	}
	else {
		// If we have tried X times to send email and failed reboot
		if (failedEmailCount > 10) {
			Serialprint_TimeRightNow();
			Serial.println("Sending email failed. Will WDT reboot.");
			while (true) {
				;
			}
		}
		Serialprint_TimeRightNow();
		Serial.println("Sending email failed. Will try again.");
		failedEmailCount++;
	}

	/*
	 * OLED SCREEN BURN PROTECTOR - TURN SCREEN OFF UNLESS SOMETHING IS HAPPENING (ACTIVITY, UPLOAD, EMAIL, AWAY)
	 */

	// Turn screen off if timeout has expired otherwise something has reset count so turn on
	if (millis() > screenOnTime + (screenOnSeconds * 1000)) {
		screenVisible(false);
	} else {
		screenVisible(true);
	}
}

//-----------------------------------------------------------------------------
// Prints a nice time display
//-----------------------------------------------------------------------------
void digitalClockDisplay() {

	// Use the new local time variable in the display
	time_t local_t = getLocalTime();

	// Update global variables used throughout this sketch
	// This is the ONLY place this is done
	currYear = year(local_t);
	currMonth = month(local_t);
	currDay = day(local_t);
	currWeekday = weekday(local_t);
	currHour = hour(local_t);
	currMin = minute(local_t);
	currSec = second(local_t);

	// OLED output
	// Date
	display.setTextColor(WHITE, BLACK);
	display.setCursor(0, 32);
	display.print(DoW[currWeekday - 1]);
	display.print(" ");
	displayDigits(currDay);
	display.print(" ");
	display.print(Months[currMonth - 1]);

	// Time
	display.setCursor(5, 41);
	displayDigits(currHour);
	display.print(":");
	displayDigits(currMin);
	display.print(":");
	displayDigits(currSec);
	display.display();
}

void printDigits(int digits) {
	// helper utility for digital clock display: prints leading 0
	if (digits < 10)
		Serial.print('0');
	Serial.print(digits);
}

void displayDigits(int digits) {
	// helper utility for digital clock display: prints leading 0
	if (digits < 10)
		display.print('0');
	display.print(digits);
}

//-----------------------------------------------------------------------------
// Contact the NTP pool and retrieve the time
//-----------------------------------------------------------------------------
time_t getNTPTime() {
	// turn off LED
	digitalWrite(LED_BUILTIN, HIGH);

	// Get a WiFi connection
	connectToWifi();

	// Send a UDP packet to the NTP pool address
	Serialprint_TimeRightNow();
	Serial.print("Sending NTP packet to ");
	Serial.println(timeServer);
	sendNTPpacket(timeServer);

	// Wait to see if a reply is available - timeout after X seconds. At least
	// this way we exit the 'delay' as soon as we have a UDP packet to process
	int timeOutCnt = 0;
	while (Udp.parsePacket() == 0
			&& ++timeOutCnt
				< (UDP_TIMEOUT_SECONDS * 10))
	{
		delay(100);
	}

	// Is there UDP data present to be processed? Sneak a peek!
	if (Udp.peek() != -1) {
		// We've received a packet, read the data from it
		Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

		// The time-stamp starts at byte 40 of the received packet and is four bytes,
		// or two words, long. First, extract the two words:
		unsigned long highWord = word(packetBuffer[40],
				packetBuffer[41]);
		unsigned long lowWord = word(packetBuffer[42],
				packetBuffer[43]);

		// combine the four bytes (two words) into a long integer
		// this is NTP time (seconds since Jan 1 1900)
		unsigned long secsSince1900 = highWord << 16 | lowWord;

		Serialprint_TimeRightNow();
		Serial.print("Seconds since Jan 1 1900 = ");
		Serial.println(secsSince1900);

		// now convert NTP time into everyday time:
		//Serial.print("Unix time = ");

		// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
		const unsigned long seventyYears = 2208988800UL;

		// subtract seventy years:
		unsigned long epoch = secsSince1900 - seventyYears;

		// Reset the interval to get the time from NTP server in case we previously changed it
		setSyncInterval(RESYNC_UTPSECONDS);

		// LED indication that all is well
		NtpSyncFailed = false;
		digitalWrite(LED_BUILTIN, LOW);
		displayNTP();

		// All done
		WiFiDisconnect();
		return epoch;
	}

	// Failed to get an NTP/UDP response
	Serialprint_TimeRightNow();
	Serial.println("No UTP response.");
	displayNTP(true);
	NtpSyncFailed = true;
	setSyncInterval(RESYNC_ERROR_SECONDS);
	WiFiDisconnect();
	return 0;
}

// Convert UTP time to Local Time
time_t getLocalTime() {
	// We'll grab the time so it doesn't change whilst we're printing it
	// FIXME test goes here
	//Serial.println("getLocalTime - Getting now()");
	time_t t = now();

	// Pointer to the Time Change Rule that was used in the conversion
	//Serial.println("getLocalTime - TCR");
	TimeChangeRule *timeChangeRule;

	// Clocks go forward 1 or 2 hours on last Sunday in March at 1am
	TimeChangeRule GBSpring = { "GB", Last, Sun, Mar, 1, +60 };
	TimeChangeRule EurSpring = { "EUR", Last, Sun, Mar, 1, +120 };

	// Clocks remain at GMT or go forward 1 hour on last Sunday in October at 2am
	TimeChangeRule GBAutumn = { "GB", Last, Sun, Oct, 2, 0 };
	TimeChangeRule EurAutumn = { "EUR", Last, Sun, Oct, 2, +60 };

	// Two timezones
	Timezone GB(GBSpring, GBAutumn);
	Timezone Eur(EurSpring, EurAutumn);

	// Convert to local time
	time_t myTime;

	// Depending on SD card timezone get the local time
	//Serial.println("getLocalTime - Getting local time");
	if (currTimeZone == "GB") {
		myTime = GB.toLocal(t, &timeChangeRule);
	}
	else {
		if (currTimeZone == "EUR") {
			myTime = Eur.toLocal(t);
		}
		else {
			// Default back to GB
			myTime = GB.toLocal(t);
		}
	}

	// Determine the DST offset to GMT
	//Serial.println("getLocalTime - DST");
	if (currTimeZone == "GB") {
		if (GB.locIsDST(myTime)) {
			currTimeZoneOffset = "+0100";
		}
		else {
			currTimeZoneOffset = "+0000";
		}
	} else if (currTimeZone == "EUR") {
		if (Eur.locIsDST(myTime)) {
			currTimeZoneOffset = "+0200";
		} else {
			currTimeZoneOffset = "+0100";
		}
	}

	// All done
	//Serial.println("getLocalTime - All done");
	return myTime;
}

//-----------------------------------------------------------------------------
// Send an NTP request to the time server at the given address
//-----------------------------------------------------------------------------
void sendNTPpacket(const char* address) {
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
	// 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;

	// all NTP fields have been given values, now you can send a packet requesting a timestamp:
	// Note that Udp.begin will request automatic translation (via a DNS server) from a
	// name (eg pool.ntp.org) to an IP address. Never use a specific IP address yourself,
	// let the DNS give back a random server IP address
	Udp.beginPacket(address, 123); //NTP requests are to port 123

	// Get the data back
	Udp.write(packetBuffer, NTP_PACKET_SIZE);

	// All done, the underlying buffer is now updated
	Udp.endPacket();
}

// -----------------------------------------------------------------------
// Establish a WiFi connection with your router
//
// Note: if connection is established, and then lost for some reason, ESP
// will automatically reconnect to last used access point once it is again
// back on-line. This will be done automatically by Wi-Fi library, without
// any user intervention.
// -----------------------------------------------------------------------
// See if the touch switch has been er, well, touched
int readTouch() {
	// Interrupts can cause mis-readings
	noInterrupts();

	// Start the touch timer
	int touch = system_adc_read();

	// Re-enable interrupts
	interrupts();

	if (touch > 100) {
		Serialprint_TimeRightNow();
		Serial.print("Touch:");
		Serial.println(touch);
		return 1;
	}

	// No touch detected
	return 0;
}

// Upload data to the cloud every so often
int uploadData(unsigned int movementCnt) {

	// Previously there was insufficient (or no) activity
	static bool previouslyWasNoMovement = false;

	// Get the current time (may invoke an NTP call)
	// FIXME
//	time_t local_t = getLocalTime();
//	unsigned int currHour = hour(local_t);
//	unsigned int currMin = minute(local_t);

	// How long since last data upload? We'll assume the current year for now
	unsigned minsSinceLastUpload = minutesInPeriod(lastDataUploadHour, lastDataUploadMin, currHour, currMin);

	// If the interval has been reached, upload to cloud.
	//
	// Additionally, we will upload immediately when movement has been detected if the
	// previous upload was for no (or low) activity level. This is an ADDITIONAL
	// upload and subsequent uploads will be re-baselined from this point.

	//If we have reached our next interval
	if (minsSinceLastUpload
		>= minutesBetweenUploads

		// Or we have uploaded at least one AWAY and MUM has now RETURNED
		|| (wasAway && !isAway)

		// Or MUM has just left the building, ie AWAY, but not whilst it's beeping
		// it causes a single, monotonous tone whilst this function runs
		|| (isAway && !wasAway && !isInExitTimeOutPeriod)

		// We send a special value on a reboot so graph is updated
		|| reboot

		// Or we previously uploaded a no/low value but now have something to report ahead of
		// usual reporting interval
		|| (previouslyWasNoMovement && (movementCnt > emailTriggerLevel))) {

		// Do nothing here - drop through. We DO want to upload

	} else {
		// We don't want to upload anything, conditions not correct
		return 0;
	}

	/*
	 *  We are committed to uploading data to the cloud.
	 */

	Serialprint_TimeRightNow();
	Serial.print("Data upload: Minutes since last upload:");
	Serial.println(minsSinceLastUpload);

	Serialprint_TimeRightNow();
	Serial.print("Data upload: Activity count:");
	Serial.println(movementCnt);

	if (wasAway && !isAway) {
		Serialprint_TimeRightNow();
		Serial.println("Data upload: Person has RETURNED");
	}

	if (reboot) {
		Serialprint_TimeRightNow();
		Serial.println("Data upload: reboot in progress.");
	}

	// Update email movement count for the current email reporting period.
	emailMovementCnt += movementCnt;

	// Turn on screen. Calling routine will sort out screen timeout
	screenVisible(true);

	// Connect to Wifi
	connectToWifi();
	if (WiFi.status() != WL_CONNECTED) {
		Serialprint_TimeRightNow();
		Serial.println("Data upload: Unable to upload - wifi connection failed.");
		return 0;
	}

	// Return value from this function
	int response = 0;

	// Progress display
	displayUploadStatus(false, "NOW");

	Serialprint_TimeRightNow();
	Serial.print("Data Upload: Connecting to ");
	Serial.println(API_SERVER);

	int retries = 5;
	while (!client.connect(API_SERVER, 443) && (retries-- > 0)) {
		Serial.print(".");
	}
	Serial.println();

	if (!client.connected()) {
		Serialprint_TimeRightNow();
		Serial.println("Data Upload: Failed to connect. Will retry.");
	}

	// Limit maximum activity count to maxActivityPerHour so we can see the graph data
	// more easily (especially AWAY state)
	if (movementCnt > maxActivityPerHour) {
		movementCnt = maxActivityPerHour;
	}

	// If the isAway flag is set always upload a fixed value of 100 for ease of identification
	if (isAway) {
		movementCnt = 50;
		// Set the wasAway so we report on movement immediately on return
		wasAway = true;

		// Clear the email aggregate movement value as this restarts upon RETURN
		emailMovementCnt = 0;
	}

	// Clear the alert status if previously set (for this routine only)
	if (movementCnt > emailTriggerLevel) {
		previouslyWasNoMovement = false;
	} else {
		previouslyWasNoMovement = true;
	}

	String thingSpeakApiCmd = String("GET ") + parameters + String(thingSpeakWriteAPIKey.c_str())
								+ "&"
								+ String(thingSpeakFieldName.c_str()) + "="
								+ (reboot ? String(-15) : String(movementCnt))
								+ " HTTP/1.1\r\n" + "Host: "
								+ API_SERVER
								+ "\r\n" + "Connection: close\r\n\r\n";

	client.print(thingSpeakApiCmd);

	Serialprint_TimeRightNow();
	Serial.println(thingSpeakApiCmd);

	// Check whether we have a response 10 times a second for X seconds
	int timeout = HTTPTIMEOUTSECONDS * 10;
	while (!client.available() && (timeout-- > 0)) {
		delay(100);
	}

	// Display the server's response messages to serial monitor (debug window), good for initial debugging
	bool uploadSuccessful = false;

	Serialprint_TimeRightNow();
	Serial.println("---- Server Response ----");
	while (client.available()) {
		// See https://arduino.stackexchange.com/questions/10088/what-is-the-difference-between-serial-write-and-serial-print-and-when-are-they
		String responseLine = client.readStringUntil('\n');
		if (responseLine.indexOf("Status: 200 OK")) {
			uploadSuccessful = true;
		}
		Serial.print(responseLine);
	}
	Serial.println("\n--- End Server Response ---");

	// Close the connection and free resources
	Serialprint_TimeRightNow();
	Serial.println("Data Upload: Closing connection.");
	client.stop();

	// Update when we last did the upload
	if (uploadSuccessful) {
		displayUploadStatus(false);
		Serialprint_TimeRightNow();
		Serial.println("Data Upload: Upload successful.");

		// Clear the away flag if Person has returned
		if (wasAway && !isAway) {
			wasAway = false;
		}

		lastDataUploadHour = currHour;
		lastDataUploadMin = currMin;
		response = 1;
	} else {
		displayUploadStatus(true);
		Serialprint_TimeRightNow();
		Serial.println("Data Upload: Upload failed.");
	}

	// Disconnect WiFi
	WiFiDisconnect();

	// As we've already spent an eternity in this routine another second won'local_t matter
	// so the final message can be read on the OLED.
	delay(1000);
	return response;
}

// Upload status (only whilst uploading)
void displayUploadStatus(bool isError, std::string msg, int16_t x, int16_t y) {
	display.setCursor(x, y);
	display.setTextColor(BLACK, WHITE);

	if (isError) {
		display.print("UPLOAD BAD");
	} else {
		if (msg.length() == 0) {
			display.print("UPLOAD  OK");
		} else {
			display.print("UPLOAD NOW");
		}
	}
	display.display();
}

// When no/low movement detected in last X minutes send alert email max ONCE per monitoring period
unsigned int sendEmailAlert() {

	// Flag to remember that we have emailed a no/low alert
	static bool previouslyEmailedLowNoActivity = false;

	// The variables to compare against are GLOBAL as they had to be initialised by the setup()
	// routine: prevEmailHour & prevEmailMin

	// Last successful hour in which an email was sent (or was not required)
	// can be found in global variable 'prevEmailHour'
	// FIXME
//	time_t local_t = getLocalTime();
//	unsigned int currEmailHour = hour(local_t);
//	unsigned int currEmailMin = minute(local_t);
	unsigned int currEmailHour = currHour;
	unsigned int currEmailMin = currMin;

	// If we previously emailed an alert but now have activity set the flag here
	bool sendCalmingEmail = false;
	if (previouslyEmailedLowNoActivity && (emailMovementCnt > emailTriggerLevel)) {
		sendCalmingEmail = true;
		Serial.println("Send email: Activity detected after no/low movement email.");
	}

	// How long since last email due? We'll assume the current year for now
	unsigned minsSinceLastEmail = minutesInPeriod(prevEmailHour, prevEmailMin, currEmailHour, currEmailMin);

	// FIXME Remove these when done
	// Some debugging messages
	if (minsSinceLastEmail >= minutesBetweenEmails) {
		Serialprint_TimeRightNow();
		Serial.print("Send email: Minutes since last check:");
		Serial.println(minsSinceLastEmail);

		Serialprint_TimeRightNow();
		Serial.print("Send email: away status:");
		Serial.println(isAway ? "true" : "false");

		Serialprint_TimeRightNow();
		Serial.print("Send email: email Movement Count:");
		Serial.println(emailMovementCnt);
	}
	// End debugging

	// If AWAY never send an email as movement count is always zero. Check again next period.
	if ((minsSinceLastEmail >= minutesBetweenEmails)) {

		if (isAway) {
			// Update period counters
			prevEmailHour = currEmailHour;
			prevEmailMin = currEmailMin;

			// Reset the email reporting movement counter
			emailMovementCnt = 0;
			previouslyEmailedLowNoActivity = false;

			// All done
			Serialprint_TimeRightNow();
			Serial.println("Send email: as MUM is AWAY email is cancelled this period.");
			return 1;
		}
	}

	// Don't send email if aggregated activity from previously uploaded periods > threshold
	if (minsSinceLastEmail >= minutesBetweenEmails) {
		if (emailMovementCnt > emailTriggerLevel) {
			// No need for email
			Serialprint_TimeRightNow();
			Serial.print("Send email: Email alert cancelled as sufficient movement detected in this period:");
			Serial.println(emailMovementCnt);

			// Update period counters
			prevEmailHour = currEmailHour;
			prevEmailMin = currEmailMin;

			// Reset the email reporting movement counter
			emailMovementCnt = 0;
			previouslyEmailedLowNoActivity = false;
			return 3;
		}
	}

	// If in next email reporting period
	if (minsSinceLastEmail >= minutesBetweenEmails) {

		// And we're not in the quiet period
		if (!(currEmailHour < quietHoursStart			// eg 22:00 NO email at 22:00
				&& currEmailHour >= quietHoursEnd)) {	// eg 08:00 email SENT at 08:00

			// No need for email
			Serialprint_TimeRightNow();
			Serial.println("Send email: Email alert cancelled as within quiet hours");

			// Update period counters
			prevEmailHour = currEmailHour;
			prevEmailMin = currEmailMin;

			// Reset the email reporting movement counter
			emailMovementCnt = 0;
			previouslyEmailedLowNoActivity = false;
			return 3;
		}
	}

	// If in next email reporting period
	if ((minsSinceLastEmail
			>= minutesBetweenEmails

	// And the aggregated activity level from all previously unreported periods is below the threshold
	// && emailMovementCnt <= emailTriggerLevel

	// And we're not in the quiet period
	//&& currEmailHour < quietHoursStart	// eg 22:00 NO email at 22:00
	//&& currEmailHour >= quietHoursEnd		// eg 08:00 email SENT at 08:00
	)
		// Or we rebooting (always email report of crashes/reboots)
		|| reboot

		// Or we previously sent an alert for no/low movement but now there is some
		|| sendCalmingEmail) {

		// Are we sure we STILL want to email an alert? Check the CURRENT movement count.
		if (emailMovementCnt + movementCnt > emailTriggerLevel
			&& !reboot
			&& !sendCalmingEmail) {

			// Don't send an email about no/low movement when we do have some activity now
			Serialprint_TimeRightNow();
			Serial.print("Send email: Email alert cancelled - aggregate movement now:");
			Serial.println(movementCnt + emailMovementCnt);

			// Update period counters
			prevEmailHour = currEmailHour;
			prevEmailMin = currEmailMin;

			// Reset the email reporting movement counter
			emailMovementCnt = 0;
			return 3;
		}
		else {
			// Yes we want to send email
		}
	}
	else {
		return 1;
	}

	Serialprint_TimeRightNow();
	Serial.print("Send email: Minutes since last upload:");
	Serial.println(minsSinceLastEmail);

	if (reboot) {
		Serialprint_TimeRightNow();
		Serial.println("Send email: reboot/crash email to be sent.");
	}

	Serialprint_TimeRightNow();
	Serial.print("Send email: Email movement :");
	Serial.println(emailMovementCnt);

	Serialprint_TimeRightNow();
	Serial.print("Send email: Real Time movement :");
	Serial.println(movementCnt);

	// Turn on screen. Calling routine will sort out screen timeout period
	screenVisible(true);

	// Above 'if' statement will drop through here if email still required
	connectToWifi();

	if (client.connect(mailServer, 465) == 1) {
		Serialprint_TimeRightNow();
		Serial.println(F("Send email: Email server connected"));
	} else {
		Serialprint_TimeRightNow();
		Serial.println(F("Send email: Email server connection failed"));
		return 0;
	}
	if (!eRcv())
		return 0;

	Serialprint_TimeRightNow();
	Serial.println(F("Sending EHLO"));
	client.println("EHLO jargongeneration.com");
	if (!eRcv())
		return 0;

	Serialprint_TimeRightNow();
	Serial.println(F("Sending auth login"));
	client.println("auth login");
	if (!eRcv())
		return 0;

	Serialprint_TimeRightNow();
	Serial.println(F("Sending User"));
	// Change to your base64, ASCII encoded user
	client.println(base64::encode("arduino@jargongeneration.uk"));
	if (!eRcv())
		return 0;

	Serialprint_TimeRightNow();
	Serial.println(F("Sending Password"));
	// change to your base64, ASCII encoded password
	client.println(base64::encode("mr$eal89!JG*"));
	if (!eRcv())
		return 0;

	Serialprint_TimeRightNow();
	Serial.println(F("Sending From"));
	// change to your email address (sender)
	client.println(F("MAIL From: rsmbacon@gmail.com"));
	if (!eRcv())
		return 0;

	// Send to all on the list
	for (int cnt = 0; cnt < 10; cnt++) {
		if (emailAddressList[cnt] != "") {
			Serialprint_TimeRightNow();
			Serial.print(F("Sending To: "));
			Serial.println(emailAddressList[cnt].c_str());
			client.print(F("RCPT To: "));
			client.println(emailAddressList[cnt].c_str());
			if (!eRcv())
				return 0;
		}
	}

	Serialprint_TimeRightNow();
	Serial.println(F("Sending DATA (email body)"));
	client.println(F("DATA"));
	if (!eRcv())
		return 0;

	// Send date so it appears correctly in an Inbox regardless of when it was received
	// Example format: Date: Fri, 01 Mar 2019 08:24:00 +0100
	String sentDateTime = "Date: " + DoW[currWeekday - 1];
	sentDateTime.concat(", ");
	sentDateTime.concat(currDay);
	sentDateTime.concat(" ");
	sentDateTime.concat(Months[currMonth - 1]);
	sentDateTime.concat(" ");

	// Buffer to hold two digits values for date/time (4 digits for year)
	char buffer[5];
	sprintf(buffer, "%04u", currYear);
	sentDateTime.concat(buffer);
	sentDateTime.concat(" ");

	sprintf(buffer, "%02u", currHour);
	sentDateTime.concat(buffer);
	sentDateTime.concat(":");

	sprintf(buffer, "%02u", currMin);
	sentDateTime.concat(buffer);
	sentDateTime.concat(":");

	sprintf(buffer, "%02u", currSec);
	sentDateTime.concat(buffer);

	sentDateTime.concat(" ");

	sentDateTime.concat(currTimeZoneOffset.c_str());
	client.println(sentDateTime);

	Serialprint_TimeRightNow();
	Serial.print("Mail sent at: ");
	Serial.println(sentDateTime);

	// change to recipient addresses from SD card
	// This is seen by recipient in email (can be any fictitious email address)
	client.println(F("To:  baconfamily@MUMwatch.de"));

	// This is the bit that is seen by the recipient
	client.println(F("From: HomeAlone@gmail.com"));

	// Reboot flag - controls sending of first mail text
	if (reboot) {
		client.println(F("Subject: Smart Care - Device Reboot"));

		client.println(
				F("The Home Alone device has been restarted (or it crashed)."));
		client.println(
				F(
						"If this was expected then no further action needs to be taken.\n"));
		client.println(
				F(
						"If this was not expected you may wish to investigate further.\n"));
		client.println(
				F(
						"Note: The monitoring device may reboot itself if it fails to"));
		client.println(F("contact an email or NTP server in a timely manner.\n"));

		// Clear the reboot flag
		reboot = false;
	}
	// Is this a special case to send an email?
	else if (sendCalmingEmail) {
		client.println(F("Subject: Smart Care - Movement Detected"));
		client.print(F("There were "));
		client.print(emailMovementCnt);
		client.println(
				F(
						" activity movements detected recently after a period of no/low activity.\n"));
		client.println(
				F(
						"You should not get the another alert email for this period.\n"));

		// Clear the flag
		previouslyEmailedLowNoActivity = false;
		sendCalmingEmail = false;
	}
	else {
		client.println(F("Subject: Smart Care  - No/Low Movement Alert"));
		client.print(F("During the hour(s) "));

		if (prevEmailHour < 10)
			client.print("0");
		client.print(prevEmailHour);
		client.print(F(":"));
		if (prevEmailMin < 10)
			client.print("0");
		client.print(prevEmailMin);
		client.print(F(" to "));
		if (currEmailHour < 10)
			client.print("0");
		client.print(currEmailHour);
		client.print(F(":"));
		if (currEmailMin < 10)
			client.print("0");
		client.print(currEmailMin);
		client.print(F("(approx) there were "));
		client.print(emailMovementCnt);
		client.println(
				F(" activity movements detected. Please check all is well.\n"));
		client.println(
				F("You may get another alert email for this period.\n"));

		// Remember that we did this alert
		previouslyEmailedLowNoActivity = true;
	}

	// Append the date/time this email was created
	client.println(sentDateTime);

	// We MUST terminate the body with a single dot. NB previous line must be a println not just print
	Serialprint_TimeRightNow();
	Serial.println(F("Terminating email (single dot)"));
	client.println(".");
	if (!eRcv())
		return 0;

	Serialprint_TimeRightNow();
	Serial.println(F("Sending QUIT"));
	client.println(F("QUIT"));
	if (!eRcv())
		return 0;

	client.stop();
	Serialprint_TimeRightNow();
	Serial.println(F("Send email: Client disconnected (stop)"));

	// Update the hour of the successful email sending
	prevEmailHour = currEmailHour;
	prevEmailMin = currEmailMin;

	// Close Wifi
	WiFiDisconnect();

	// All done - return that we ACTUALLY sent an email
	emailMovementCnt = 0;
	return 2;
}

// Email error checking routine

// Check email response
byte eRcv() {
	byte respCode;
	byte thisByte;
	int loopCount = 0;

	while (!client.available()) {
		delay(1);
		loopCount++;
		// if nothing received for X seconds, timeout
		if (loopCount > HTTPTIMEOUTSECONDS * 1000 * 2) {
			client.stop();
			WiFiDisconnect();
			Serial.println(F("Timeout"));
			return 0;
		}
	}

	// Grab the response so it can be returned
	respCode = client.peek();
	while (client.available()) {
		thisByte = client.read();
		Serial.write(thisByte);
	}

	if (respCode >= '4') {
		Serial.print("Failed in eRcv with response: ");
		Serial.print(respCode);
		return 0;
	}

	// All done
	return 1;
}

// Read the WiFi Credentials from SD
void getWiFiCredentials() {

	std::string ssid = readSDInfo((char *) "SSID");
	std::string password = readSDInfo((char *) "Password");

	SSID = ssid == "" ? "MySSIDnameHere" : ssid;
	WiFiPassword = password == "" ? "MyWiFiPassword" : password;
}

// Switch off WiFi
void WiFiDisconnect() {
	if (!wiFiDisconnected) {
		Serialprint_TimeRightNow();
		Serial.println("Wifi disconnecting from network.");

		WiFi.disconnect();
		wiFiDisconnected = true;

		Serialprint_TimeRightNow();
		Serial.println("WiFi disconnected.");
	}
}

// Convert the WiFi (error) response to a string we can understand
const char* wl_status_to_string(wl_status_t status) {
	switch (status) {
	case WL_NO_SHIELD:
		return "WL_NO_SHIELD";
	case WL_IDLE_STATUS:
		return "WL_IDLE_STATUS";
	case WL_NO_SSID_AVAIL:
		return "WL_NO_SSID_AVAIL";
	case WL_SCAN_COMPLETED:
		return "WL_SCAN_COMPLETED";
	case WL_CONNECTED:
		return "WL_CONNECTED";
	case WL_CONNECT_FAILED:
		return "WL_CONNECT_FAILED";
	case WL_CONNECTION_LOST:
		return "WL_CONNECTION_LOST";
	case WL_DISCONNECTED:
		return "WL_DISCONNECTED";
	default:
		return "UNKNOWN";
	}
}

// Turn OLED screen on or off to prevent screen burn
void screenVisible(bool turnOn) {
	if (turnOn && !screenStateOn) {
		display.ssd1306_command(SSD1306_DISPLAYON);
		//Serialprint_TimeRightNow();
		//Serial.println("Turning screen ON");
		screenStateOn = true;
	} else if (!turnOn && screenStateOn) {
		display.ssd1306_command(SSD1306_DISPLAYOFF);
		//Serialprint_TimeRightNow();
		//Serial.println("Turning screen OFF");
		screenStateOn = false;
	}
}

// Calculate how many minutes in the period
unsigned int minutesInPeriod(int hourFrom, int minuteFrom, int hourTo, int minuteTo) {
	int totMinutes = 0;

	// If we are going backwards in time add a day
	// eg 23:45 vs 00:15 turns 00:15 into 24:15 then it works
	if (hourTo < hourFrom) {
		hourFrom += 24;
	}

	// Hours first. If we have past the hour subtract 1 from hours total
	if (minuteTo < minuteFrom)
		totMinutes += (hourTo - hourFrom - 1) * 60;
	else
		totMinutes += (hourTo - hourFrom) * 60;

	// If we have gone past the hour
	if (minuteTo >= minuteFrom)
		totMinutes += minuteTo - minuteFrom;
	else
		totMinutes += (60 - minuteFrom) + minuteTo;

	// All done
	return totMinutes;
}

void connectToWifi() {

	Serialprint_TimeRightNow();
	Serial.print("Connecting to SSID: ");
	Serial.println(SSID.c_str());

	WiFi.mode(WIFI_STA);
	WiFi.begin(SSID.c_str(), WiFiPassword.c_str());

	// This may have been the culprit to wifi not being connected after a while
	WiFi.setSleepMode(WIFI_NONE_SLEEP);

	// OLED display
	displayWiFi(false, "WAIT");

	// Try to connect 4 times a second for X seconds before timing out
	int timeout = WIFITIMEOUTSECONDS * 4;
	while (WiFi.status() != WL_CONNECTED && (timeout-- > 0)) {
		delay(250);
		Serial.print(".");
	}

	// Successful connection?
	wl_status_t wifiStatus = WiFi.status();
	if (wifiStatus != WL_CONNECTED) {
		Serialprint_TimeRightNow();
		Serial.println("\nFailed to connect, exiting");
		// Show the error
		displayWiFi(true);

		Serialprint_TimeRightNow();
		Serial.print("WiFi Status: ");
		Serial.println(wl_status_to_string(wifiStatus));
		return;
	}

	Serial.print("\n");
	Serialprint_TimeRightNow();
	Serial.print("WiFi connected with (local) IP address of: ");
	Serial.println(WiFi.localIP());
	wiFiDisconnected = false;
	displayWiFi();
}

// Wifi state to OLED
void displayWiFi(bool isError, String msg, int16_t x, int16_t y) {
	display.setCursor(x, y);
	if (isError) {
		display.setTextColor(BLACK, WHITE);
		display.print("WIFI   BAD");
	} else {
		display.setTextColor(WHITE, BLACK);
		display.print(
				String(msg) == "" ? "WIFI    OK" : "WIFI  " + msg);
	}
	display.display();
}

// NTP state to OLED
void displayNTP(bool isError, int16_t x, int16_t y) {
	display.setCursor(x, y);
	if (isError) {
		display.setTextColor(BLACK, WHITE);
		display.print("NTP    BAD");
	} else {
		display.setTextColor(WHITE, BLACK);
		display.print("NTP     OK");
	}
	display.display();
}

// Setup state to OLED (overwritten afterwards by movement count)
void displaySetup(int16_t x, int16_t y) {
	display.setCursor(x, y);
	display.setTextColor(WHITE, BLACK);
	display.println("SETUP   OK");
	display.display();
}

// Movement count in last hour to OLED
void displayMoves(bool isAway, unsigned int moveCnt, int16_t x, int16_t y) {
	display.setCursor(x, y);

	// Movement count (in any hour) restricted to 50
	if (isAway) {
		display.setTextColor(BLACK, WHITE);
		display.print("MUM IS OUT");
	} else {
		if (moveCnt > maxActivityPerHour) {
			moveCnt = maxActivityPerHour;
		}

		// Two characters to print (fixed width)
		display.setTextColor(WHITE, BLACK);
		char strMoves[2];
		sprintf(strMoves, "%02d", moveCnt);
		display.print("MOVES   ");
		display.print(strMoves);
	}
	display.display();
}

// SD card reader (done ONCE in setup)
std::string readSDInfo(char* itemRequired) {

	char startMarker = '<';
	char endMarker = '>';
	char* receivedChars = new char[32];
	int charCnt = 0;
	char data;
	bool foundKey = false;

	Serial.print("Looking for key '");
	Serial.print(itemRequired);
	Serial.println("'.");

	// Start the SD process
	SD.begin();

	// Get a handle to the file
	File configFile = SD.open("config.txt", FILE_READ);

	// Look for the required key
	while (configFile.available()) {
		charCnt = 0;

		// Read until start marker found
		while (configFile.available() && configFile.peek()
											!= startMarker) {
			// Do nothing, ignore spurious chars
			data = configFile.read();
			//Serial.print("Throwing away preMarker:");
			//Serial.println(data);
		}

		// If EOF this is an error
		if (!configFile.available()) {
			// Abort - no further data
			continue;
		}

		// Throw away startMarker char
		configFile.read();

		// Read all following characters as the data (key or value)
		while (configFile.available() && configFile.peek() != endMarker) {
			data = configFile.read();
			receivedChars[charCnt] = data;
			charCnt++;
		}

		// Terminate string
		receivedChars[charCnt] = '\0';

		// If we previously found the matching key then return the value
		if (foundKey)
			break;

		//Serial.print("Found: '");
		//Serial.print(receivedChars);
		//Serial.println("'");
		if (strcmp(receivedChars, itemRequired) == 0) {
			//Serial.println("Found matching key - next string will be returned");
			foundKey = true;
		}
	}

	// Terminate file
	configFile.close();

	// Did we find anything
	Serial.print("SD card key '");
	Serial.print(itemRequired);
	if (charCnt == 0) {
		Serial.println("' not found.");
		return "";
	} else {
		Serial.print("': '");
		Serial.print(receivedChars);
		Serial.println("'");
	}

	return receivedChars;
}

// All quiet on the western front or do we have activity?
void checkActivity() {
	// If the PIR has detected something and we haven't noted this (first time)
	// - enable the on-board LED
	// - display something on the debug window
	// - issue a tiny beep to the beeper (or LED)
	// - remember we have done this by setting the isDetected flag
	if (digitalRead(PIR_PIN) == LOW && !isDetected) {
		digitalWrite(LED_BUILTIN, LOW);
		//Serial.println("PIR detected movement.");
		isDetected = true;

		// If there is movement, person is not away (or it's a thief)
		if (isAway) {
			isAway = false;
			Serial.println("Status: AWAY has been cancelled by movement.");
		}

		// Beep (more a bip really) but REALLY annoying nonetheless
		if (beepOnMovement) {
			digitalWrite(RX, HIGH);
			delay(50);
			digitalWrite(RX, LOW);
		}
		if (movementCnt < maxActivityPerHour)
			movementCnt++;
	}
	// Otherwise if the PIR has not detected anything and we have not noted this
	else if (digitalRead(PIR_PIN) == HIGH && isDetected) {
		// Turn off the on-board LED, reset the flag and write something to debug window
		digitalWrite(LED_BUILTIN, HIGH);
		digitalWrite(RX, LOW);
		isDetected = false;
		//Serial.println("PIR waiting for movement.");
	}
}

// Read config SD card for whether we give a small bip on movement
unsigned int getBeepOnMovement() {
	std::string beepOnMovement = readSDInfo((char *) "BeepOnMovement");
	if (beepOnMovement == "") {
		return 0;
	} else {
		return std::atoi(beepOnMovement.c_str());
	}
}

// Get maximum activity level per hour we want to record
unsigned int getActivityMax() {
	std::string maxActivityCount = readSDInfo(
			(char *) "MaxActivityCount");
	if (maxActivityCount == "") {
		return 30;
	} else {
		return std::atoi(maxActivityCount.c_str());
	}
}

// Return a valid number from a string
unsigned int getTimeOutPeriod() {
	std::string timeOutPeriodString = readSDInfo(
			(char *) "TimeOutPeriod");
	if (timeOutPeriodString == "") {
		return 30;
	} else {
		return std::atoi(timeOutPeriodString.c_str());
	}
}

// Get OLED screen timeout period
unsigned int getScreenOnSeconds() {
	std::string screenTimeOutSeconds = readSDInfo(
			(char *) "ScreenTimeOutSeconds");
	if (screenTimeOutSeconds == "") {
		return 15;
	} else {
		return std::atoi(screenTimeOutSeconds.c_str());
	}
}

// Email alert activity trigger level
unsigned int getEmailTriggerLevel() {
	std::string emailTriggerLevel = readSDInfo(
			(char *) "emailTriggerLevel");
	if (emailTriggerLevel == "") {
		return 5;
	} else {
		return std::atoi(emailTriggerLevel.c_str());
	}
}

// Get the ThingSpeak channel to send info
unsigned int getThingSpeakChannelNo() {
	std::string thingSpeakChannelNo = readSDInfo(
			(char *) "ThingSpeakChannelNo");
	if (thingSpeakChannelNo == "") {
		return 768404;
	} else {
		return std::atoi(thingSpeakChannelNo.c_str());
	}
}

// Get the ThingSpeak channel to send info
unsigned int getThingSpeakFieldNo() {
	std::string thingSpeakFieldNo = readSDInfo(
			(char *) "ThingSpeakFieldNo");
	if (thingSpeakFieldNo == "") {
		return 1;
	} else {
		return std::atoi(thingSpeakFieldNo.c_str());
	}
}

// Minutes between data uploads to ThingSpeak
unsigned int getMinutesBetweenUploads() {
	std::string minutesBetweenUploads = readSDInfo(
			(char *) "MinutesBetweenUploads");
	if (minutesBetweenUploads == "") {
		return 30;
	} else {
		return std::atoi(minutesBetweenUploads.c_str());
	}
}

// Minutes between alert emails
unsigned int getMinutesBetweenEmails() {
	std::string minutesBetweenEmails = readSDInfo(
			(char *) "MinutesBetweenEmails");
	if (minutesBetweenEmails == "") {
		return 60;
	} else {
		return std::atoi(minutesBetweenEmails.c_str());
	}
}

// Get the ThingSpeak WRITE API Key
std::string getThingSpeakWriteAPIKey() {
	std::string thingSpeakWriteAPIKey = readSDInfo((char *) "ThingSpeakWriteAPIKey");
	if (thingSpeakWriteAPIKey == "") {
		return "11YYDAWC9B51U96N";
	} else {
		return thingSpeakWriteAPIKey;
	}
}

// Get quiet period - update global vars directly
void getQuietPeriodHours() {
	std::string quietPeriodStart = readSDInfo(
			(char *) "QuietHoursStart");
	if (quietPeriodStart == "") {
		quietHoursStart = 23;
	} else {
		quietHoursStart = std::atoi(quietPeriodStart.c_str());
	}

	std::string quietPeriodEnd = readSDInfo((char *) "QuietHoursEnd");
	if (quietPeriodEnd == "") {
		quietHoursEnd = 7;
	} else {
		quietHoursEnd = std::atoi(quietPeriodEnd.c_str());
	}
}

// Get the time zone from SD
std::string getTimeZone() {
	std::string currTimeZone = readSDInfo((char *) "Timezone");
	if (currTimeZone == "") {
		return "GB";
	} else {
		return currTimeZone;
	}
}

// Get email addresses (up to 10)
void getEmailAddresses() {

	// Total of the number of email addresses we find on the SD card. <EmailX><you@home.com>
	int emailAddressCnt = 0;

	//Loop through all potential SD keys of Email + X where X is 1 to 9
	for (int cnt = 1; cnt < 10; cnt++) {
		string emailAddress;
		emailAddress.append("Email");

		// Convert the count into a (one character) string followed by \0
		char myCount[2];
		itoa(cnt, myCount, 10);
		emailAddress.append(myCount);

		// Convert the email address string into a null terminated char*
		char *emailKey = &emailAddress[0u];

		// Go find it
		std::string emailAddressFound = readSDInfo(emailKey);
		if (emailAddressFound != "") {
			emailAddressList[emailAddressCnt++] = emailAddressFound;
		}
	}
}

// Read NTP server pool from SD card <NTPpool>
std::string getNTPServer() {
	std::string ntpPool = readSDInfo(
			(char *) "NTPpool");
	if (ntpPool == "") {
		return "de.pool.ntp.org";
	} else {
		return ntpPool;
	}
}

// Prints the current time (if available) in square brackets: [12:32:12]
void Serialprint_TimeRightNow() {

	// Buffer to hold the two digits
	char buffer[5];

	// Hours
	Serial.print(F("["));
	sprintf(buffer, "%02d", currHour);
	Serial.print(buffer);
	Serial.print(":");

	// Minutes
	sprintf(buffer, "%02d", currMin);
	Serial.print(buffer);
	Serial.print(":");

	// Seconds
	sprintf(buffer, "%02d", currSec);
	Serial.print(buffer);
	Serial.print(F("] "));

//		Serial.print(F("["));
//		if (currHour < 10)
//			Serial.print("0");
//		Serial.print(currHour);
//		Serial.print(":");
//
//		if (currMin < 10)
//			Serial.print("0");
//		Serial.print(currMin);
//		Serial.print(":");
//
//		if (currSec < 10)
//			Serial.print("0");
//		Serial.print(currSec);
//		Serial.print(F("] "));
}

// Display the clock (plus other bits). The most important function here is
// that the clock (global vars) is updated
void displayOLEDClock()
{
	// Keep the display updated with wifi status (will auto reconnect if down)
	int wifiStatus = WiFi.status();
	if (wiFiDisconnected) {
		displayWiFi(false, " ZZZ");
	} else {
		displayWiFi(wifiStatus == WL_CONNECTED ? true : false);
	}
	// PIR count (goes high for a few seconds on movement)
	displayMoves(isAway, movementCnt);

	// Display the date/time
	digitalClockDisplay();
}
