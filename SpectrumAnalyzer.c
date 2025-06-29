#include <stdbool.h>        // מאפשר שימוש ב-type בוליאני כמו true ו-false
#include <stdio.h>          // ספרייה סטנדרטית לפונקציות קלט/פלט כמו printf
#include <math.h>           // ספרייה מתמטית - עבור פעולות כמו sqrt(), sin(), cos()
#define FFT_SIZE 256        // מגדיר את מספר הדגימות והגודל של ה-FFT

// חיבורי נתונים ובקרה למסך
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


float realSignal[FFT_SIZE];            // החלק הממשי של האות (input ל-FFT)
float imagSignal[FFT_SIZE];            // החלק המדומה של האות (מתאפס בד"כ בתחילה)
float spectrumMag[FFT_SIZE];           // התוצאה - גודל ספקטרום האות (משרעת)
int spectrumBar[FFT_SIZE];             // אותו גודל מומר לערך שלם לצורך ציור על המסך

volatile float currentSample = 0;      // ערך דגימה אחרון מה-ADC
volatile int currentADC = 0;           // ערך דגימה אחרון בצורת int
volatile bool newSampleFlag = false;   // האם התקבלה דגימה חדשה?
volatile int drawCursor = 0;           // אינדקס לציור על המסך
volatile bool graphFinished = false;   // האם הציור הסתיים (בשימוש ב-main)
int loopCounter;                       // משתנה עזר כללי ללולאות
volatile float adcBuffer[FFT_SIZE] = {0};  // מאגר של דגימות מ-ADC
volatile int sampleIdx = 0;                 // אינדקס כתיבה לדגימה הבאה
volatile bool isBufferReady = false;       // האם המאגר מלא?
volatile bool isProcessingDone = false;    // האם הסתיים העיבוד (FFT)?
// אתחול הלדים - מגדיר את כל פורט A כיציאה ומאפס אותו
void InitLEDs() {
    AD1PCFG = 0xFFFF;        // מגדיר את כל הפינים כאנלוגיים כבויים (דיגיטליים)
    JTAGEN_bit = 0;          // מכבה את ממשק ה-JTAG כדי לשחרר את הפינים
    TRISA = 0x0000;          // מגדיר את כל פורט A כיציאה
    LATA = 0;                // מאפס את כל הפינים ביציאה
}

// אתחול טיימר 1 – משמש ליצירת דגימה קבועה (למשל כל 1ms)
void InitTimer1() {
    T1CON = 0x8010;          // הפעלת טיימר עם קדם מחלק 1:8
    T1IP0_bit = 1;           // רמת עדיפות לפסיקה – ביט ראשון
    T1IP1_bit = 1;           // ביט שני
    T1IP2_bit = 1;           // ביט שלישי (עדיפות גבוהה)
    T1IF_bit = 0;            // איפוס דגל הפסיקה
    T1IE_bit = 1;            // הפעלת פסיקת טיימר 1
    PR1 = 20000;             // ערך השוואה – קובע את קצב הפסיקה
    TMR1 = 0;                // איפוס מונה הטיימר
}

// אתחול של כניסת ה-ADC
void InitADC() {
    AD1PCFG = 0xFFFE;        // הפין RB0 נשאר כאנלוגי, כל השאר דיגיטליים
    JTAGEN_bit = 0;          // מכבה את ה-JTAG
    TRISB0_bit = 1;          // מגדיר את RB0 כקלט
    ADC1_Init();             // אתחול רכיב ה-ADC
    Delay_ms(100);           // המתנה לייצוב
}

// אתחול מסך ה-TFT והצגת כותרת ראשונית
void InitTFT() {
    TFT_BLED =1;                       // מדליק את התאורה האחורית
    TFT_Init_ILI9341_8bit(320, 240);         // אתחול מסך 320x240 בתקשורת 8 ביט
    TFT_Fill_Screen(CL_Yellow);              // צבע רקע צהוב
    TFT_Set_Pen(CL_BLACK, 20);               // מגדיר צבע עט לשחור ועובי 20
    TFT_Set_Font(TFT_defaultFont, CL_BLACK, FO_HORIZONTAL); // מגדיר את הפונט
    TFT_Write_Text("Yoni and Daniel graph", 100, 100); // כותב טקסט באמצע המסך
}

void DrawGraphAxes() {
    // רקע כללי
    TFT_Fill_Screen(CL_YELLOW);

    // ציר Y
    TFT_Set_Pen(CL_BLACK, 2);
    TFT_Line(30, 220, 30, 60);  // קו אנכי (Y)
    TFT_Write_Text("Amp", 5, 40);

    // חץ בקצה העליון של ציר Y
    TFT_Line(30, 60, 27, 65);
    TFT_Line(30, 60, 33, 65);

    // ציר X
    TFT_Line(20, 210, 315, 210);  // קו אופקי (X)
    TFT_Write_Text("Freq [Hz]", 260, 220);

    // חץ בקצה הימני של ציר X
    TFT_Line(315, 210, 310, 207);
    TFT_Line(315, 210, 310, 213);



}

void DrawBar(int amplitude, int xCoord) {
    unsigned long yHeight = (unsigned long)amplitude;

    // הגבלת גובה סביר
    if (yHeight > 160) yHeight = 160;
    else if (yHeight < 40) yHeight *= 2;
    else if (yHeight < 20) yHeight *= 4;

    if (xCoord < 32 || xCoord > 308) return;  // הגנה על קצה הגרף

    // צבע הגרף: שחור
    TFT_Set_Pen(CL_BLACK, 2);

    // קו מהבסיס של הגרף (210) כלפי מעלה לפי גובה המשרעת
    TFT_line(xCoord, 210, xCoord, 210 - yHeight);
}


bool isTemplateDrawn = false;  // האם הרקע של הגרף כבר צויר?

void DrawSpectrum(int* dataPtr, int dataSize) {
    if (!isTemplateDrawn) {
        DrawGraphAxes();         // מצייר את הצירים והחצים
        isTemplateDrawn = true;
    }

    // מחיקת גרף קודם בתוך האזור
    TFT_Set_Pen(CL_YELLOW, 1);
    TFT_Set_Brush(1, CL_YELLOW, 0, LEFT_TO_RIGHT, CL_YELLOW, CL_YELLOW);
    TFT_Rectangle_Round_Edges(33, 25, 308, 208,1);



    for (drawCursor = 1; drawCursor < dataSize && drawCursor < 70; drawCursor++) {
        int x = 32 + drawCursor * 4;
        DrawBar(*(dataPtr + drawCursor), x);
    }

    graphFinished = true;
}




// מחשב את המשרעת (גודל) של כל תדר ב-FFT
void CalculateSpectrumMagnitude() {
    for (loopCounter = 0; loopCounter < FFT_SIZE; loopCounter++) {
        spectrumMag[loopCounter] = sqrt(realSignal[loopCounter] * realSignal[loopCounter] +
                                        imagSignal[loopCounter] * imagSignal[loopCounter]) / FFT_SIZE;

        spectrumBar[loopCounter] = (int)spectrumMag[loopCounter]; // שמירה כערך שלם לציור
    }
}
void BitReversal(float re[], float im[]) {
    unsigned int target = 0, pos, mask;

    for (pos = 0; pos < FFT_SIZE; pos++) {
        if (target > pos) {
            // החלפת מיקום בין האיבר הנוכחי לאיבר המטרה
            float tempRe = re[target];
            float tempIm = im[target];
            re[target] = re[pos];
            im[target] = im[pos];
            re[pos] = tempRe;
            im[pos] = tempIm;
        }

        // עדכון אינדקס היעד לפי ביט-רברסל
        mask = FFT_SIZE;
        while (target & (mask >>= 1)) {
            target &= ~mask;
        }
        target |= mask;
    }
}
void ExecuteFFT(float re[], float im[]) {
    const float pi = -3.14159265358979323846; // ? שלילי (לפי ההגדרה של FFT)
    unsigned int stageSize, groupIdx, pairIdx;

    for (stageSize = 1; stageSize < FFT_SIZE; stageSize <<= 1) {
        unsigned int groupStep = stageSize << 1;     // מרחק בין זוגות
        float baseAngle, twRe, twIm;

        for (groupIdx = 0; groupIdx < stageSize; groupIdx++) {
            // חישוב טוויידל (W) לכל קבוצה
            baseAngle = pi * groupIdx / stageSize;
            twRe = cos(baseAngle);
            twIm = sin(baseAngle);

            for (pairIdx = groupIdx; pairIdx < FFT_SIZE; pairIdx += groupStep) {
                unsigned int match = pairIdx + stageSize;

                float tempRe = twRe * re[match] - twIm * im[match];
                float tempIm = twIm * re[match] + twRe * im[match];

                // חיבור וחיסור מרוכבים לפי FFT
                re[match] = re[pairIdx] - tempRe;
                im[match] = im[pairIdx] - tempIm;

                re[pairIdx] += tempRe;
                im[pairIdx] += tempIm;
            }
        }
    }
}
void RunSpectrumFFT(float re[], float im[]) {
    BitReversal(re, im);       // סידור ביטים לפי סדר ביט הפוך
    ExecuteFFT(re, im);        // חישוב ה־FFT בפועל
}
void ApplyAlternatingSign() {
    for (loopCounter = 0; loopCounter < FFT_SIZE; loopCounter += 2) {
        realSignal[loopCounter] = -realSignal[loopCounter]; // הופך סימן לסירוגין
        imagSignal[loopCounter] = -imagSignal[loopCounter]; // מדמה חלון (window)
    }
}
void ProcessIncomingSignal() {
    // העתקת הדגימות מהבאפר אל משתני העבודה
    for (loopCounter = 0; loopCounter < FFT_SIZE; loopCounter++) {
        realSignal[loopCounter] = adcBuffer[loopCounter]; // רק חלק ממשי
        imagSignal[loopCounter] = 0;                      // מדומה מאותחל ל-0
    }

    ApplyAlternatingSign();             // הכנה (כמו חלון)
    RunSpectrumFFT(realSignal, imagSignal); // הרצת FFT
    CalculateSpectrumMagnitude();       // חישוב גודל לכל תדר
    isProcessingDone = true;            // סימון שסיימנו
}
// פסיקת טיימר – מתבצעת כל פרק זמן קבוע (לפי PR1) ומבצעת דגימה מה-ADC
void Timer1Interrupt() iv IVT_TIMER_1 ilevel 7 ics ICS_SRS {
    T1IF_bit = 0;                         // מאפס את דגל הפסיקה

    if (isBufferReady) return;           // אם הבאפר כבר מלא – לא דוגם

    currentADC = ADC1_Get_Sample(0);     // קורא דגימה מערוץ 0 של ה-ADC
    currentSample = (float)currentADC;   // ממיר אותה ל-float
    newSampleFlag = true;                // מסמן שהגיעה דגימה חדשה
}
bool processLock = false;     // מונע עיבוד כפול
bool startCycle = false;      // האם נכנסנו למחזור עבודה חדש

// אתחול למצב התחלתי – איפוס המשתנים ומתחילים דגימה מחדש
void StartNewAcquisitionCycle() {
    processLock = false;
    startCycle = false;
    isBufferReady = false;
    currentSample = 0;

    for (sampleIdx = 0; sampleIdx < FFT_SIZE; sampleIdx++) {
        adcBuffer[sampleIdx] = 0;        // מאפס את כל הבאפר
    }

    sampleIdx = 0;

    LATAbits.LATA1 = 1;  // מדליק לד 1 - דגימה פעילה
    LATAbits.LATA2 = 0;  // מכבה לד 2 - עדיין לא מעבד
    EnableInterrupts();  // מאפשר פסיקות
}
void main() {
    InitTFT();       // אתחול מסך
    InitADC();       // אתחול ממיר ADC
    InitLEDs();      // הגדרת לדים
    InitTimer1();    // אתחול טיימר לדגימה
    EnableInterrupts();

    LATAbits.LATA1 = 1; // לד 1 דולק כשמתחילים דגימה

    while (1) {
        if (currentSample != 0 || startCycle) {
            startCycle = true;  // נכנסנו למחזור עבודה

            if (newSampleFlag && sampleIdx < FFT_SIZE) {
                newSampleFlag = false;                      // איפוס דגל דגימה
                adcBuffer[sampleIdx++] = currentSample;     // שמירה בבאפר
            }
            else if (!processLock && sampleIdx >= FFT_SIZE) {
                DisableInterrupts();         // אין יותר דגימות – עוצרים את הפסיקות
                processLock = true;

                LATAbits.LATA1 = 0;          // לד דגימה נכבה
                LATAbits.LATA2 = 1;          // לד עיבוד נדלק
                isBufferReady = true;        // סימון שהבאפר מוכן

                ProcessIncomingSignal();     // עיבוד FFT

                if (isProcessingDone) {
                    DrawSpectrum(spectrumBar + FFT_SIZE / 2, FFT_SIZE / 2); // מצייר חצי ספקטרום
                    isProcessingDone = false;
                }

                if (graphFinished) {
                    TFT_Write_Text("Yoni and Daniel graph", 100, 10);  // כיתוב על המסך
                    StartNewAcquisitionCycle(); // מחזור חדש מתחיל
                    Delay_ms(1000);             // המתנה קלה
                    graphFinished = false;
                }
            }
        }
    }
}