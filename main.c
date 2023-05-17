// Author: Adam Jetmar, xjetma02
// Date: 16.12.2022


#include "MK60D10.h"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define LED1 0x20
#define LED2 0x10
#define LED3 0x8
#define LED4 0x4

#define STRING_LENGTH 80

unsigned int secondsClock;
unsigned int secondsAlarm;
unsigned int lightSignalingType;
unsigned int soundSignalingType;
unsigned int numOfReps;
unsigned int intervalLength;
unsigned int secondsConversion;


typedef enum {
	PROMPT,
	WORKING,
	END
} State;

// string is used for loading input from stdin and for uploading output to stdout
char string[STRING_LENGTH];


void wait(unsigned long long int time) {
	for(unsigned long int i = 0; i < time; i++);
}

// initialize ports
void initializePorts() {
	// enable clocks
    SIM->SCGC1 = SIM_SCGC1_UART5_MASK;
    SIM->SCGC5 = SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTE_MASK;
    SIM->SCGC6 = SIM_SCGC6_RTC_MASK;
    PORTA->PCR[4] = PORT_PCR_MUX(0x01);
    PORTB->PCR[3] = PORT_PCR_MUX(0x01); // LED
    PORTB->PCR[4] = PORT_PCR_MUX(0x01); // LED
    PORTB->PCR[2] = PORT_PCR_MUX(0x01); // LED
    PORTB->PCR[5] = PORT_PCR_MUX(0x01); // LED
    PORTE->PCR[8]  = PORT_PCR_MUX(0x03); // UART
    PORTE->PCR[9]  = PORT_PCR_MUX(0x03); // UART
    PORTE->PCR[10] = PORT_PCR_MUX(0x01);
    PORTE->PCR[11] = PORT_PCR_MUX(0x01);
    PORTE->PCR[12] = PORT_PCR_MUX(0x01);
    PORTE->PCR[26] = PORT_PCR_MUX(0x01);
    PORTE->PCR[27] = PORT_PCR_MUX(0x01);
    PTA->PDDR =  GPIO_PDDR_PDD(0x0010);
    PTB->PDDR =  GPIO_PDDR_PDD(0x3C);
    PTB->PDOR |= GPIO_PDOR_PDO(0x3C); // turn off leds
}

// Init MCU
void initializeMCU() {
    MCG_C4 |= ( MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS(0x01) );
    WDOG_STCTRLH &= ~WDOG_STCTRLH_WDOGEN_MASK;
    SIM_CLKDIV1 |= SIM_CLKDIV1_OUTDIV1(0x00);
}

// Init UART
void initializeUART() {
    UART5->C2  &= ~(UART_C2_TE_MASK | UART_C2_RE_MASK);
    UART5->S2  |= 0xC0;
    UART5->BDH =  0x00;
    UART5->C3  =  0x00;
    UART5->C1  =  0x00;
    UART5->C2  |= ( UART_C2_TE_MASK | UART_C2_RE_MASK );
    UART5->BDL =  0x1A;
    UART5->C4  =  0x0F;
    UART5->MA1 =  0x00;
    UART5->MA2 =  0x00;
}

void initializeRTC() {
	RTC_CR |= RTC_CR_SWR_MASK;
	RTC_CR &= ~RTC_CR_SWR_MASK;
	RTC_TCR = 0x0000;
	RTC_CR |= RTC_CR_OSCE_MASK;
	wait(0x600000);
	RTC_SR &= ~RTC_SR_TCE_MASK;
	RTC_TSR = 0x00000000;
	RTC_TAR = 0xFFFFFFFF;

	RTC_IER |= RTC_IER_TAIE_MASK;
	NVIC_ClearPendingIRQ(RTC_IRQn);
	NVIC_EnableIRQ(RTC_IRQn);
	RTC_SR |= RTC_SR_TCE_MASK;
}



char receiveChar() {
	while(!(UART5->S1 & UART_S1_RDRF_MASK));
	return UART5->D;
}

void sendChar(char c) {
	while(!(UART5->S1 & UART_S1_TC_MASK) && !(UART5->S1 & UART_S1_TDRE_MASK));
	UART5->D = c;
}

void sendStdout(char *s) {
	for(int i = 0; s[i] != '\0'; i++) {
		sendChar(s[i]);
	}
}

void receiveStdin() {
	char temp;
	int j;
	// clear string variable
	for(int i = 0; i < STRING_LENGTH; i++) {
		string[i]='\0';
	}

	// load all the chars into string variable one by one
	for(j = 0; j < (STRING_LENGTH - 1); j++) {
		temp = receiveChar();
		sendChar(temp);

		if(temp == '\r') {
			break;
		}

		string[j] = temp;
	}

	string[j] = '\0'; // the last char of string variable indicates end of string
	sendStdout("\r\n");
}

void sound() {
	for(int i = 0; i < 700; i++) {
		PTA->PDOR = GPIO_PDOR_PDO(0x0010);
		wait(700);
		PTA->PDOR = GPIO_PDOR_PDO(0x0000);
		wait(700);
	}
}

void soundSignaling(int option) {
	if(option == 1) {	// one loud sound
		for(int i = 0; i < 10; i++) {
			sound();
		}
	}

	else if(option == 2) { // beeping with short pauses
		for(int i = 0; i < 10; i++) {
			sound();
			wait(50000);
		}
	}

	else if(option == 3) {
		for(int i = 0; i < 6; i++) { // beeping with even shorter pauses
			sound();
			wait(15000);
		}
	}


}

void lightSignaling(int option) {
	if(option == 1) { // leds blink one after another
		for(int i = 0; i < 10; i++) {
			GPIOB_PDOR ^= LED1;
			PTB->PDOR &= ~GPIO_PDOR_PDO(0x1);
			wait(500000);
			PTB->PDOR |= GPIO_PDOR_PDO(0x3C);

			GPIOB_PDOR ^= LED2;
			PTB->PDOR &= ~GPIO_PDOR_PDO(0x1);
			wait(500000);
			PTB->PDOR |= GPIO_PDOR_PDO(0x3C);

			GPIOB_PDOR ^= LED3;
			PTB->PDOR &= ~GPIO_PDOR_PDO(0x1);
			wait(500000);
			PTB->PDOR |= GPIO_PDOR_PDO(0x3C);

			GPIOB_PDOR ^= LED4;
			PTB->PDOR &= ~GPIO_PDOR_PDO(0x1);
			wait(500000);
			PTB->PDOR |= GPIO_PDOR_PDO(0x3C);
		}
	}

	else if(option == 2)  { // all the leds blink
		for(int i = 0; i < 10; i++) {
			PTB->PDOR &= ~GPIO_PDOR_PDO(0x3C);
			wait(500000);
			PTB->PDOR |= GPIO_PDOR_PDO(0x3C);
			wait(500000);
		}
	}

	else if(option == 3) { // two leds on the left side blink and then two leds on the right side blink
		for(int i = 0; i < 10; i++) {
			GPIOB_PDOR ^= (LED3 | LED4);
			PTB->PDOR &= ~GPIO_PDOR_PDO(0x1);
			wait(500000);
			PTB->PDOR |= GPIO_PDOR_PDO(0x3C);
			wait(500000);

			GPIOB_PDOR ^= (LED1 | LED2);
			PTB->PDOR &= ~GPIO_PDOR_PDO(0x1);
			wait(500000);
			PTB->PDOR |= GPIO_PDOR_PDO(0x3C);
			wait(500000);
		}
	}
}

int convertTimeToSeconds(char *dateTime, unsigned int *seconds) {
	struct tm timeStruct;
	int count;

	count = sscanf(dateTime, "%d-%d-%d %d:%d:%d", &timeStruct.tm_year, &timeStruct.tm_mon, &timeStruct.tm_mday, &timeStruct.tm_hour, &timeStruct.tm_min, &timeStruct.tm_sec);
	if(count != 6) {
		sendStdout("ERROR, wrong input given\r\n");
	}

	// verify date
	if(timeStruct.tm_year < 1970 || timeStruct.tm_year > 2023) {
		sendStdout("Wrong input year given\r\n");
		return 1;
	}
	if(timeStruct.tm_mon < 1 || timeStruct.tm_mon > 12) {
		sendStdout("Wrong input month given\r\n");
		return 1;
	}
	if(timeStruct.tm_mday > 31) {
		sendStdout("Wrong input month given\r\n");
		return 1;
	}

	// verify time
	if(timeStruct.tm_hour > 23 || timeStruct.tm_hour < 0) {
		sendStdout("Wrong input hour given\r\n");
		return 1;
	}
	if(timeStruct.tm_min > 59 || timeStruct.tm_min < 0) {
			sendStdout("Wrong input minute given\r\n");
			return 1;
	}
	if(timeStruct.tm_sec > 59 || timeStruct.tm_sec < 0) {
			sendStdout("Wrong input second given\r\n");
			return 1;
	}

	timeStruct.tm_year = timeStruct.tm_year - 1900;
	timeStruct.tm_mon = timeStruct.tm_mon - 1;
	timeStruct.tm_isdst = -1;

	time_t conv;
	conv = mktime(&timeStruct);
	*seconds = (unsigned int)conv;

	return 0;
}

void convertSecondsToTime(unsigned int *seconds, char *datetime) {
	time_t temp = *seconds;
	struct tm timeStructure = *localtime(&temp);
	for(unsigned int i = 0; i < STRING_LENGTH; i++) {
		datetime[i] = '\0';
	}
	strftime(datetime, STRING_LENGTH, "%Y-%m-%d %H:%M:%S", &timeStructure);

}


// RTC module timer
void RTC_IRQHandler() {
	if(RTC_SR & RTC_SR_TAF_MASK) {
		soundSignaling(soundSignalingType);
		lightSignaling(lightSignalingType);

		if(numOfReps > 0) {
			numOfReps--;
			RTC_TAR += intervalLength;
		} else {
			RTC_TAR = 0;
		}
	}
}

int setCurrentDatetime() {
	// set date and time
	int conversionFlag;

	sendStdout("Enter current date and time in YYYY-MM-DD HH:MM:SS format.\r\n");
	sendStdout("Date and time: ");
	receiveStdin();

	// receive datetime and convert it into seconds and store it into variable
	conversionFlag = convertTimeToSeconds(string, &secondsClock);
	if(conversionFlag == 0) {
		RTC_SR &= ~RTC_SR_TCE_MASK;
		RTC_TSR = secondsClock;
		RTC_SR |= RTC_SR_TCE_MASK;
		sendStdout("Date and time was successfully set.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 0;
	} else {
		sendStdout("ERROR, please enter valid date and time.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 1;
	}
}

int setSoundSignaling() {
	sendStdout("Choose type of sound signaling, type number between 1 and 3.\r\n");
	sendStdout("You can exmperiment with types of sound signaling to help you decide which one you like the most, type exp[1-3].\r\n");
	sendStdout("Sound signaling type: ");
	receiveStdin();

	if(strcmp(string, "1") == 0) {
		soundSignalingType = 1;
		sendStdout("Sound signaling was successfully set.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 0;
	}
	else if(strcmp(string, "2") == 0) {
		soundSignalingType = 2;
		sendStdout("Sound signaling was successfully set.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 0;
	}
	else if(strcmp(string, "3") == 0) {
		soundSignalingType = 3;
		sendStdout("Sound signaling was successfully set.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 0;
	}
	else if(strcmp(string, "exp1") == 0) {
		soundSignaling(1);
		return 1;
	}
	else if(strcmp(string, "exp2") == 0) {
		soundSignaling(2);
		return 1;
	}
	else if(strcmp(string, "exp3") == 0) {
		soundSignaling(3);
		return 1;
	} else {
		sendStdout("ERROR, please enter valid type of sound signaling.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 1;
	}
}

int setLightSignaling() {
	sendStdout("Choose type of light signaling, type number between 1 and 3.\r\n");
	sendStdout("You can exmperiment with types of light signaling to help you decide which one you like the most, type exp[1-3].\r\n");
	sendStdout("Light signaling type: ");
	receiveStdin();

	if(strcmp(string, "1") == 0) {
		lightSignalingType = 1;
		sendStdout("Light signaling was successfully set.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 0;
	}
	else if(strcmp(string, "2") == 0) {
		lightSignalingType = 2;
		sendStdout("Light signaling was successfully set.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 0;
	}
	else if(strcmp(string, "3") == 0) {
		lightSignalingType = 3;
		sendStdout("Light signaling was successfully set.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 0;
	}
	else if(strcmp(string, "exp1") == 0) {
		lightSignaling(1);
		return 1;
	}
	else if(strcmp(string, "exp2") == 0) {
		lightSignaling(2);
		return 1;
	}
	else if(strcmp(string, "exp3") == 0) {
		lightSignaling(3);
		return 1;
	} else {
		sendStdout("ERROR, please enter valid type of light signaling.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 1;
	}
}

int setAlarmDatetime() {
	int conversionFlag;

	sendStdout("Enter date and time of alarm in YYYY-MM-DD HH:MM:SS format.\r\n");
	sendStdout("Alarm date and time: ");
	receiveStdin();

	conversionFlag = convertTimeToSeconds(string, &secondsAlarm);
	if((RTC_TSR < secondsAlarm) && (conversionFlag == 0)) {
		RTC_TAR = secondsAlarm;
		sendStdout("Date and time of alarm was successfully set.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 0;
	} else {
		sendStdout("ERROR, please enter valid date and time of alarm.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 1;
	}
}

int setNumOfReps() {
	int conversionFlag;

	sendStdout("Enter number of repetitions of alarm, type number between 0 and 5.\r\n");
	sendStdout("You can turn off repeating of alarm by typing 0.\r\n");
	sendStdout("Number of repetitions: ");
	receiveStdin();

	conversionFlag = sscanf(string, "%d", &numOfReps);
	if(conversionFlag != 1) {
		sendStdout("ERROR, please enter valid number of repetitions.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 1;
	} else {
		if(numOfReps > 5 || numOfReps < 0) {
			sendStdout("ERROR, please enter valid number of repetitions.\r\n");
			sendStdout("-----------------------------------------------\r\n");
			return 1;
		} else {
			sendStdout("Number of repetitions of alarm was successfully set.\r\n");
			sendStdout("-----------------------------------------------\r\n");
			return 0;
		}
	}
}

int setIntervalBetweenReps() {
	int conversionFlag;

	sendStdout("Enter length of interval between alarm repetitions, type number between 20 and 480.\r\n");
	sendStdout("Length of interval: ");
	receiveStdin();

	conversionFlag = sscanf(string, "%d", &intervalLength);
	if(conversionFlag != 1) {
		sendStdout("ERROR, please enter valid length of interval between alarm repetitions.\r\n");
		sendStdout("-----------------------------------------------\r\n");
		return 1;
	} else {
		if(intervalLength < 20 || intervalLength > 480) {
			sendStdout("ERROR, please enter valid length of interval between alarm repetitions.\r\n");
			return 1;
		} else {
			sendStdout("Length of interval between alarm repetitions was successfully set.\r\n");
			sendStdout("-----------------------------------------------\r\n");
			return 0;
		}
	}
}

int main() {
	initializePorts();
	initializeMCU();
	initializeUART();
	initializeRTC();

	wait(1000);

	int flag;
	State currState = PROMPT;


	while(1) {
		switch(currState) {
			case PROMPT:

				// prompt the user to type current date and time
				flag = setCurrentDatetime();
				if(flag == 1) {
					continue;
				}

				// prompt the user to choose type of sound signaling
				soundSettings:
				flag = setSoundSignaling();
				if(flag == 1) {
					goto soundSettings;
				}

				// prompt the user to choose type of light signaling
				lightSettings:
				flag = setLightSignaling();
				if(flag == 1) {
					goto lightSettings;
				}

				// prompt the user to type time and date of alarm
				alarmSettings:
				flag = setAlarmDatetime();
				if(flag == 1) {
					goto alarmSettings;
				}

				// prompt the user to set number of repetitions of alarm
				repsSettings:
				flag = setNumOfReps();
				if(flag == 1) {
					goto repsSettings;
				}

				// prompt the user to set length of interval between alarm repetitions
				if(numOfReps > 0) {
					intervalSettings:
					flag = setIntervalBetweenReps();
					if(flag == 1) {
						goto intervalSettings;
					}
				}

				currState = WORKING;
			break;

			case WORKING:
				sendStdout("Current date and time: ");
				secondsConversion = RTC_TSR; // current time in seconds
				convertSecondsToTime(&secondsConversion, string);
				sendStdout(string);
				sendStdout("\r\n");

				sendStdout("Date and time of alarm: ");
				secondsConversion = RTC_TAR;
				if(secondsConversion == 0) {
					sendStdout("alarm is off.\r\n");
				} else {
					convertSecondsToTime(&secondsConversion, string);
					sendStdout(string);
					sendStdout("\r\n");
				}

				sendStdout("\r\n");

				// Offer the user to either turn the alarm off or set a new one
				sendStdout("To turn alarm off, type \"off\"\r\n");
				sendStdout("To set new alarm and stop current one, type \"new\"\r\n");
				sendStdout("Input: ");

				receiveStdin();
				if(strcmp(string, "new") == 0) {
					RTC_TAR = 0;
					currState = PROMPT;
				}
				else if(strcmp(string, "off") == 0) {
					RTC_TAR = 0;
					currState = END;
				}


			break;

			case END:
				sendStdout("Application was turned off.\r\n");
				while(1);
			break;
		}
	}

}
