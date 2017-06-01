#include "STC/STC12C5A60S2.H"
#include <stdlib.h> 		//for rand()/srand() function

#define FOSC 22118400L 		//quartz crystal oscillator frequency
#define BAUD 9600			//baurdrate
#define C8 (256-FOSC/24/4186) 
#define Db8 (256-FOSC/24/4435) 
#define D8 (256-FOSC/24/4699) 
#define Eb8 (256-FOSC/24/4978) 
#define E8 (256-FOSC/24/5274) 
#define F8 (256-FOSC/24/5588) 
#define Gb8 (256-FOSC/24/5920) 
#define G8 (256-FOSC/24/6272) 
#define Ab8 (256-FOSC/24/6644) 
#define A8 (256-FOSC/24/7040) 	//7.04KHz frequency calculation method of 12T mode
#define Bb8 (256-FOSC/24/7459)
#define B8 (256-FOSC/24/7902) 	//7902Hz
#define C9 (256-FOSC/24/8372) 	//8372Hz 
#define Db9 (256-FOSC/24/8870)
#define D9 (256-FOSC/24/9397) 	//9397Hz
#define Eb9 (256-FOSC/24/9956)
#define E9 (256-FOSC/24/10548)	//10540Hz

bit set_settings=0;
bit correctflag=1;
bit busy;					//bit used in uart interrupt
bit target=0;				//target bit used to tell if song is target=1/nontarget=0
bit receive_settings=0;
bit pause=1;
bit leftdripflag=0;
bit rightdripflag=0;
bit stoptimer=0;

sbit BRTCLKO=P2^2;			//pin 1
sbit leftlight=P2^0;			//pin 2
sbit rightlight=P2^1;			//pin 3
sbit rightvalve=P3^6;			
sbit leftvalve=P3^4;

unsigned char no_lick_help_likelyhood=2;
unsigned char chance_mouse_drop=0;
unsigned char lickdoesntcount=0;
unsigned char parameters[26]={0x48,0x32,0x55,0x22,0x00,0x01,0x2C,0x01,0x2C,0x01,0xF4,0x00,0xC8,0x1E,0x14,0x23,0x23,0x00,0x0A,0x03,0x3D};
unsigned char tone_index[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
unsigned char parameter_index=0;		
unsigned char no_lick_help=8;		
unsigned char info; 
unsigned char tone_duration=30;         
unsigned char time_between_tones=20;	
unsigned char left_valve_open_time=35; //units of 10ms
unsigned char right_valve_open_time=35;	
unsigned char max_difference=100;
unsigned char min_difference=10;
unsigned char drip_delay_time=0;		//only applies to trainingphase 1
unsigned char training_phase=3; 
unsigned char song[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
unsigned char nolickcount=0;
unsigned char k=0;				//index used for tones			    
unsigned char correct=0;			//correct=2 => mouse licked correctly;    correct=1 => incorrect;    correct=0 => no lick
unsigned char phase=0;    	//phase=0 song; phase=1 lickwindow; phase=2 reward; phase=3 delay
unsigned char driptimecounter=0;
unsigned char mouselicked=0;
unsigned char songdifficulty=0;
unsigned char targetsong[6]={0,16,7,12,0xFF,0xFF};	
unsigned char NUM_TONES_PER_SONG=0;
unsigned char xdata tones[17]={C8,Db8,D8,Eb8,E8,F8,Gb8,G8,Ab8,A8,Bb8,B8,C9,Db9,D9,Eb9,E9};

unsigned int j=0; 			//j is the index used to indicate which song is being played 		
unsigned int trial_number=600;       	//sets number of trials
signed int phasecounter=0;	//+1 everytime timer0 overflows, resets when phase changes
unsigned int checksum;
unsigned int MCUchecksum;
unsigned int punishment_duration=00; 
unsigned int delay_duration=300; 		
unsigned int lickwindow_duration=200;   

void RegularTransmission(unsigned char h){
	while(busy); busy=1; SBUF=h;}

void ScheduledTransmission1(){
	EX0=0; EX1=0; RegularTransmission(0x71);   
	RegularTransmission((j>>8)&0xff); RegularTransmission(j); 
  EX0=1; EX1=1;}

void ScheduledTransmission2(){
	EX0=0; EX1=0; RegularTransmission(0x72);   
	RegularTransmission(tone_index[0]); RegularTransmission(tone_index[1]); RegularTransmission(tone_index[2]);   
	RegularTransmission(tone_index[3]); RegularTransmission(tone_index[4]); RegularTransmission(tone_index[5]);
	EX0=1; EX1=1;}

void ScheduledTransmission3(){
	EX0=0; EX1=0; RegularTransmission(0x74); RegularTransmission((correct<<4)|mouselicked);
	RegularTransmission(songdifficulty); RegularTransmission(lickdoesntcount); EX0=1; EX1=1;}

void waitfordata(){
	unsigned char n=0; EX1=0; EX0=0; stoptimer=1;
	while (pause){
		if (set_settings){MCUchecksum=0;
			checksum=(((unsigned int)parameters[24])<<8)+parameters[25];
			for (n=0;n<=23;n++){MCUchecksum=MCUchecksum+parameters[n];}
				if(MCUchecksum==checksum){RegularTransmission(0x77);
					training_phase=parameters[0];   	//set time you want the punishment to be in seconds in the parenthesises
					min_difference=parameters[1];
					max_difference=parameters[2];
					targetsong[0]=parameters[3]; targetsong[1]=parameters[4]; 
					targetsong[2]=parameters[5]; targetsong[3]=parameters[6]; 
					targetsong[4]=parameters[7]; targetsong[5]=parameters[8]; 		
					trial_number=(((unsigned int)parameters[9])<<8)+parameters[10];
					delay_duration=(((unsigned int)parameters[11])<<8)+parameters[12];
					punishment_duration=(((unsigned int)parameters[13])<<8)+parameters[14];
					lickwindow_duration=(((unsigned int)parameters[15])<<8)+parameters[16];
					tone_duration=parameters[17];         	//set duration of tones in seconds in the parenthesises
					time_between_tones=parameters[18];		//set duration of time between tones in the parenthesises
					right_valve_open_time=parameters[19];		//set duration of open reward valve in seconds			
					left_valve_open_time=parameters[20];
					drip_delay_time=parameters[21];		//only applies to trainingphase 1
					no_lick_help=parameters[22];
					no_lick_help_likelyhood=parameters[23];}
			else {RegularTransmission(0x75);}
			set_settings=0;}} EX1=1; EX0=1; stoptimer=0;}

void timer0(void) interrupt 1 {
	TL0=0x2B; TH0=0xB1;//initial values loaded to timer 100HZ (.01s period) 65355-(24,000,000/12/100)=45355 45355(dec)->B12B(hex) TH0=B1 TL0=2B			
	if (!stoptimer) {phasecounter++;
		if (leftdripflag==1){driptimecounter++;
			if (driptimecounter==drip_delay_time+1){leftvalve=1; leftlight=0;}
			if (driptimecounter==drip_delay_time+left_valve_open_time+1){
				leftdripflag=0; driptimecounter=0; leftlight=1; leftvalve=0;}}
		if (rightdripflag==1){driptimecounter++;
			if (driptimecounter==drip_delay_time+1){rightvalve=1; rightlight=0;}
			if (driptimecounter==drip_delay_time+right_valve_open_time+1){
				rightdripflag=0; driptimecounter=0; rightlight=1; rightvalve=0;}}
		if (phase==1 & phasecounter==lickwindow_duration){phase=2; nolickcount++; 
			correct=0; mouselicked=0; ScheduledTransmission3(); phasecounter=0;}
		if (phase==2 & ((correct==2 & phasecounter==delay_duration) | phasecounter==punishment_duration+delay_duration-1)){ //phase 2
			phase=0; ScheduledTransmission1(); k=0; BRT=song[k]; phasecounter=0;}		 //phasecounter resets every phase change}
		if (phase==0){
			if (phasecounter == k*(tone_duration+time_between_tones)){WAKE_CLKO=0x04;}
			else if (k < NUM_TONES_PER_SONG & (phasecounter == (k+1)*tone_duration+k*time_between_tones)){WAKE_CLKO=0; k++; BRT=song[k];}
			if (k == NUM_TONES_PER_SONG){phase=1; ScheduledTransmission2(); phasecounter=0; 
				if (training_phase==0){nolickcount=0; lickdoesntcount=1;
					if(target){rightdripflag=1;} else{leftdripflag=1;}}
				else if (nolickcount>=no_lick_help){chance_mouse_drop=rand()%(no_lick_help_likelyhood-1);
					if(chance_mouse_drop==0){nolickcount=0; lickdoesntcount=1; 
						if(target){rightdripflag=1;} else{leftdripflag=1;}}}}}}}
									
void exint0() interrupt 0 {	   //left lick interrupt (location at 0003H)
	RegularTransmission(0xFD); 
	if (phase==1){mouselicked=0x02; phase=2; phasecounter=0; nolickcount=0;
		if (!target){correct=0x01; correctflag=1; if (!lickdoesntcount){leftdripflag=1;}}
		else {correct=0x02;} ScheduledTransmission3();} lickdoesntcount=0;}

void exint1() interrupt 2 {	   //right lick interrupt (location at 0013H)
	RegularTransmission(0xFE);   	
	if (phase==1){mouselicked=0x01; phase=2; phasecounter=0; nolickcount=0;
		if (target){correct=0x01; correctflag=1; if (!lickdoesntcount){rightdripflag=1;}}//if mouse was supposed to lick right lead, it is incorrect, so correct=2
		else {correct=0x02;} ScheduledTransmission3();} lickdoesntcount=0;}
		
void Uart_Isr() interrupt 4 {
	if (RI){RI=0; info=SBUF;
		if (receive_settings){parameters[parameter_index]=info; parameter_index++;
			//27 might be 1 too high, but I don't think so.
			
			if (parameter_index==27){set_settings=1; receive_settings=0; parameter_index=0;}}
		else {
			if (pause & info==0x88) { 	//flush ON
				rightvalve=1; leftvalve=1; rightlight=0; leftlight=0;}
			else if (pause & info==0x89) { 	//flush OFF
				rightvalve=0; leftvalve=0; rightlight=1; leftlight=1;}
			if (info==0x55){pause=1;}
			else if (info==0x56 & rightvalve==0){pause=0;}
			else if (info==0x11 & pause==1){receive_settings=1;}}}
	if (TI){TI=0; busy=0;}}    	//Clear transmit interrupt flag; Clear transmit busy flag 

void main(){
	unsigned char tonedifficulty=0;
	unsigned char i=0;
	EX0=0; EX1=0;P3=0xAF; IP=0x10; IPH=0x12; 
	P3M0=0x50; P3M1=0x00; rightvalve=0; leftvalve=0; //configures port 3, for valves
	AUXR=0x10; SCON=0x5a; TMOD=0x21;       //8 bit data, no parity bit.. 8-bit auto reload timer 1 and 16 bit timer 0
	TH1=TL1=0xFD;           //Set Uart baudrate	(19200)
	TL0=0x2B;  TH0=0xB1;    //initial values loaded to timer 100HZ (.01s period); 65355-(24,000,000/12/100)=45355 45355(dec)->B12B(hex) TH0=B1 TL0=2B
	TCON=0x55; TR1=1; TR0=1;			//start timers 0 and 1	 
	IE=0x82; REN=1; ES=1; //enable all interrupts																			   
	waitfordata(); 
	srand(TL0);  EX0=1; EX1=1; 
	for (i=0;i<=5;i++){if (targetsong[i]!=0xFF){NUM_TONES_PER_SONG++;}}
	phasecounter=-1;
	while(1){	
		if ((training_phase==0 | (training_phase==1 & correctflag)) & j%3==0){correctflag=0; target=!target;}
		else if ((training_phase==2 & correct==1) | training_phase==3){target=rand()%2;} //song composed in phase 3		
		if (target){songdifficulty=0;
			for(i=0;i<=NUM_TONES_PER_SONG-1;i++){tone_index[i]=targetsong[i]; song[i]=tones[tone_index[i]];}} 	
		else{
			do{songdifficulty=0;
				for (i=0;i<=NUM_TONES_PER_SONG-1;i++){
					tone_index[i]=rand()%16; song[i]=tones[tone_index[i]]; tonedifficulty=tone_index[i];
					songdifficulty=songdifficulty+abs(tonedifficulty-targetsong[i]);}}
			while(songdifficulty>=max_difference | songdifficulty<=min_difference);}
		j++; 
		//if(j>=trial_number){j=0;}	
		waitfordata();	
		while(phase==2){}				//catches code until delay ends																			
		while(phase==0){}			
		while(phase==1){}}}
