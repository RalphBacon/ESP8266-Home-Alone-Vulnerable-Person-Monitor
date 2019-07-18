/*
 * ForwardDeclarations.h
 *
 *  Created on: 1 May 2019
 *      Author: Ralph
 */

#ifndef FORWARDDECLARATIONS_H_
#define FORWARDDECLARATIONS_H_

void sendNTPpacket(const char* address);
void printDigits(int digits);
void displayDigits(int digits);
void digitalClockDisplay();
time_t getNTPTime();
void displayWiFi(bool isError = false, String msg = "", int16_t x = 0,
		int16_t y = 0);
void displayNTP(bool isError = false, int16_t x = 0, int16_t y = 9);
void displaySetup(int16_t x = 0, int16_t y = 17);
void displayMoves(bool isAway = false, unsigned int moveCnt = 0, int16_t x = 0,
		int16_t y = 17);
void displayUploadStatus(bool isError = false, std::string msg = "", int16_t x =
		0, int16_t y = 0);
std::string readSDInfo(char* itemRequired);
int readTouch();
void checkActivity();
unsigned int getTimeOutPeriod();
unsigned int getActivityMax();
int uploadData(unsigned int movementCnt);
unsigned int getBeepOnMovement();
unsigned int getEmailTriggerLevel();
void getQuietPeriodHours();
void getEmailAddresses();
unsigned int sendEmailAlert();
std::string getTimeZone();
time_t getLocalTime();
std::string getNTPServer();
byte eRcv();
void getWiFiCredentials();
void WiFiDisconnect();
void connectToWifi();
const char* wl_status_to_string(wl_status_t status);
void screenVisible(bool turnOn = true);
unsigned int getScreenOnSeconds();
unsigned int getThingSpeakFieldNo();
unsigned int getThingSpeakChannelNo();
std::string getThingSpeakWriteAPIKey();
unsigned int minutesInPeriod(int hourFrom, int minuteFrom, int hourTo, int minuteTo);
unsigned int getMinutesBetweenUploads();
unsigned int getMinutesBetweenEmails();
void Serialprint_TimeRightNow();
void displayOLEDClock();

#endif /* FORWARDDECLARATIONS_H_ */
