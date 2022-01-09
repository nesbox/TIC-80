#include "tic80.h"
#include <stdbool.h>

typedef struct {
  int x;
  int y;
} TICGuy;

uint16_t t = 0;

TICGuy mascot = {.x = 96, .y = 24};

void TIC(){
  
  if (btn(0)) {
    mascot.y -= 1;
  } 
  if (btn(1)) {
      mascot.y +=1;
  } 
  if (btn(2)) {
    mascot.x -= 1;
  } 
  if (btn(3)) {
    mascot.x += 1;
  } 
  
  cls(13);
  uint8_t trans_color[] = {14};
  spr(1+t%60/30*2, mascot.x, mascot.y, trans_color, 1, 3, 0, 0, 2, 2);
  print("HELLO from C!", 84, 84, 15, true, 1, false);
  
  t++;
}


