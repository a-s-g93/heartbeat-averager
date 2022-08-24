/* ECG Simulator from SD Card - Receiver
 * 
 * This program uses a Nucleo F303K8 to receive a short from another
 * F303K8 and convert it to a float value from 0 - 1 for output from an analog
 * output pin. The short sent from the other Nucleo (referred to as Sender) is 
 * divided by 2048 because it was obtained via an 11-bit ADC.
 * 
 * Modified heavily from: https://forums.mbed.com/t/two-nucleo-serial-communication-via-tx-and-rx-and-vice-versa/8131
 * 
 * Authors:    Amit J. Nimunkar and Lucas N. Ratajczyk
 * Date:       05-24-2021
 * 
 * Modified by Royal Oakes, 02-02-2022.
 */

#include "mbed.h"

//Serial      pc(USBTX,USBRX);    // Optionally - Set up serial communication with the host PC for printing statement to console
Serial      sender(D1,D0);      // Set up serial communication with the Nucleo which is sending the signal to us (Sender)
AnalogOut   Aout(A3);           // Initialize an analog output pin to display the signal extracted from Sender

// This union is used to recieve data from the sender. Use data.s to access the
// data as a short, and use data.h to access the individual bytes of data.
typedef union _data {
    short s;
    char h[sizeof(short)];
} myShort;

myShort data;

char d;         // Variable to hold the current byte extracted from Sender
int num;        // Integer version of the myShort value
int i = 0;      // Index counter for number of bytes received
float samp_rate = 200.0f;           // Sample rate of the ISR

// Ticker for the ISR
Ticker sampTick; 

// Prototypes
void ISRfxn();

AnalogOut Aout2(A5); 
AnalogOut Aout3(A4);

DigitalOut myled(LED1);


float FiltCoeff_LPF_a[13] = {1, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, 1};
float FiltCoeff_LPF_b[3] = {1, -2, 1};
float LPF_Xbuf[13];
float LPF_Ybuf[3];

float Gain_LPF = 1.0/36; 

int size_filt_LPF_a = sizeof FiltCoeff_LPF_a / sizeof *FiltCoeff_LPF_a;  
int size_filt_LPF_b = sizeof FiltCoeff_LPF_b / sizeof *FiltCoeff_LPF_b;

float FiltCoeff_HPF_a[33]= {[0] = -0.03125, [16] = 1, [17] = -1, [32] = 0.03125};
float FiltCoeff_HPF_b[2] = {1, -1};
float HPF_Xbuf[33];
float HPF_Ybuf[2];

float Gain_HPF = 1.0/1.2; 

int size_filt_HPF_a = sizeof FiltCoeff_HPF_a / sizeof *FiltCoeff_HPF_a;  
int size_filt_HPF_b = sizeof FiltCoeff_HPF_b / sizeof *FiltCoeff_HPF_b;



//deriv declarations
float FiltCoeff_deriv[5] = {2, 1, 0, -1, -2};
float deriv_Xbuf[5];
float Gain_deriv = 1.0/4.0;
int size_filt_deriv = sizeof FiltCoeff_deriv / sizeof *FiltCoeff_deriv;  



//squaring declarations
float sq;


//MWI
float MWI_Ybuf[32];
float Gain_MWI = 1.0 / 32.0;
int MWI_Size = 32;


//threshold
float peaki;
float peakt = 0;
float npki;
float spki;
float thresholdi1;
float x5[3];

int flag = 0;

//Group delays
int GD_LPF = 5;
int GD_HPF = 16;
int GD_deriv = 2;
int GD_MWI = 32;

int k = GD_LPF + GD_HPF + GD_deriv + GD_MWI; // = -55T delay


//SA declarations
float Tempbuf[200] = {0}; // 200-k = 125
float SAbuf[200] = {0};
float GDbuf[55] = {0};
//float Drybuf[235];

//int counter_dry = 0;
int counter_SA = 144;
int c_epochs = 0;
int max_epochs = 32;

//float new_SA = 0;
//float old_SA = 0;

int counter_GD = 0;
int counter_out = 199;

int main() {
    // Set up serial communication
    sender.baud(115200);
    //pc.baud(115200); // Optional debugging.
    
    // Sample num at a fixed rate
    sampTick.attach(&ISRfxn, 1.0f/samp_rate);
    
    // Get data from sender
    while (1) {
        
        // Get the current character from Sender
        d = sender.getc();
        
        // If the byte we got was a '\0', it is possibly the terminator
        if (d == '\0' && i >= sizeof(short)){
            i = 0;                          // Reset index counter.
            num = (int) data.s;             // Convert the short to an int.
        } else if (i < sizeof(short)) {     // If 2 bytes haven't been received, 
            data.h[i++] = d;                // then the byte is added to data
        }
    }
}


/* Interrupt function. Computes the ADC value of num and outputs the voltage.
 */
void ISRfxn() {
    // Convert the number we extracted from Sender into a float with scale 0 - 1 (note
    // division by 2048 due to acquisition of data by an 11-bit ADC) and output it from A3
    float fnum = (float) num/2048.0f;
    Aout = fnum;
    
    
    
    
      LPF_Xbuf[0] = fnum-0.5f;
       
      float Output_temp_LPF_a = 0.0; 
      float Output_temp_LPF_b = 0.0; 

      float Output_LPF; 
      
      // numerator
      for ( int i = 0; i < size_filt_LPF_a; i++) {
          Output_temp_LPF_a = (1.0f/20.0f) * LPF_Xbuf[i] * FiltCoeff_LPF_a[i];
          Output_LPF = Output_LPF + Output_temp_LPF_a; 
      }
      
      // denominator 
      for (int i = 1; i < size_filt_LPF_b; i++) {
          Output_temp_LPF_b = -1.0f * LPF_Ybuf[i] * FiltCoeff_LPF_b[i];
          Output_LPF = Output_LPF + Output_temp_LPF_b;
      }
      

      LPF_Ybuf[0] = Output_LPF;
     
      // shift X and Y buffers
      for (int i = size_filt_LPF_a-1; i > 0; i--) {
          LPF_Xbuf[i] = LPF_Xbuf[i-1];
      }
      for (int i = size_filt_LPF_b-1; i > 0; i--) {
          LPF_Ybuf[i] = LPF_Ybuf[i-1];
      }
      
      //Aout2 = Output_LPF+0.5f;
      //pc.printf("%f\n", LPF_Ybuf[0]);
    
    
//HPF   
    float Output_temp_HPF_a = 0.0; 
    float Output_temp_HPF_b = 0.0; 

    float Output_HPF; 
    
      HPF_Xbuf[0] = LPF_Ybuf[0]; 
      
      // numerator
      for ( int i = 0; i < size_filt_HPF_a; i++) {
          Output_temp_HPF_a = Gain_HPF * HPF_Xbuf[i] * FiltCoeff_HPF_a[i];
          Output_HPF = Output_HPF + Output_temp_HPF_a; 
      }
      
      // denominator 
      for (int i = 1; i < size_filt_HPF_b; i++) {
          Output_temp_HPF_b = -1.0f * HPF_Ybuf[i] * FiltCoeff_HPF_b[i];
          Output_HPF = Output_HPF + Output_temp_HPF_b;
      }
      
      //Aout_BPF = Output_HPF + 0.5f; 
      HPF_Ybuf[0] = Output_HPF;
     
      // shift X and Y buffers
      for (int i = size_filt_HPF_a-1; i > 0; i--) {
          HPF_Xbuf[i] = HPF_Xbuf[i-1];
      }
      for (int i = size_filt_HPF_b-1; i > 0; i--) {
          HPF_Ybuf[i] = HPF_Ybuf[i-1];
      }  
    
    
      //Aout2 = Output_HPF + 0.5f;
    
//deriv portion

      float Output_temp_deriv = 0.0; 
      float Output_deriv; 
      
      deriv_Xbuf[0] = HPF_Ybuf[0]; 
      
      for (int i = 0; i < size_filt_deriv; i++) {
          Output_temp_deriv = Gain_deriv * deriv_Xbuf[i] * FiltCoeff_deriv[i];
          Output_deriv = Output_deriv + Output_temp_deriv; 
      }

     
      for (int i = size_filt_deriv-1; i > 0; i--) {
          deriv_Xbuf[i] = deriv_Xbuf[i-1];
      }
      
//square portion

     sq = Output_deriv*Output_deriv*5;   
    
     //Aout2 = Output_deriv*Output_deriv*5;


//MWI portion 
 
    float Output_temp_MWI;
    float Output_MWI;
   
     MWI_Ybuf[0] = Output_deriv*Output_deriv;
     
     for (int i = 0; i < MWI_Size; i++) {
         Output_temp_MWI = MWI_Ybuf[i];
         Output_MWI += Output_temp_MWI;
         }
    
     for (int i = MWI_Size-1; i > 0; i--) {
         MWI_Ybuf[i] = MWI_Ybuf[i-1];
         }     
     
     
     Aout2 = Output_MWI;
     
     
     
//peak detection
     x5[0] = Output_MWI;
     
     if (x5[0] > x5[2] && x5[0] > peakt) {peakt = x5[0];}
     if (peakt > thresholdi1 && flag != 1) {
         myled = 1;
         flag = 1;
         counter_out=199;
     }
     
     if (x5[0] <= x5[2] && x5[0] < 0.5f*peakt) {
         peaki = peakt;
         myled = 0;  
         //flag = 0;
         
         if (peaki > thresholdi1) {spki = (1.0f/8.0f)*peaki + (7.0f/8.0f)*spki;}  

         else {npki = (1.0f/8.0f)*peaki + (7.0f/8.0f)*npki;}
         
         thresholdi1 = npki + (1.0f/4.0f)*(spki - npki);
         
         peakt = 0;
         
         }
         
     //Aout2 = thresholdi1;
         
     for (int i = 2; i > 0; i--) {x5[i] = x5[i-1];}

     
//signal averager

    GDbuf[0] = fnum; //new at [0]
    
    if (flag == 1) {
        Aout3 = SAbuf[counter_out] / max_epochs;
        
        
       if (counter_out==0) {counter_out=200;}
        
       if (counter_SA >= 144){
            for (int i=200-1; i>144; i--) {Tempbuf[i] += GDbuf[i-145];} //load group delay samples into temp
            }
            
       Tempbuf[counter_SA] += fnum; 
        
        
        counter_SA--;
        counter_out--;
        
        if (counter_SA <= 0) {
            flag = 0; //reset flag
            counter_SA = 144; //reset SA counter
            c_epochs++; //increment epoch counter
            
            if (c_epochs >= max_epochs) { //at max epochs...
                
                //update SA buffer
                for (int i=200-1; i>0; i--) {SAbuf[i] = Tempbuf[i];} //load temp into SA
                
                //reset temp buffer
                for (int i=0; i<200; i++) {Tempbuf[i] = 0;}
                
                //update c_epochs
                c_epochs = 0;
                }
            }
        }  
    
      for (int i=55-1; i>0; i--) {GDbuf[i] = GDbuf[i-1];} //newest --> oldest  
 
}
//END