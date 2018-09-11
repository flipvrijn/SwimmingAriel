/******************************************************************
 Created with PROGRAMINO IDE for Arduino - 03.09.2018 11:44:28
 Project     :
 Libraries   :
 Author      :
 Description :
******************************************************************/

#include <SPI.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include "TouchScreen_kbv.h"
#include "Vector.h"

#include "bitmaps.h"

#define LCD_CS A3 // Chip Select
#define LCD_CD A2 // Command/Data
#define LCD_WR A1 // LCD Write
#define LCD_RD A0 // LCD Read
#define LCD_RESET A4

#define PORTRAIT      0
#define LANDSCAPE     1
#define PORTRAIT_REV  2
#define LANDSCAPE_REV 3

#define YP A3
#define XM A2
#define YM 9
#define XP 8
#define MIN_PRESSURE 200
#define MAX_PRESSURE 1000

const uint16_t WHITE = 0xFFFF;
const uint16_t RED = 0xF800;
const uint16_t GRAY = 0x8410;
const uint16_t BLACK = 0x0000;
const uint16_t BACKGROUND = 0x02D3;
const uint16_t PILLAR = 0x5B4E;
const uint16_t PILLAR_HIGHLIGHT = 0x9577;

MCUFRIEND_kbv tft;
int SCREEN_ORIENTATION = LANDSCAPE_REV;
uint16_t SCREEN_WIDTH = 320;
uint16_t SCREEN_HEIGHT = 240;
TouchScreen_kbv touchScreen(XP, YP, XM, YM, 300);
TSPoint_kbv touchPoint;

const int REFRESH_RATE = 60;
unsigned long nextRefreshTime;
int bottomBarHeight = 15;

void gameOver();
void resetGame();

struct Player;
struct Pillar;

struct Pillar {
  int pillarNumber = 1;
  int position;
  int gapPosition;
  int gapHeight;
  int speed;
  int speedIncrementEvery = 2;
  int speedBoost = 1;
  int width;
  
  void init() {
    position = SCREEN_WIDTH;
    gapPosition = 50;
    gapHeight = 90;
    width = 30;
    speed = 5;
  }
  
  void draw() {
    if (position >= 0 && position < SCREEN_WIDTH) {
      int highlightWidth = 5;
      // Pillar base color
      tft.fillRect(position, 0, highlightWidth, gapPosition, PILLAR_HIGHLIGHT);
      tft.fillRect(position, gapPosition + gapHeight + 1, highlightWidth, SCREEN_HEIGHT - bottomBarHeight - (gapPosition + gapHeight + 1), PILLAR_HIGHLIGHT);
      
      // Pillar highlight
      tft.fillRect(position + highlightWidth, 0, width - highlightWidth, gapPosition, PILLAR);
      tft.fillRect(position + highlightWidth, gapPosition + gapHeight + 1, width - highlightWidth, SCREEN_HEIGHT - bottomBarHeight - (gapPosition + gapHeight + 1), PILLAR);
    }
  }
  
  void clear() {
    for (int i = 0; i < speed; i++) {
      tft.drawFastVLine(position + width + i, 0, SCREEN_HEIGHT - bottomBarHeight, BACKGROUND);
    }
  }
  
  void update() {
    position -= speed;
    
    // Spawn a new pillar when the current is out of sight
    if (position < -1 * width) {
      position = SCREEN_WIDTH;
      gapPosition = random(20, 120);
      //pillarNumber++;
      //
      //if (pillarNumber % speedIncrementEvery == 0) {
      //  speed += speedBoost;
      //}
    }
  }
} pillar;

struct Player {
  Vec2 position;
  int fallRate = 1;
  int boostRate = 8;
  int boosted = false;
  int width = 32;
  int height = 29;
  
  int animationFrame = 0;  
  const int animationRefreshRate = 500;
  unsigned long nextAnimationFrameTime;
  
  void init() {
    position = Vec2(50, SCREEN_HEIGHT / 2);
    fallRate = 1;
    animationFrame = 0;
    nextAnimationFrameTime = millis();
  }

  void draw() {  
    if (animationFrame == 0 && !boosted) {
      tft.drawRGBBitmap(position.x, position.y, ariel1, ariel1Mask, width, height);
    } else if (animationFrame == 1 || boosted) {
      tft.drawRGBBitmap(position.x, position.y, ariel2, ariel2Mask, width, height);
      boosted = false;
    }
    
    if (millis() > nextAnimationFrameTime) {
      animationFrame++;
      if (animationFrame == 2) {
        animationFrame = 0;
      }
      
      nextAnimationFrameTime += animationRefreshRate;
    }
  }

  void clear() {
    tft.fillRect(position.x, position.y, width, height, BACKGROUND);
  }
  
  void boost() {
    fallRate = -1 * boostRate;
    boosted = true;
  }
  
  void update() {
    position.y += fallRate;
    fallRate++;
  }
} player;

struct GameState {
  boolean screenPressed = false;
  boolean started = false;
  boolean crashed = false;
  boolean passedPillar = false;
  int score = 0;
  int highScore = 0;
  
  void init() {
    screenPressed = false;
    started = false;
    crashed = false;
    passedPillar = false;
    score = 0;
    
    highScore = EEPROM.read(0);
    initialDraw();
  }
  
  void clear() {
    char buf[50];
    sprintf(buf, "Score: %d", score);
    
    int16_t x, y;
    uint16_t w, h;
    tft.setTextSize(1);
    tft.getTextBounds(buf, 3, SCREEN_HEIGHT - bottomBarHeight + 3, &x, &y, &w, &h);
    
    tft.fillRect(x, y, w, h, GRAY);
  }
  
  void initialDraw() {
    tft.fillRect(0, SCREEN_HEIGHT - bottomBarHeight, SCREEN_WIDTH, bottomBarHeight, GRAY);
    
    draw();
  }
  
  void draw() {
    char buf[50];
    sprintf(buf, "Score: %d", score);
    
    tft.setTextColor(BLACK);
    tft.setCursor(3, SCREEN_HEIGHT - bottomBarHeight + 3);
    tft.setTextSize(1);
    tft.print(buf);
  }
  
  void update(Player player, Pillar pillar) {
    if (player.position.x > pillar.position + pillar.width && passedPillar) {
      passedPillar = false;
      score++;
      clear();
      draw();
    }
  }
} gameState;

void readResistiveTouch(void) {
  touchPoint = touchScreen.getPoint();
  pinMode(YP, OUTPUT);      //restore shared pins
  pinMode(XM, OUTPUT);
  digitalWrite(YP, HIGH);  //because TFT control pins
  digitalWrite(XM, HIGH);
}

bool ISPRESSED(void) {
  return touchPoint.z > MIN_PRESSURE && touchPoint.z < MAX_PRESSURE;
}

void centerprint(const char *s, int y, uint16_t textSize) {
    int len = strlen(s) * 6 * textSize;
    tft.setTextSize(textSize);
    tft.setTextColor(WHITE);
    tft.setCursor((SCREEN_WIDTH - len) / 2, y);
    tft.print(s);
}

void logos() {
  tft.fillScreen(BLACK);
  
  tft.drawBitmap((SCREEN_WIDTH - 64) / 2, (SCREEN_HEIGHT - 32) / 2, taraLogo1, 64, 32, WHITE);
  delay(500);
  tft.drawBitmap((SCREEN_WIDTH - 64) / 2, (SCREEN_HEIGHT - 32) / 2, taraLogo2, 64, 32, WHITE);
  delay(500);
  tft.drawBitmap((SCREEN_WIDTH - 64) / 2, (SCREEN_HEIGHT - 32) / 2, taraLogo3, 64, 32, WHITE);
  delay(500);
  tft.drawBitmap((SCREEN_WIDTH - 64) / 2, (SCREEN_HEIGHT - 32) / 2, taraLogo4, 64, 32, WHITE);
  
  delay(5000);
}

void splashScreen() {
  tft.fillScreen(BACKGROUND);

  resetGame();
  
  centerprint("TAP TO START", SCREEN_HEIGHT / 2 - 6, 2);

  nextRefreshTime = millis() + REFRESH_RATE;
}

void resetGame() {
  player.init();
  pillar.init();
  gameState.init();
}

void gameOver() {
  tft.fillScreen(BACKGROUND);
  
  char defeatTexts[7][60] = {
    "You're not getting cold fins now, are you?",
    "I... I'm not sure we should be here, Ariel.",
    "Oh don't be a guppy.",
    "Jeez, mon, I'm surrounded by amateurs.",
    "Careless and reckless behavior!",
    "Yes, hurry home, princess.",
    "Ariel, where are you going?"
  };
  
  Serial.println(gameState.score);
  Serial.println(gameState.highScore);
  if (gameState.score > gameState.highScore) {
    centerprint("NEW HIGHSCORE!", SCREEN_HEIGHT / 2 - 12, 2);
    EEPROM.write(0, gameState.highScore);
  } else {
    centerprint("GAME OVER", SCREEN_HEIGHT / 2 - 12, 2);
    centerprint(defeatTexts[random(0, 7)], SCREEN_HEIGHT / 2 + 18, 1); 
  }
  
  char buf[50];
  sprintf(buf, "Score: %d", gameState.score);
  centerprint(buf, SCREEN_HEIGHT / 2 + 6, 1);
}

void drawGame() {
  if (gameState.started) {
    player.clear();
    pillar.clear();
    
    player.update();
    pillar.update();
    
    player.draw();
    pillar.draw();
    
    gameState.update(player, pillar);
  }
}

void checkCollision() {
  // Collision detection with the top and bottom of the screen
  if (player.position.y + player.height >= SCREEN_HEIGHT - bottomBarHeight || player.position.y <= 0) {
    gameState.crashed = true;
  }
  
  // Collision detection with the pillar
  if (player.position.x + player.width > pillar.position && player.position.x < pillar.position + pillar.width) {
    if (player.position.y < pillar.gapPosition || player.position.y + player.height > pillar.gapPosition + pillar.gapHeight) {
      gameState.crashed = true;
    } else {
      gameState.passedPillar = true;
    }
  }
  
  // Show the game over screen
  if (gameState.crashed) {
    gameOver();

    gameState.started = false;
    
    delay(1200);
  }
}

void setup()
{
  Serial.begin(9600);
  randomSeed(analogRead(0));
  
  uint16_t ID = tft.readID();
  tft.begin(ID);
  tft.setRotation(SCREEN_ORIENTATION);
  
  logos();
  splashScreen();
}

void loop()
{
  unsigned long now = millis();
  
  if (millis() > nextRefreshTime && !gameState.crashed) {
    drawGame();
    checkCollision();
    nextRefreshTime += REFRESH_RATE;
  }
  
  readResistiveTouch();
  
  if (ISPRESSED() && !gameState.screenPressed) {
    if (gameState.crashed) {
      splashScreen();
    } else if (!gameState.started) {
      tft.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - bottomBarHeight, BACKGROUND);
      gameState.started = true;
    } else {
      player.boost();
      gameState.screenPressed = true;
    }
  } else if (touchPoint.z == 0 && gameState.screenPressed) {
    gameState.screenPressed = false;
  }
  
}