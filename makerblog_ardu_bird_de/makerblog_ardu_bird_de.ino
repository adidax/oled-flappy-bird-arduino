/***************************************************************************
 Flappy Bird für Arduino mit OLED Display 128x64 - Kommentare in deutsch
 https://www.makerblog.at

 - Arduino UNO R3 (oder vergleichbar)
 - OLED I2C Display 128x64 mit SSD1306
 - Push Button

 Verkabelung:
 
 OLED -> Arduino UNO R3
 SDA -> A4
 SCL -> A5
 GND -> GND
 VIN -> 5V

 Push Button: GND - Push Button - D2

 Folgende Libraries müssen installiert sein, bei Fehlermeldung bitte mit dem Library Manager 
 kontrollieren

 - Adafruit SSD1306 inkl. Adafruit GFX

  Optimierung:
 -------------
 Um Gleitkommazahlen (float) zu vermeiden, werden alle relevanten Positionen 
 und Geschwindigkeiten mit dem SCALE_FACTOR = 10 multipliziert und als int gespeichert. 
 Dies verbessert die Performance und vermeidet Rundungsfehler.

 Skalierte Werte:
 - Koordinaten werden *10 gespeichert.
 - Zur Darstellung auf dem Display werden diese Werte durch SCALE_FACTOR = 10 geteilt .

 Beispiel:
 - Ein Rohr x = 350 befindet sich in Wirklichkeit bei x = 35 Pixeln auf dem Display.
 
 Diese Skalierung wird in allen Berechnungen berücksichtigt, aber 
 in der Darstellung (drawBitmap, drawFastHLine usw.) auf den echten Pixelwert 
 zurückgerechnet (x / SCALE_FACTOR).
 
 ***************************************************************************/


#include <avr/pgmspace.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Display Einstellungen
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int OLED_RESET = -1;
const int SCREEN_ADDRESS = 0x3C; // I2C Adresse des Displays, siehe Datenblatt
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const bool DEBUG_PRINT = false;  // auf true für Debug-Ausgaben am seriellen Monitor

// Button
const int BUTTON_PIN = 2; // Push-Button an Digitalpin 2

// Physik und Hindernisse
const int SCALE_FACTOR = 10;  // Interne Skalierung um Faktor 10 für präzisere Bewegungen
const int GRAVITY = 5; // Schwerkraft, die pro Frame auf Spielfigur einwirkt
const int JUMP_STRENGTH = -25; // Sprung-Beschleunigung bei gedrücktem Pushbutton
const int PIPE_WIDTH = 15 * SCALE_FACTOR; // Rohre an Display 15px breit
const int PIPE_GAP = 30 * SCALE_FACTOR; // Lücke in Rohren 30px hoch
const int PIPE_SPEED = 10; // Geschwindigkeit des Sidescrolls
const int PIPE_SPACING = 65 * SCALE_FACTOR;  // Default Abstand zwischen 2 Pipes

// Größe des Spieler-Sprites
const int BIRD_WIDTH_PX = 10;
const int BIRD_HEIGHT_PX = 8;

// Grafiken für Spielfigur
// konvertiert aus PNG mit https://javl.github.io/image2cpp/

// 'bird1', 10x8px
const unsigned char epd_bitmap_bird1[] PROGMEM = {
  0x0e, 0x00, 0x15, 0x00, 0x60, 0x80, 0x81, 0x80, 0xfc, 0xc0, 0xfb, 0x80, 0x71, 0x00, 0x1e, 0x00
};
// 'bird2', 10x8px
const unsigned char epd_bitmap_bird2[] PROGMEM = {
  0x0e, 0x00, 0x15, 0x00, 0x70, 0x80, 0xf9, 0x80, 0xfc, 0xc0, 0x83, 0x80, 0x71, 0x00, 0x1e, 0x00
};

// Array of all bitmaps for convenience. (Total bytes used to store images in PROGMEM = 64)
const int epd_bitmap_allArray_LEN = 2;
const unsigned char* epd_bitmap_allArray[2] = {
  epd_bitmap_bird1,
  epd_bitmap_bird2
};

// Game Variablen
int birdY; // Vertikale Position des Spielers
float velocity = 0; // Kombinierte vertikale Geschwindigkeit der Spielfigur
bool gameOver = false;
int score = 0;
unsigned long frameCounter = 0;
unsigned long gameOverTime = 0;

// Spielstatus
enum GameState { STARTSCREEN,
                 HIGHSCORE,
                 PLAYING };
GameState gameState = STARTSCREEN;

// Datentyp für ein Rohr (x-Position, Höhe des oberen Teils)
struct Pipe {
  int x;
  int height;
};

// Maximal 3 Rohre gleichzeitig
const int MAX_PIPES = 3;
Pipe pipes[MAX_PIPES];

// Highscore
long highscores[5] = {};
int newHighscoreIndex = -1;
unsigned long highscoreTime = 0;
bool showBlink = true;
unsigned long lastBlinkTime = 0;

// Framerate Steuerung
const int FRAME_TIME = 40;  // 40ms pro Frame = 25 FPS
unsigned long lastFrameTime = 0;


void setup() {
  Serial.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  randomSeed(analogRead(0));

  // Display initialisieren oder Pause bei Fehler
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1) {
      delay(100);
    }
  }

  gameState = STARTSCREEN;  
}

void loop() {
  unsigned long currentTime = millis();

  // Stelle sicher, dass das nächste Frame erst nach 40ms berechnet wird
  if (currentTime - lastFrameTime < FRAME_TIME) return;
  lastFrameTime = currentTime;
  frameCounter++;

  switch (gameState) {
    case STARTSCREEN:
      renderStartScreen();
      if (anyKeyPressed()) {
        resetGame(); // Variablen zurücksetzen
        gameState = PLAYING; // Spiel starten
      }
      break;

    case PLAYING:
      if (gameOver) {
        // Bei Game over nach 3 Sekunden automatisch zum Startscreen wechseln
        if (millis() - gameOverTime > 3000) {  
          gameState = HIGHSCORE;
          highscoreTime = millis();
        }
        // Oder Spieler drückt den Button (1 sec Zwangspause), dann sofort neues Spiel starten
        if ((millis() - gameOverTime > 1000) && anyKeyPressed()) {
          resetGame();
          gameState = PLAYING;
        }
      } else {

        // Wenn Button gedrückt, negative Beschleunigung (nach oben) addieren
        if (digitalRead(BUTTON_PIN) == LOW) {
            velocity = JUMP_STRENGTH;
        }
        velocity += GRAVITY; // Schwerkraft anwenden (positiver Wert = nach unten)
        velocity = constrain(velocity, -50, 50); // Begrenzung, damit Spielfigur nicht zu schnell fällt oder steigt

        // Position des Spielers berechnen
        birdY += velocity;
        // Wenn Spieler ganz oben (y=0), nicht aus dem Bild verschwinden lassen
        if (birdY < 0) birdY = 0;

        if (DEBUG_PRINT) {
          Serial.print("Vel: ");
          Serial.println(velocity);
        }

        movePipes();
        checkCollision();
      }
      drawGame();
      break;

    case HIGHSCORE:
      renderHighscoreScreen();
      // Nach 8 sec ohne Tastendruck zum Startscreen wechseln
      if (millis() - gameOverTime > 8000) {
        gameState = STARTSCREEN;
      }
      // oder neues Spiel starten wenn Tastendruck
      if (anyKeyPressed()) {
        resetGame();
        gameState = PLAYING;
      }
      break;
  }
}

// PULLUP-Logik: LOW bedeutet gedrückt
bool anyKeyPressed() {
  return digitalRead(BUTTON_PIN) == LOW;
}

// ---- Reset Spiel ---- //
void resetGame() {
  // Startposition des Spielers auf 1/5 der Bildschirmhöhe
  birdY = SCREEN_HEIGHT * SCALE_FACTOR / 5;
  velocity = 0;
  score = 0;
  gameOver = false;

  // Die erste Pipe direkt außerhalb des Bildschirmrandes erzeugen
  pipes[0].x = SCREEN_WIDTH * SCALE_FACTOR;

  pipes[0].height = generatePipeHeight();
  // Pipes 2 und 3 erzeugen, mit zufälliger Variation im Abstand
  for (int i = 1; i < MAX_PIPES; i++) {
    pipes[i].x = SCREEN_WIDTH * SCALE_FACTOR + i * PIPE_SPACING + random(-20, 20) * SCALE_FACTOR;
    pipes[i].height = generatePipeHeight();
  }
}

 // Zufällige Höhe des oberen Rohrteils, aber so dass mindestens Platz für Öffnung und 10px für unteren Teil bleibt 
int generatePipeHeight() {
  return random(10 * SCALE_FACTOR, SCREEN_HEIGHT * SCALE_FACTOR - PIPE_GAP - 10 * SCALE_FACTOR);
}


// Pipes auf den Spieler zu bewegen
void movePipes() {

  int currentSpeed = PIPE_SPEED + score; // Pipes bewegen sich immer schneller

  for (int i = 0; i < MAX_PIPES; i++) {
    // Position der Pipe um currentSpeed nach links verschieben
    pipes[i].x -= currentSpeed;

    // Wenn Pipe den linken Rand verlässt, sofort neue Pipe am  rechten Rand erzeugen
    if (pipes[i].x < -PIPE_WIDTH) {
      pipes[i].x = findFurthestPipe() + PIPE_SPACING + random(-20, 20) * SCALE_FACTOR;  // Setze neue Pipe ganz rechts
      pipes[i].height = generatePipeHeight();
      score++;
    }
  }
}

// Hilfsfunktion, um die am weitesten rechts stehende Pipe zu finden
int findFurthestPipe() {
  int maxX = 0;
  for (int i = 0; i < MAX_PIPES; i++) {
    if (pipes[i].x > maxX) {
      maxX = pipes[i].x;
    }
  }
  return maxX;
}

// ---- Kollisionserkennung ---- //
void checkCollision() {

// Kollisionsprüfung mit um 2px verkleinerter Hitbox (Collision Forgiveness),
// da Spielfigur eher rund ist → vermeidet unfaire Treffer an Ecken der Spielfigur
  for (int i = 0; i < MAX_PIPES; i++) {
    // Prüfen, ob die Spielfigur horizontal mit einem Rohr überlappt 
    if (pipes[i].x < (15 + 2) * SCALE_FACTOR && pipes[i].x + PIPE_WIDTH > (15 - 2) * SCALE_FACTOR) {
      // Prüfen, ob die Spielfigur vertikal gegen das Rohr stößt (oben oder unten)
      if ((birdY - 2 * SCALE_FACTOR) < pipes[i].height || (birdY + 2 * SCALE_FACTOR) > pipes[i].height + PIPE_GAP) {
        gameOver = true;
        gameOverTime = millis();
        saveHighscores(score);
      }
    }
  }

  if (birdY >= SCREEN_HEIGHT * SCALE_FACTOR) {
    gameOver = true;
    gameOverTime = millis();
    saveHighscores(score);
  }

}


// ---- Spiel zeichnen ---- //
void drawGame() {
  display.clearDisplay();

  drawBird();
  drawPipes();
  drawScore();

  if (gameOver) displayGameOver();  // Wenn Game over, dann Overlay ausgeben

  display.display();
}

void drawBird() {
  // Spielfigur hat Animation mit 2 Bildern, diese werden abwechselnd alle 5 Frames dargestellt.
  int birdFrame = (frameCounter / 5) % 2;
  display.drawBitmap(15 - BIRD_WIDTH_PX / 2, birdY / SCALE_FACTOR - BIRD_HEIGHT_PX / 2,
                     epd_bitmap_allArray[birdFrame], BIRD_WIDTH_PX, BIRD_HEIGHT_PX, SSD1306_WHITE);
}

void drawPipes() {
  for (int pipeIndex = 0; pipeIndex < MAX_PIPES; pipeIndex++) {
    int pipeX = pipes[pipeIndex].x / SCALE_FACTOR;
    int upperPipeHeight = pipes[pipeIndex].height / SCALE_FACTOR;
    int gapHeight = PIPE_GAP / SCALE_FACTOR;
    int lowerPipeY = upperPipeHeight + gapHeight;
    int lowerPipeHeight = SCREEN_HEIGHT - lowerPipeY;    
    int pipeWidth = PIPE_WIDTH / SCALE_FACTOR;

    // Oberes Rohrsegment (vertikal)
    display.fillRect(pipeX, 0, pipeWidth, upperPipeHeight - 6, SSD1306_WHITE);
    // Rohrkopf oben (horizontaler Abschluss)
    display.fillRect(pipeX - 2, upperPipeHeight - 6, pipeWidth + 4, 6, SSD1306_WHITE);
    // Rohrkopf unten (horizontaler Abschluss unterhalb der Lücke)
    display.fillRect(pipeX - 2, lowerPipeY, pipeWidth + 4, 6, SSD1306_WHITE);
    // Unteres Rohrsegment (vertikal)
    display.fillRect(pipeX, lowerPipeY + 6, pipeWidth, lowerPipeHeight - 6, SSD1306_WHITE);
  }
}

// Rechts oben im Display aktuellen Score anzeigen
void drawScore() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(110, 2);
  display.print(score);
}

// ---- Startscreen anzeigen ---- //
void renderStartScreen() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println("Ardu Bird");
  display.setTextSize(1);
  display.setCursor(20, 40);
  display.println("Press to start");
  display.display();
}

// ---- Game Over anzeigen ---- //
void displayGameOver() {
  display.fillRect(20, 5, SCREEN_WIDTH - 40, SCREEN_HEIGHT - 10, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(38, 10);
  display.println("GAME OVER");
  display.setCursor(35, 20);
  display.println("Your score");

  int16_t x1, y1;
  uint16_t textWidth, textHeight;
  char scoreBuffer[10];               // Buffer für die Zahl als String
  sprintf(scoreBuffer, "%d", score);  // Wandelt Zahl in String um
  display.setTextSize(2);
  display.getTextBounds(scoreBuffer, 0, 0, &x1, &y1, &textWidth, &textHeight);  // Score-Breite berechnen für Zentrierung
  display.setCursor(64 - textWidth / 2, 36);
  //  display.setCursor(40, 26);
  display.println(scoreBuffer);
}

// ---- Highscore Liste anzeigen ---- //
void renderHighscoreScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(25, 2);
  display.println("HIGHSCORES");
  display.setTextSize(1);

  // Aktuell erreichter Score soll blinken, wenn in Top10, Blinkstatus alle 100ms ändern
  if (millis() - lastBlinkTime > 100) {
    showBlink = !showBlink;
    lastBlinkTime = millis();
  }

  for (int i = 0; i < 5; i++) {
    if (i == newHighscoreIndex && !showBlink) {
      continue;
    }
    display.setCursor(30, 14 + (i * 9));
    display.print(i + 1);
    display.print(".    ");
    display.print(highscores[i]);
  }
  display.display();
}

// ---- Neuen Highscore eintragen ---- //
void saveHighscores(int newScore) {
  // neuer Score wird mit bestehenden Scores verglichen und ggf richtig einsortiert
  newHighscoreIndex = -1;
  for (int i = 0; i < 5; i++) {
    if (newScore > highscores[i]) {
      for (int j = 4; j > i; j--) {
        highscores[j] = highscores[j - 1];
      }
      highscores[i] = newScore;
      newHighscoreIndex = i;
      break;
    }
  }
}
