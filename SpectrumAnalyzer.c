#include <stdbool.h>        // ����� ����� �-type ������� ��� true �-false
#include <stdio.h>          // ������ �������� ��������� ���/��� ��� printf
#include <math.h>           // ������ ������ - ���� ������ ��� sqrt(), sin(), cos()
#define FFT_SIZE 256        // ����� �� ���� ������� ������ �� �-FFT

// ������ ������ ����� ����
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


float realSignal[FFT_SIZE];            // ���� ����� �� ���� (input �-FFT)
float imagSignal[FFT_SIZE];            // ���� ������ �� ���� (����� ��"� ������)
float spectrumMag[FFT_SIZE];           // ������ - ���� ������� ���� (�����)
int spectrumBar[FFT_SIZE];             // ���� ���� ���� ���� ��� ����� ���� �� ����

volatile float currentSample = 0;      // ��� ����� ����� ��-ADC
volatile int currentADC = 0;           // ��� ����� ����� ����� int
volatile bool newSampleFlag = false;   // ��� ������ ����� ����?
volatile int drawCursor = 0;           // ������ ����� �� ����
volatile bool graphFinished = false;   // ��� ����� ������ (������ �-main)
int loopCounter;                       // ����� ��� ���� �������
volatile float adcBuffer[FFT_SIZE] = {0};  // ���� �� ������ �-ADC
volatile int sampleIdx = 0;                 // ������ ����� ������ ����
volatile bool isBufferReady = false;       // ��� ����� ���?
volatile bool isProcessingDone = false;    // ��� ������ ������ (FFT)?
// ����� ����� - ����� �� �� ���� A ������ ����� ����
void InitLEDs() {
    AD1PCFG = 0xFFFF;        // ����� �� �� ������ ��������� ������ (���������)
    JTAGEN_bit = 0;          // ���� �� ���� �-JTAG ��� ����� �� ������
    TRISA = 0x0000;          // ����� �� �� ���� A ������
    LATA = 0;                // ���� �� �� ������ ������
}

// ����� ����� 1 � ���� ������ ����� ����� (���� �� 1ms)
void InitTimer1() {
    T1CON = 0x8010;          // ����� ����� �� ��� ���� 1:8
    T1IP0_bit = 1;           // ��� ������ ������ � ��� �����
    T1IP1_bit = 1;           // ��� ���
    T1IP2_bit = 1;           // ��� ����� (������ �����)
    T1IF_bit = 0;            // ����� ��� ������
    T1IE_bit = 1;            // ����� ����� ����� 1
    PR1 = 20000;             // ��� ������ � ���� �� ��� ������
    TMR1 = 0;                // ����� ���� ������
}

// ����� �� ����� �-ADC
void InitADC() {
    AD1PCFG = 0xFFFE;        // ���� RB0 ���� �������, �� ���� ���������
    JTAGEN_bit = 0;          // ���� �� �-JTAG
    TRISB0_bit = 1;          // ����� �� RB0 ����
    ADC1_Init();             // ����� ���� �-ADC
    Delay_ms(100);           // ����� ������
}

// ����� ��� �-TFT ����� ����� �������
void InitTFT() {
    TFT_BLED =1;                       // ����� �� ������ �������
    TFT_Init_ILI9341_8bit(320, 240);         // ����� ��� 320x240 ������� 8 ���
    TFT_Fill_Screen(CL_Yellow);              // ��� ��� ����
    TFT_Set_Pen(CL_BLACK, 20);               // ����� ��� �� ����� ����� 20
    TFT_Set_Font(TFT_defaultFont, CL_BLACK, FO_HORIZONTAL); // ����� �� �����
    TFT_Write_Text("Yoni and Daniel graph", 100, 100); // ���� ���� ����� ����
}

void DrawGraphAxes() {
    // ��� ����
    TFT_Fill_Screen(CL_YELLOW);

    // ��� Y
    TFT_Set_Pen(CL_BLACK, 2);
    TFT_Line(30, 220, 30, 60);  // �� ���� (Y)
    TFT_Write_Text("Amp", 5, 40);

    // �� ���� ������ �� ��� Y
    TFT_Line(30, 60, 27, 65);
    TFT_Line(30, 60, 33, 65);

    // ��� X
    TFT_Line(20, 210, 315, 210);  // �� ����� (X)
    TFT_Write_Text("Freq [Hz]", 260, 220);

    // �� ���� ����� �� ��� X
    TFT_Line(315, 210, 310, 207);
    TFT_Line(315, 210, 310, 213);



}

void DrawBar(int amplitude, int xCoord) {
    unsigned long yHeight = (unsigned long)amplitude;

    // ����� ���� ����
    if (yHeight > 160) yHeight = 160;
    else if (yHeight < 40) yHeight *= 2;
    else if (yHeight < 20) yHeight *= 4;

    if (xCoord < 32 || xCoord > 308) return;  // ���� �� ��� ����

    // ��� ����: ����
    TFT_Set_Pen(CL_BLACK, 2);

    // �� ������ �� ���� (210) ���� ���� ��� ���� ������
    TFT_line(xCoord, 210, xCoord, 210 - yHeight);
}


bool isTemplateDrawn = false;  // ��� ���� �� ���� ��� ����?

void DrawSpectrum(int* dataPtr, int dataSize) {
    if (!isTemplateDrawn) {
        DrawGraphAxes();         // ����� �� ������ ������
        isTemplateDrawn = true;
    }

    // ����� ��� ���� ���� �����
    TFT_Set_Pen(CL_YELLOW, 1);
    TFT_Set_Brush(1, CL_YELLOW, 0, LEFT_TO_RIGHT, CL_YELLOW, CL_YELLOW);
    TFT_Rectangle_Round_Edges(33, 25, 308, 208,1);



    for (drawCursor = 1; drawCursor < dataSize && drawCursor < 70; drawCursor++) {
        int x = 32 + drawCursor * 4;
        DrawBar(*(dataPtr + drawCursor), x);
    }

    graphFinished = true;
}




// ���� �� ������ (����) �� �� ��� �-FFT
void CalculateSpectrumMagnitude() {
    for (loopCounter = 0; loopCounter < FFT_SIZE; loopCounter++) {
        spectrumMag[loopCounter] = sqrt(realSignal[loopCounter] * realSignal[loopCounter] +
                                        imagSignal[loopCounter] * imagSignal[loopCounter]) / FFT_SIZE;

        spectrumBar[loopCounter] = (int)spectrumMag[loopCounter]; // ����� ���� ��� �����
    }
}
void BitReversal(float re[], float im[]) {
    unsigned int target = 0, pos, mask;

    for (pos = 0; pos < FFT_SIZE; pos++) {
        if (target > pos) {
            // ����� ����� ��� ����� ������ ����� �����
            float tempRe = re[target];
            float tempIm = im[target];
            re[target] = re[pos];
            im[target] = im[pos];
            re[pos] = tempRe;
            im[pos] = tempIm;
        }

        // ����� ������ ���� ��� ���-�����
        mask = FFT_SIZE;
        while (target & (mask >>= 1)) {
            target &= ~mask;
        }
        target |= mask;
    }
}
void ExecuteFFT(float re[], float im[]) {
    const float pi = -3.14159265358979323846; // ? ����� (��� ������ �� FFT)
    unsigned int stageSize, groupIdx, pairIdx;

    for (stageSize = 1; stageSize < FFT_SIZE; stageSize <<= 1) {
        unsigned int groupStep = stageSize << 1;     // ���� ��� �����
        float baseAngle, twRe, twIm;

        for (groupIdx = 0; groupIdx < stageSize; groupIdx++) {
            // ����� ������� (W) ��� �����
            baseAngle = pi * groupIdx / stageSize;
            twRe = cos(baseAngle);
            twIm = sin(baseAngle);

            for (pairIdx = groupIdx; pairIdx < FFT_SIZE; pairIdx += groupStep) {
                unsigned int match = pairIdx + stageSize;

                float tempRe = twRe * re[match] - twIm * im[match];
                float tempIm = twIm * re[match] + twRe * im[match];

                // ����� ������ ������� ��� FFT
                re[match] = re[pairIdx] - tempRe;
                im[match] = im[pairIdx] - tempIm;

                re[pairIdx] += tempRe;
                im[pairIdx] += tempIm;
            }
        }
    }
}
void RunSpectrumFFT(float re[], float im[]) {
    BitReversal(re, im);       // ����� ����� ��� ��� ��� ����
    ExecuteFFT(re, im);        // ����� ��FFT �����
}
void ApplyAlternatingSign() {
    for (loopCounter = 0; loopCounter < FFT_SIZE; loopCounter += 2) {
        realSignal[loopCounter] = -realSignal[loopCounter]; // ���� ���� ��������
        imagSignal[loopCounter] = -imagSignal[loopCounter]; // ���� ���� (window)
    }
}
void ProcessIncomingSignal() {
    // ����� ������� ������ �� ����� ������
    for (loopCounter = 0; loopCounter < FFT_SIZE; loopCounter++) {
        realSignal[loopCounter] = adcBuffer[loopCounter]; // �� ��� ����
        imagSignal[loopCounter] = 0;                      // ����� ������ �-0
    }

    ApplyAlternatingSign();             // ���� (��� ����)
    RunSpectrumFFT(realSignal, imagSignal); // ���� FFT
    CalculateSpectrumMagnitude();       // ����� ���� ��� ���
    isProcessingDone = true;            // ����� �������
}
// ����� ����� � ������ �� ��� ��� ���� (��� PR1) ������ ����� ��-ADC
void Timer1Interrupt() iv IVT_TIMER_1 ilevel 7 ics ICS_SRS {
    T1IF_bit = 0;                         // ���� �� ��� ������

    if (isBufferReady) return;           // �� ����� ��� ��� � �� ����

    currentADC = ADC1_Get_Sample(0);     // ���� ����� ����� 0 �� �-ADC
    currentSample = (float)currentADC;   // ���� ���� �-float
    newSampleFlag = true;                // ���� ������ ����� ����
}
bool processLock = false;     // ���� ����� ����
bool startCycle = false;      // ��� ������ ������ ����� ���

// ����� ���� ������ � ����� ������� �������� ����� ����
void StartNewAcquisitionCycle() {
    processLock = false;
    startCycle = false;
    isBufferReady = false;
    currentSample = 0;

    for (sampleIdx = 0; sampleIdx < FFT_SIZE; sampleIdx++) {
        adcBuffer[sampleIdx] = 0;        // ���� �� �� �����
    }

    sampleIdx = 0;

    LATAbits.LATA1 = 1;  // ����� �� 1 - ����� �����
    LATAbits.LATA2 = 0;  // ���� �� 2 - ����� �� ����
    EnableInterrupts();  // ����� ������
}
void main() {
    InitTFT();       // ����� ���
    InitADC();       // ����� ���� ADC
    InitLEDs();      // ����� ����
    InitTimer1();    // ����� ����� ������
    EnableInterrupts();

    LATAbits.LATA1 = 1; // �� 1 ���� ��������� �����

    while (1) {
        if (currentSample != 0 || startCycle) {
            startCycle = true;  // ������ ������ �����

            if (newSampleFlag && sampleIdx < FFT_SIZE) {
                newSampleFlag = false;                      // ����� ��� �����
                adcBuffer[sampleIdx++] = currentSample;     // ����� �����
            }
            else if (!processLock && sampleIdx >= FFT_SIZE) {
                DisableInterrupts();         // ��� ���� ������ � ������ �� �������
                processLock = true;

                LATAbits.LATA1 = 0;          // �� ����� ����
                LATAbits.LATA2 = 1;          // �� ����� ����
                isBufferReady = true;        // ����� ������ ����

                ProcessIncomingSignal();     // ����� FFT

                if (isProcessingDone) {
                    DrawSpectrum(spectrumBar + FFT_SIZE / 2, FFT_SIZE / 2); // ����� ��� �������
                    isProcessingDone = false;
                }

                if (graphFinished) {
                    TFT_Write_Text("Yoni and Daniel graph", 100, 10);  // ����� �� ����
                    StartNewAcquisitionCycle(); // ����� ��� �����
                    Delay_ms(1000);             // ����� ���
                    graphFinished = false;
                }
            }
        }
    }
}