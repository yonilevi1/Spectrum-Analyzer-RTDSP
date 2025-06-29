#define DAC_PIN DAC1  // יציאת DAC – פין אנלוגי (DAC1 בארדואינו Due)

const int tableSize = 256;         // מספר הדגימות במחזור אחד של האות
float signalTable[tableSize];      // טבלת ערכים (Lookup Table) של האות שישודר ל-DAC

// הגדרה של סוגי האותות האפשריים
enum SignalType {SINUS, SAWTOOTH, PULSE};
SignalType signalType = SINUS;     // ברירת מחדל: סוג האות הוא גל סינוס

// הגדרת אמפליטודות אפשריות לוולטים (רשימה)
float amplitudes[] = {2.0, 5.0};
int currentAmpIndex = 0;           // אינדקס נוכחי באמפליטודות – מתחיל מהראשונה (2.0V)

// רשימת תדרים אפשריים לאות
float frequencies[] = {10.0, 55.0};
int currentFreqIndex = 0;          // אינדקס נוכחי ברשימת התדרים – מתחיל מ־10Hz

unsigned long lastUpdate = 0;      // משתנה לזמן עדכון אחרון של דגימה ל-DAC
float sampleRate = 1000.0;         // קצב דגימה – 1000 דגימות לשנייה = כל 1ms
float t = 0;                        // מונה הדגימות שחלפו – משמש לחישוב אינדקס בטבלה

void setup() {
  analogWriteResolution(12);       // הגדרת רזולוציית ה-DAC ל־12 ביט (ערכים בין 0 ל־4095)
  Serial.begin(9600);              // הפעלת תקשורת סריאלית במהירות 9600 ביט לשנייה
  buildSignalTable();              // יצירת טבלת האות הראשונית לפי סוג, תדר ואמפליטודה
}

void loop() {
  // בדיקה האם התקבל תו מהטרמינל
  if (Serial.available()) {
    char cmd = Serial.read();  // קריאת הפקודה מהמשתמש

    if (cmd == 'S' || cmd == 's') {
      // החלפה בין סוגי האותות (מעגלי: סינוס → שן מסור → פולס → סינוס)
      signalType = (SignalType)((signalType + 1) % 3);
      Serial.println("Switched Signal Type");
      
      // הצגת סוג האות החדש
      switch (signalType) {
        case SINUS:   Serial.print("SINUS");   break;
        case SAWTOOTH:Serial.print("SAWTOOTH");break;
        case PULSE:   Serial.print("PULSE");   break;
      }
      Serial.println(" ");
      buildSignalTable();  // עדכון הטבלה בהתאם לסוג החדש
    }

    if (cmd == 'A' || cmd == 'a') {
      // מעבר לאמפליטודה הבאה ברשימה
      currentAmpIndex = (currentAmpIndex + 1) % 2;
      Serial.print("Amplitude: ");
      Serial.print(amplitudes[currentAmpIndex]);
      Serial.println(" V");
      buildSignalTable();  // עדכון הטבלה לפי האמפליטודה החדשה
    }

    if (cmd == 'F' || cmd == 'f') {
      // מעבר לתדר הבא ברשימה
      currentFreqIndex = (currentFreqIndex + 1) % 2;
      Serial.print("Frequency: ");
      Serial.print(frequencies[currentFreqIndex]);
      Serial.println(" Hz");
    }
  }

  // הפקת האות בפועל – שליחת ערך מתאים ל-DAC כל 1ms
  unsigned long now = micros();  // זמן נוכחי במיקרושניות
  if (now - lastUpdate >= (1000000.0 / sampleRate)) {
    lastUpdate = now;  // עדכון זמן הדגימה האחרונה
    float freq = frequencies[currentFreqIndex];  // תדר נוכחי
    // חישוב אינדקס בטבלה לפי הזמן והתדר
    int index = (int)(t * tableSize * freq / sampleRate) % tableSize;
    analogWrite(DAC_PIN, (int)signalTable[index]);  // שליחת ערך מתוך הטבלה ל-DAC
    t += 1.0;  // עדכון מונה הדגימות
  }
}

// פונקציה לבניית טבלת הערכים של האות בהתאם לסוג, אמפליטודה ותדר
void buildSignalTable() {
  float amp = amplitudes[currentAmpIndex];     // האמפליטודה שנבחרה
  float dac_max = 3.3;                          // מתח מרבי שה-DAC יכול להוציא (במתח לוגי רגיל)
  float dac_scale = 4095.0 / dac_max;           // יחס המרה מוולטים לערכי DAC של 12 ביט

  for (int i = 0; i < tableSize; i++) {
    float value = 0;  // משתנה מקומי לאחסון ערך האות לדגימה i

    switch (signalType) {
      case SINUS:
        // גל סינוס עם אפנון DC – נע סביב 1.65V
        value = 1.65 + amp * sin(2 * PI * i / tableSize);
        break;

      case SAWTOOTH:
        // גל שן מסור – קו ליניארי עולה מ־1.65V מינוס אמפליטודה עד 1.65 פלוס אמפליטודה
        value = 1.65 + amp * (2.0 * i / tableSize - 1.0);
        break;

      case PULSE:
        // גל פולס – חצי ראשון גבוה, חצי שני נמוך סביב 1.65V
        value = (i < tableSize / 2) ? (1.65 + amp) : (1.65 - amp);
        break;
    }

    // הגבלת הערך למתח התחום של DAC
    if (value > 3.3) value = 3.3;
    if (value < 0)   value = 0;

    // המרה לערך דיגיטלי (12 ביט) ואחסון בטבלה
    signalTable[i] = value * dac_scale;
  }
}
