# title: Bunnymark in Python
# author: Kolten Pearson
# desc: Benchmarking tool to see how many bunnies can fly around the screen, using Python
# input: gamepad
# script: python
# version: 0.0.0
import random 

screen_width = 240
screen_height = 136
toolbar_height = 6
t = 0


class Bunny :

  def __init__(self) :
    self.width = 26
    self.height = 32
    self.x = random.randint(0, screen_width - self.width)
    self.y = random.randint(0, screen_height - self.height)
    self.speed_x = random.uniform(-100.0, 100.0) / 60.0
    self.speed_y = random.uniform(-100.0, 100.0) / 60.0
    self.sprite = 1
    
  def draw(self) :
    spr(self.sprite, int(self.x), int(self.y), 1, 1, 0, 0, 4, 4)
    
  def update(self) :
    self.x = self.x + self.speed_x
    self.y = self.y + self.speed_y
    
    if (self.x + self.width > screen_width) :
      self.x = screen_width - self.width
      self.speed_x = self.speed_x * -1.0
    
    if (self.y + self.height > screen_height) :
      self.y = screen_height - self.height
      self.speed_y = self.speed_y * -1.0
      
    if (self.x < 0) :
      self.x = 0
      self.speed_x = self.speed_x * -1.0
    
    if (self.y < toolbar_height) :
      self.y = toolbar_height
      self.speed_y = self.speed_y * -1.0
          
class FPS :
  
  def __init__(self) :
    self.value = 0
    self.frames = 0
    self.last_time = 0
    
  def get_value(self) :
    if (time() - self.last_time <= 1000) :
      self.frames = self.frames + 1
    else :
      self.value = self.frames
      self.frames = 0
      self.last_time = time()
      
    return self.value
    
fps = FPS()

bunnies = [Bunny()]

def TIC() :
  global t
  
  if t == 0 :
    music(0)
  
  if t == 6 * 64 * 2.375 :
    music(1)
    
  t += 1
  
  if btn(0) :
    for i in range(5) :
      bunnies.append(Bunny())
  
  if btn(1) :
    for i in range(5) :
      if len(bunnies) > 0 :
        bunnies.pop()
  
  for bunny in bunnies :
    bunny.update()
    
  cls(15)
  for bunny in bunnies :
    bunny.draw()
    
  rect(0, 0, screen_width, toolbar_height, 0)
  print("Bunnies: " + str(len(bunnies)), 1, 0, 11, False, 1, False)
  print("FPS: " + str(fps.get_value()), screen_width//2, 0, 11, False, 1, False)
  
# <TILES>
# 001:11111100111110dd111110dc111110dc111110dc111110dc111110dd111110dd
# 002:00011110ddd0110dccd0110dccd0110dccd0110dccd0110dcddd00dddddddddd
# 003:00001111dddd0111cccd0111cccd0111cccd0111cccd0111dcdd0111dddd0111
# 004:1111111111111111111111111111111111111111111111111111111111111111
# 017:111110dd111110dd111110dd111110dd10000ddd1eeeeddd1eeeeedd10000eed
# 018:d0ddddddd0ddddddddddddddddd0000dddddccddddddccdddddddddddddddddd
# 019:0ddd01110ddd0111dddd0111dddd0111ddddd000ddddddddddddddddddddd000
# 020:1111111111111111111111111111111101111111d0111111d011111101111111
# 033:111110ee111110ee111110ee111110ee111110ee111110ee111110ee111110ee
# 034:dddcccccddccccccddccccccddccccccddccccccdddcccccdddddddddddddddd
# 035:dddd0111cddd0111cddd0111cddd0111cddd0111dddd0111dddd0111dddd0111
# 036:1111111111111111111111111111111111111111111111111111111111111111
# 049:111110ee111110ee111110ee111110ee111110ee111110ee111110ee11111100
# 050:dddeeeeeddeeeeeed00000000111111101111111011111110111111111111111
# 051:eddd0111eedd01110eed011110ee011110ee011110ee011110ee011111001111
# 052:1111111111111111111111111111111111111111111111111111111111111111
# </TILES>

# <PALETTE>
# 000:1a1c2c5d275db13e53ef7d57ffcd75a7f07038b76425717929366f3b5dc941a6f673eff7f4f4f494b0c2566c86333c57
# </PALETTE>

