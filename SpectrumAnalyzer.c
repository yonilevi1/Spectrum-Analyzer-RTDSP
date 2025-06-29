#include <stdbool.h>        // Enables use of boolean types like true and false
#include <stdio.h>          // Standard library for I/O functions like printf
#include <math.h>           // Math library for functions like sqrt(), sin(), cos()
#define FFT_SIZE 256        // Defines number of samples and FFT size

// TFT display data and control pin connections
char TFT_DataPort at LATE;
sbit TFT_RST at LATD7_bit;
sbit TFT_BLED at LATD2_bit;
sbit TFT_RS at LATD9_bit;
sbit TFT_CS at LATD10_bit;
sbit TFT_RD at LATD5_bit;
sbit TFT_WR at LATD4_bit;

char TFT_DataPort_Direction at TRISE;
sbit TFT_RST_Direction at TRISD7_bit;
sbit TFT_BLED_Direction at TRISD2_bit;
sbit TFT_RS_Direction at TRISD9_bit;
sbit TFT_CS_Direction at TRISD10_bit;
sbit TFT_RD_Direction at TRISD5_bit;
sbit TFT_WR_Direction at TRISD4_bit;

float realSignal[FFT_SIZE];            // Real part of the signal (FFT input)
float imagSignal[FFT_SIZE];            // Imaginary part of the signal (usually zero)
float spectrumMag[FFT_SIZE];           // FFT output - magnitude spectrum
int spectrumBar[FFT_SIZE];             // Magnitude converted to integer for display

volatile float currentSample = 0;      // Last sampled value from ADC
volatile int currentADC = 0;           // Last sampled value as integer
volatile bool newSampleFlag = false;   // Indicates if a new sample is available
volatile int drawCursor = 0;           // Current draw position on screen
volatile bool graphFinished = false;   // Indicates if drawing is complete (used in main)
int loopCounter;                       // General loop counter
volatile float adcBuffer[FFT_SIZE] = {0};  // ADC samples buffer
volatile int sampleIdx = 0;                 // Next sample write index
volatile bool isBufferReady = false;       // Indicates if buffer is full
volatile bool isProcessingDone = false;    // Indicates if FFT processing is complete

// Initializes PORTA for LEDs as output and clears it
void InitLEDs() {
    AD1PCFG = 0xFFFF;        // Sets all pins to digital (disables analog)
    JTAGEN_bit = 0;          // Disables JTAG to free pins
    TRISA = 0x0000;          // Sets PORTA as output
    LATA = 0;                // Clears all outputs
}

// Initializes Timer1 to create sampling interrupts at fixed intervals (e.g. 1ms)
void InitTimer1() {
    T1CON = 0x8010;          // Enables timer with 1:8 prescaler
    T1IP0_bit = 1;           // Interrupt priority bits
    T1IP1_bit = 1;
    T1IP2_bit = 1;           // High priority
    T1IF_bit = 0;            // Clears interrupt flag
    T1IE_bit = 1;            // Enables Timer1 interrupt
    PR1 = 20000;             // Compare value determines interrupt frequency
    TMR1 = 0;                // Resets timer counter
}

// Initializes ADC input
void InitADC() {
    AD1PCFG = 0xFFFE;        // RB0 analog input, others digital
    JTAGEN_bit = 0;          // Disables JTAG
    TRISB0_bit = 1;          // Sets RB0 as input
    ADC1_Init();             // Initializes ADC
    Delay_ms(100);           // Waits for stabilization
}

// Initializes TFT display with initial title
void InitTFT() {
    TFT_BLED = 1;                                // Turns on backlight
    TFT_Init_ILI9341_8bit(320, 240);             // Initializes 320x240 TFT in 8-bit mode
    TFT_Fill_Screen(CL_Yellow);                  // Sets background to yellow
    TFT_Set_Pen(CL_BLACK, 20);                   // Sets pen color black, thickness 20
    TFT_Set_Font(TFT_defaultFont, CL_BLACK, FO_HORIZONTAL); // Sets font
    TFT_Write_Text("Yoni and Daniel graph", 100, 100); // Displays initial text
}

// Draws graph axes and labels
void DrawGraphAxes() {
    TFT_Fill_Screen(CL_YELLOW);

    // Y-axis
    TFT_Set_Pen(CL_BLACK, 2);
    TFT_Line(30, 220, 30, 60);
    TFT_Write_Text("Amp", 5, 40);

    // Arrow at top of Y-axis
    TFT_Line(30, 60, 27, 65);
    TFT_Line(30, 60, 33, 65);

    // X-axis
    TFT_Line(20, 210, 315, 210);
    TFT_Write_Text("Freq [Hz]", 260, 220);

    // Arrow at end of X-axis
    TFT_Line(315, 210, 310, 207);
    TFT_Line(315, 210, 310, 213);
}

// Draws a single bar on the spectrum graph
void DrawBar(int amplitude, int xCoord) {
    unsigned long yHeight = (unsigned long)amplitude;

    // Limits maximum height, scales small amplitudes
    if (yHeight > 160) yHeight = 160;
    else if (yHeight < 40) yHeight *= 2;
    else if (yHeight < 20) yHeight *= 4;

    if (xCoord < 32 || xCoord > 308) return;  // Bounds check

    TFT_Set_Pen(CL_BLACK, 2);
    TFT_line(xCoord, 210, xCoord, 210 - yHeight);
}

bool isTemplateDrawn = false;  // Indicates if graph template is drawn

// Draws the full spectrum graph
void DrawSpectrum(int* dataPtr, int dataSize) {
    if (!isTemplateDrawn) {
        DrawGraphAxes();
        isTemplateDrawn = true;
    }

    // Clear previous graph area
    TFT_Set_Pen(CL_YELLOW, 1);
    TFT_Set_Brush(1, CL_YELLOW, 0, LEFT_TO_RIGHT, CL_YELLOW, CL_YELLOW);
    TFT_Rectangle_Round_Edges(33, 25, 308, 208,1);

    for (drawCursor = 1; drawCursor < dataSize && drawCursor < 70; drawCursor++) {
        int x = 32 + drawCursor * 4;
        DrawBar(*(dataPtr + drawCursor), x);
    }

    graphFinished = true;
}

// Calculates magnitude spectrum from FFT result
void CalculateSpectrumMagnitude() {
    for (loopCounter = 0; loopCounter < FFT_SIZE; loopCounter++) {
        spectrumMag[loopCounter] = sqrt(realSignal[loopCounter] * realSignal[loopCounter] +
                                        imagSignal[loopCounter] * imagSignal[loopCounter]) / FFT_SIZE;

        spectrumBar[loopCounter] = (int)spectrumMag[loopCounter];
    }
}
// Reorders array elements based on bit reversal for FFT preparation
void BitReversal(float re[], float im[]) {
    unsigned int target = 0, pos, mask;

    for (pos = 0; pos < FFT_SIZE; pos++) {
        if (target > pos) {
            // Swap real parts
            float tempRe = re[target];
            re[target] = re[pos];
            re[pos] = tempRe;

            // Swap imaginary parts
            float tempIm = im[target];
            im[target] = im[pos];
            im[pos] = tempIm;
        }

        mask = FFT_SIZE;
        while (target & (mask >>= 1)) {
            target &= ~mask;
        }
        target |= mask;
    }
}

// Executes FFT computation
void ExecuteFFT(float re[], float im[]) {
    const float pi = -3.14159265358979323846; // Negative per FFT definition
    unsigned int stageSize, groupIdx, pairIdx;

    for (stageSize = 1; stageSize < FFT_SIZE; stageSize <<= 1) {
        unsigned int groupStep = stageSize << 1;
        float baseAngle, twRe, twIm;

        for (groupIdx = 0; groupIdx < stageSize; groupIdx++) {
            baseAngle = pi * groupIdx / stageSize;
            twRe = cos(baseAngle);
            twIm = sin(baseAngle);

            for (pairIdx = groupIdx; pairIdx < FFT_SIZE; pairIdx += groupStep) {
                unsigned int match = pairIdx + stageSize;

                float tempRe = twRe * re[match] - twIm * im[match];
                float tempIm = twIm * re[match] + twRe * im[match];

                re[match] = re[pairIdx] - tempRe;
                im[match] = im[pairIdx] - tempIm;

                re[pairIdx] += tempRe;
                im[pairIdx] += tempIm;
            }
        }
    }
}

// Runs full FFT process including bit reversal
void RunSpectrumFFT(float re[], float im[]) {
    BitReversal(re, im);
    ExecuteFFT(re, im);
}

// Applies alternating sign to simulate windowing effect
void ApplyAlternatingSign() {
    for (loopCounter = 0; loopCounter < FFT_SIZE; loopCounter += 2) {
        realSignal[loopCounter] = -realSignal[loopCounter];
        imagSignal[loopCounter] = -imagSignal[loopCounter];
    }
}

// Processes incoming signal: copies ADC buffer, runs FFT, calculates magnitude
void ProcessIncomingSignal() {
    for (loopCounter = 0; loopCounter < FFT_SIZE; loopCounter++) {
        realSignal[loopCounter] = adcBuffer[loopCounter];
        imagSignal[loopCounter] = 0;
    }

    ApplyAlternatingSign();
    RunSpectrumFFT(realSignal, imagSignal);
    CalculateSpectrumMagnitude();
    isProcessingDone = true;
}

// Timer1 interrupt: samples ADC at fixed intervals
void Timer1Interrupt() iv IVT_TIMER_1 ilevel 7 ics ICS_SRS {
    T1IF_bit = 0; // Clears interrupt flag

    if (isBufferReady) return; // If buffer is full, skip sampling

    currentADC = ADC1_Get_Sample(0); // Reads sample from ADC channel 0
    currentSample = (float)currentADC; // Converts to float
    newSampleFlag = true; // Sets flag indicating new sample
}

bool processLock = false;     // Prevents multiple processing at once
bool startCycle = false;      // Indicates if acquisition cycle has started

// Starts a new acquisition cycle: resets variables and starts sampling
void StartNewAcquisitionCycle() {
    processLock = false;
    startCycle = false;
    isBufferReady = false;
    currentSample = 0;

    for (sampleIdx = 0; sampleIdx < FFT_SIZE; sampleIdx++) {
        adcBuffer[sampleIdx] = 0;
    }

    sampleIdx = 0;

    LATAbits.LATA1 = 1;  // Turns on LED1 - sampling active
    LATAbits.LATA2 = 0;  // Turns off LED2 - not processing yet
    EnableInterrupts();  // Enables interrupts
}

// Main function
void main() {
    InitTFT();       // Initializes TFT display
    InitADC();       // Initializes ADC
    InitLEDs();      // Initializes LEDs
    InitTimer1();    // Initializes Timer1 for sampling
    EnableInterrupts();

    LATAbits.LATA1 = 1; // LED1 on at start

    while (1) {
        if (currentSample != 0 || startCycle) {
            startCycle = true;

            if (newSampleFlag && sampleIdx < FFT_SIZE) {
                newSampleFlag = false;
                adcBuffer[sampleIdx++] = currentSample;
            }
            else if (!processLock && sampleIdx >= FFT_SIZE) {
                DisableInterrupts();
                processLock = true;

                LATAbits.LATA1 = 0;  // Turns off sampling LED
                LATAbits.LATA2 = 1;  // Turns on processing LED
                isBufferReady = true;

                ProcessIncomingSignal(); // Runs FFT processing

                if (isProcessingDone) {
                    DrawSpectrum(spectrumBar + FFT_SIZE / 2, FFT_SIZE / 2); // Draws half-spectrum
                    isProcessingDone = false;
                }

                if (graphFinished) {
                    TFT_Write_Text("Yoni and Daniel graph", 100, 10);
                    StartNewAcquisitionCycle(); // Starts new cycle
                    Delay_ms(1000);
                    graphFinished = false;
                }
            }
        }
    }
}
