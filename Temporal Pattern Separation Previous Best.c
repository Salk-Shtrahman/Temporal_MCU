/*
This is the previous best version of code. We are changing it because we want to
expand range of tones in library, expand number of tones per song, and a "chord"
function to the behavior task
*/
#include "STC/STC12C5A60S2.H"
#include <stdlib.h> 		//for rand()/srand() function

#define FOSC 24000000L 		//quartz crystal oscillator frequency
#define BAUD 9600			//baurdrate
#define a (256-FOSC/24/7040) 	//7.04KHz frequency calculation method of 1T mode
#define b (256-FOSC/24/7902) 	//7902Hz
#define c (256-FOSC/24/8372) 	//8372Hz 
#define d (256-FOSC/24/9397) 	//9397Hz
#define e (256-FOSC/24/10540)	//10540Hz
#define f (256-FOSC/24/11175)	//11175Hz
#define g (256-FOSC/24/12544)	//12544Hz hello

bit set_settings=0;
bit correctflag=1;
bit busy;					//bit used in uart interrupt
bit target=0;				//target bit used to tell if song is target=1/nontarget=0
bit receive_settings=0;
bit pause=1;
bit leftdripflag=0;
bit rightdripflag=0;
bit lickdoesntcount=0;
bit stoptimer=0;

sbit BRTCLKO=P2^2;			//pin 1
sbit leftlight=P2^0;			//pin 2
sbit rightlight=P2^1;			//pin 3
sbit rightvalve=P3^6;			
sbit leftvalve=P3^4;

//training_phase=(parameters[0]&0xe0)>>5;
//min_distance=parameters[0]&0x1F;
//max_distance=parameters[1];
//targetsong[0]=parameters[2]&0xf0; targetsong[1]=parameters[2]&0x0f; 
//targetsong[2]=parameters[3]&0xf0; targetsong[3]=parameters[3]&0x0f; 
//targetsong[4]=parameters[4]&0xf0; targetsong[5]=parameters[4]&0x0f; 		
//trial_number=(((unsigned int)parameters[5])<<8)+parameters[6];
//delay_duration=(((unsigned int)parameters[7])<<8)+parameters[8];
//punishment_duration=(((unsigned int)parameters[9])<<8)+parameters[10];
//lickwindow_duration=(((unsigned int)parameters[11])<<8)+parameters[12];
//tone_duration=parameters[13];         	//time duration of tones in units of 10ms
//time_between_tones=parameters[14];		  //time duration of time between tones in units of 10ms
//right_valve_open_time=parameters[15];		//time duration of reward from right valve in units of 10ms		
//left_valve_open_time=parameters[16];		//time duration of reward from left valve in units of 10ms
//drip_delay_time=parameters[17];					//time duration between end of song and automatic reward in units of 10ms
//no_lick_help=parameters[18];						//number of no licks before the mouse is given an encouragement drop
//checksum=(((unsigned int)parameters[19])<<8)+parameters[20];

unsigned char parameters[21]={0x48,0x32,0x55,0x22,0x00,0x01,0x2C,0x01,0x2C,0x01,0xF4,0x00,0xC8,0x1E,0x14,0x23,0x23,0x00,0x0A,0x03,0x3D};
unsigned char parameter_index=0;		
unsigned char no_lick_help=10;		
unsigned char info; 
unsigned char tone_duration=30;         
unsigned char time_between_tones=20;	
unsigned char left_valve_open_time=35; //units of 10ms
unsigned char right_valve_open_time=35;	
unsigned char max_distance=100;
unsigned char min_distance=10;
unsigned char drip_delay_time=20;		//only applies to trainingphase 1
unsigned char training_phase=2; 
unsigned char song[6]={0,0,0,0,0,0};
unsigned char nolickcount=0;
unsigned char k=0;				//index used for tones			    
unsigned char correct=0;			//correct=2 => mouse licked correctly;    correct=1 => incorrect;    correct=0 => no lick
unsigned char phase=0;    	//phase=0 song; phase=1 lickwindow; phase=2 reward; phase=3 delay
unsigned char driptimecounter=0;
unsigned char mouselicked=0;
unsigned char songdifficulty=0;
unsigned char targetsong[6]={0,2,4,6,0,0};		//8th octave: {0,1,2,3,4,5,6} --> {a,b,c,d,e,f,g}

unsigned int trial_number=600;       	//sets number of trials
int phasecounter=0;	//+1 everytime timer0 overflows, resets when phase changes
unsigned int checksum;
unsigned int MCUchecksum;
unsigned int punishment_duration=00; 
unsigned int delay_duration=300; 		
unsigned int lickwindow_duration=200;   

void RegularTransmission(unsigned char h){
	while(busy); busy=1; SBUF=h;}

void ScheduledTransmission(unsigned char x,unsigned char y,unsigned char z){
	EX0=0; EX1=0;
	RegularTransmission(x);   
	RegularTransmission(y);
	RegularTransmission(z);
	EX0=1; EX1=1;}

char Tone2Freq(char tone_goes_here){
	unsigned char freq;
	switch(tone_goes_here){	//and sets freq to the value such that it produces the appropriate frequency
		case 0: freq=a; break; case 1: freq=b; break; case 2: freq=c; break;
		case 3: freq=d; break; case 4: freq=e; break; case 5: freq=f; break;
		case 6: freq=g; break;}
	return freq;}

void waitfordata(){
	unsigned char n=0; EX1=0; EX0=0; stoptimer=1;
	while (pause){
		if (set_settings){
			training_phase=(parameters[0]&0xe0)>>5;   	//set time you want the punishment to be in seconds in the parenthesises
			min_distance=parameters[0]&0x1F;
			max_distance=parameters[1];
			targetsong[0]=(parameters[2]&0xf0)>>4; targetsong[1]=(parameters[2]&0x0f); 
			targetsong[2]=(parameters[3]&0xf0)>>4; targetsong[3]=(parameters[3]&0x0f); 
			targetsong[4]=(parameters[4]&0xf0)>>4; targetsong[5]=parameters[4]&0x0f; 		
			trial_number=(((unsigned int)parameters[5])<<8)+parameters[6];
			delay_duration=(((unsigned int)parameters[7])<<8)+parameters[8];
			punishment_duration=(((unsigned int)parameters[9])<<8)+parameters[10];
			lickwindow_duration=(((unsigned int)parameters[11])<<8)+parameters[12];
			tone_duration=parameters[13];         	//set duration of tones in seconds in the parenthesises
			time_between_tones=parameters[14];		//set duration of time between tones in the parenthesises
			right_valve_open_time=parameters[15];		//set duration of open reward valve in seconds			
			left_valve_open_time=parameters[16];
			drip_delay_time=parameters[17];		//only applies to trainingphase 1
			no_lick_help=parameters[18];
			checksum=(((unsigned int)parameters[19])<<8)+parameters[20];		//set duration of open reward valve in seconds
			set_settings=0; MCUchecksum=0;
			for (n=0;n<=18;n++){MCUchecksum=MCUchecksum+parameters[n];}
			if(MCUchecksum==checksum){RegularTransmission(0x77);}
			else {RegularTransmission(0x75);}}} EX1=1; EX0=1; stoptimer=0;}

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
		if (phase==1){
			if(phasecounter==lickwindow_duration){phasecounter=0; phase=2; nolickcount++; correct=0; mouselicked=0;
				ScheduledTransmission(0x74,(correct<<4)|mouselicked,songdifficulty);}}
		if (phase==2){			  //delay phase
			if (correct==2 & phasecounter==delay_duration){phase=0; phasecounter=-1;}
			else if (phasecounter==punishment_duration+delay_duration){phase=0; phasecounter=-1;}}		 //phasecounter resets every phase change}
		if (phase==0){
			if (phasecounter==1){BRT=Tone2Freq(song[k]); WAKE_CLKO=0x04; ScheduledTransmission(0x71,(j>>8)&0xff,j);}
			else if (phasecounter == tone_duration){WAKE_CLKO=0; k++; BRT=Tone2Freq(song[k]);}  
			else if (phasecounter == k*(tone_duration+time_between_tones)){WAKE_CLKO=0x04;}
			else if (k !=3 & phasecounter == (k+1)*tone_duration+k*time_between_tones){WAKE_CLKO=0; k++; BRT=Tone2Freq(song[k]);}
			else if (phasecounter==4*tone_duration+3*time_between_tones){WAKE_CLKO=0; phase=1; phasecounter=0; k=0;
				ScheduledTransmission(0x72,(song[0]<<4)|song[1],(song[2]<<4)|song[3]);  
				if (training_phase==1 | (nolickcount==no_lick_help)){nolickcount=0; lickdoesntcount=1;
					if(target){rightdripflag=1;}else{leftdripflag=1;}}}}}}

void exint0() interrupt 0 {	   //left lick interrupt (location at 0003H)
	RegularTransmission(0xFE); 
	if (phase==1){mouselicked=0x02; phase=2; phasecounter=0; nolickcount=0;
		if (!target){correct=0x01; correctflag=1; if (!lickdoesntcount){leftdripflag=1;}}
		else {correct=0x02;} ScheduledTransmission(0x74,(correct<<4)|mouselicked,songdifficulty);} lickdoesntcount=0;}

void exint1() interrupt 2 {	   //right lick interrupt (location at 0013H)
	RegularTransmission(0xFF);   	
	if (phase==1){mouselicked=0x01; phase=2; phasecounter=0; nolickcount=0;
		if (target){correct=0x01; correctflag=1; if (!lickdoesntcount){rightdripflag=1;}}//if mouse was supposed to lick right lead, it is incorrect, so correct=2
		else {correct=0x02;} ScheduledTransmission(0x74,(correct<<4)|mouselicked,songdifficulty);} lickdoesntcount=0;}
		
void Uart_Isr() interrupt 4 {
	if (RI){RI=0; info=SBUF;
		if (receive_settings){parameters[parameter_index]=info; parameter_index++;
			if (parameter_index==20){set_settings=1; receive_settings=0; parameter_index=0;}}
		else {
			if (info==0xA1) {rightdripflag=1;} 	//right valve drip
			else if (info==0x85) {leftdripflag=1;}	//left valve drip
			else if (pause & info==0x88) { 	//flushout
				rightvalve=1; leftvalve=1; rightlight=0; leftlight=0;}
			else if (pause & info==0x89) { 	//flushout
				rightvalve=0; leftvalve=0; rightlight=1; leftlight=1;}
			else if (info==0x55){pause=1;}
			else if (info==0x56 & rightvalve==0){pause=0;}
			else if (info==0x11 & pause==1){receive_settings=1;}}}
	if (TI){TI=0; busy=0;}}    	//Clear transmit interrupt flag; Clear transmit busy flag 

char ToneDifficulty(char tone){	
	unsigned char tonedifficulty=0;
	switch(tone){
		case 0: tonedifficulty=0; break; case 1: tonedifficulty=2; break; case 2: tonedifficulty=3; break; 	
		case 3: tonedifficulty=5; break; case 4: tonedifficulty=7; break; case 5: tonedifficulty=8; break;
	 	case 6: tonedifficulty=10; break;}
	return tonedifficulty;}

void main(){
	unsigned char tones[7]={0,1,2,3,4,5,6}; //array used to randomly generate songs,
	unsigned char targetdifficulty[6]={0,0,0,0,0,0};
	unsigned char tonedifficulty=0;
	unsigned char i=0;
	unsigned int j=0; 			//j is the index used to indicate which song is being played 		
	EX0=0; EX1=0;P3=0xAF; IP=0x10; IPH=0x12; 
	AUXR=0x10; SCON=0x5a; TMOD=0x21;       //8 bit data, no parity bit.. 8-bit auto reload timer 1 and 16 bit timer 0
	TH1=TL1=0xFD;           //Set Uart baudrate	(21000?)
	TL0=0x2B;  TH0=0xB1;    //initial values loaded to timer 100HZ (.01s period); 65355-(24,000,000/12/100)=45355 45355(dec)->B12B(hex) TH0=B1 TL0=2B
	TCON=0x55; TR1=1; TR0=1;			//start timers 0 and 1	 
	IE=0x82; REN=1; ES=1; //enable all interrupts																			   
	P3M0=0x50; P3M1=0x00;  //configures port 3, for valves 
	waitfordata(); srand(TL0);  EX0=1; EX1=1; 
	for (i=0;i<=3;i++){targetdifficulty[i]=ToneDifficulty(targetsong[i]);}
	while(1){	
		if ((training_phase==1 | (training_phase==2 & correctflag)) & j%3==0){correctflag=0; target=!target;}
		else if ((training_phase==3 & correct==1) | training_phase==4){target=rand()%2;} songdifficulty=0;//song composed in phase 3		
		while(phase==2){}				//catches code until delay ends				
		if (target){song[0]=targetsong[0]; song[1]=targetsong[1]; song[2]=targetsong[2]; song[3]=targetsong[3];} 	
		else{while(songdifficulty>max_distance | songdifficulty<min_distance){songdifficulty=0;
			for (i=0;i<=3;i++){
				song[i]=tones[rand()%7]; tonedifficulty=ToneDifficulty(song[i]);	//if target==1, song = cdef. if not, random song
				songdifficulty=songdifficulty+abs(tonedifficulty-targetdifficulty[i]);}}}
		waitfordata();
		j++; 
		//if(j>=trial_number){j=0;}																	
		while(phase==0){}			
		while(phase==1){}}}