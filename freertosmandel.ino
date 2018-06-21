// This Arduino/FreeRTOS threaded Mandelbrot was
// implemented by Folkert van Heusden. It is released
// under AGPL v3.0.
// reactions can be send to: mail@vanheusden.com

// Yes, it can be improved. I did some things in a
// slightly odd way as I wanted to try some of the
// FreeRTOS functions.

#ifdef ESP32
#include <freertos/event_groups.h>
#define N_THREADS 6
#else
#include <Arduino_FreeRTOS.h>
#include <event_groups.h>
#include <semphr.h>
#define N_THREADS 4
#endif

constexpr double startX = -0.7453, startY = 0.1127;
double dim = 2;
double x1, x2, Y1, y2;
constexpr uint8_t mit = 255, xres = 80, yres = 24;

SemaphoreHandle_t xSemaphore = NULL;
EventGroupHandle_t xEventGroupStart = NULL, xEventGroupFinished = NULL;

void lock() {
  while (xSemaphoreTake(xSemaphore, (TickType_t )127) != pdTRUE) {
  }
}

void unlock() {
  xSemaphoreGive(xSemaphore);
}

void putPixel(const uint8_t c, const uint8_t x, const uint8_t y, const float wx, const float wy) {
  lock();
  Serial.print(F("\033["));
  Serial.print(y + 1);
  Serial.print(';');
  Serial.print(x);
  Serial.print('H');

#if 0 // messy
  uint8_t iwx = (int(wx) % 8) + 30;
  uint8_t iwy = (int(wy) % 8) + 40;
  Serial.print(F("\033["));
  Serial.print(iwx);
  Serial.print(';');
  Serial.print(iwy);
  Serial.print('m');
#endif

  Serial.print(char(32 + (c % 95)));
  Serial.flush();
  unlock();
}

typedef struct
{
  uint8_t yStart, nLines, bitNr;
} pars_t;

void drawThread(void *pars) {
  const pars_t *const p = (const pars_t *)pars;

  for (;;) {
    if (!xEventGroupWaitBits(xEventGroupStart, 1 << p -> bitNr, pdFALSE, pdTRUE, 100))
      continue;

    xEventGroupClearBits(xEventGroupStart, 1 << p -> bitNr);

    for (uint8_t y = p -> yStart; y < p -> yStart + p -> nLines; y++) {
      float yc = (y2 - Y1) / yres * y + Y1;

      for (uint8_t x = 0; x < xres; x++) {
        float xc = (x2 - x1) / xres * x + x1;

        float wx = 0, wy = 0;
        uint8_t it = 0;

        do {
          double temp = wx * wx - wy * wy + xc;
          wy = 2 * wx * wy + yc;
          wx = temp;
          it++;
        }
        while (wx * wx + wy * wy < 4.0 && it < mit);

        it %= 64;
        it += 32;

        putPixel(it, x, y, wx, wy);
      }
    }

    xEventGroupSetBits(xEventGroupFinished, 1 << p -> bitNr);
  }

  vTaskDelete(NULL);
}

pars_t pars[N_THREADS];
EventBits_t eventWaitBits = 0;

void setupMandelbrot() {
  x1 = startX - dim;
  x2 = startX + dim;
  Y1 = startY - dim;
  y2 = startY + dim;
  dim = (dim * 2.0 / 3.0);
}

void header() {
  static bool inv = false;
  if (inv)
    Serial.print(F("\033[7m"));
  inv = !inv;
  Serial.print(F("\033[24;1HFreeRTOS threaded Mandelbrot Zoom, (C) 2018 folkert@vanheusden.com"));
  Serial.print(F("\033[0m"));
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("Init"));

  xSemaphore = xSemaphoreCreateMutex();
  if (!xSemaphore) {
    Serial.println(F("No semaphore!!!"));
    for (;;) delay(1000);
  }

  xEventGroupStart = xEventGroupCreate();
  xEventGroupFinished = xEventGroupCreate();
  if (!xEventGroupStart || !xEventGroupFinished) {
    Serial.println(F("No eventgroups!!!"));
    for (;;) delay(1000);
  }

  const uint8_t nLines = 24 / N_THREADS;

  for (uint8_t i = 0; i < N_THREADS; i++) {
    pars[i].yStart = i * nLines;
    pars[i].nLines = nLines;
    pars[i].bitNr = i;

    eventWaitBits |= 1 << i;

#ifdef ESP32
    xTaskCreate(drawThread, "", 1024, &pars[i], 1, NULL);
#else
    xTaskCreate(drawThread, "", 200, &pars[i], 1, NULL);
#endif
  }

  setupMandelbrot();

  Serial.println(F("Go!"));
  header();

  xEventGroupSetBits(xEventGroupStart, eventWaitBits);
}

void loop() {
  if (xEventGroupWaitBits(xEventGroupFinished, eventWaitBits, pdFALSE, pdTRUE, 100)) {
    xEventGroupClearBits(xEventGroupFinished, eventWaitBits);
    setupMandelbrot();

    for(int i=0; i<9; i++) {
      header();
      delay(100);
    }

    Serial.println(F("\033[2J"));
    header();
    
    xEventGroupSetBits(xEventGroupStart, eventWaitBits);
  }
}
