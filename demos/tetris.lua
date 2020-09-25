-- title:  tetris
-- author: AlKau http://alkau.itch.io
-- desc:   classic Tetris game
-- script: lua
-- input:  gamepad

local CELL_H=8
local CELL_W=8

local isFirstTic=true
local isSfxOn=true
local state=0

local MENU_SCR="Menu"
local GAME_SCR="Game"
local PAUSE_SCR="Pause"
local OVER_SCR="GameOver"

local MENU_SFX="MenuSfx"
local SEL_MENU_SFX="SelectionMenuSfx"
local GAME_OVER_SFX="GameOverSfx"
local FELL_SFX="FellSfx"
local LINE_SFX="LineSfx"

local EMPTY_CELL_SPR=1
local DIE1_CELL_SPR=17
local DIE2_CELL_SPR=16

local DARKBLUE_CELL_SPR=3
local GREEN_CELL_SPR=4
local YELLOW_CELL_SPR=5
local RED_CELL_SPR=6
local BLUE_CELL_SPR=7
local BROWN_CELL_SPR=8
local LIGHTBLUE_CELL_SPR=9

local bestScore=0
local playerScore=0
local screenMng
local gameScreen
local soundMng

local saveSfxOnIdx=0
local saveScoreIdx=1

-- Helpers --
function PrintToCell(text,cx,cy,color)
 print(text,CELL_W*cx,CELL_H*cy+2,color)
end

function DrawToCell(sprId,cx,cy)
 spr(sprId,CELL_W*cx,CELL_H*cy)
end

-- temporary placeholder
function sfx()
end

local function UpdateBestScore(score)
 if bestScore<score then
	 bestScore=score
	end
 pmem(saveScoreIdx,bestScore)
end

local function LoadBestScore()
 bestScore = pmem(saveScoreIdx)
end

-- Game Grid
local function GameGrid(width,height)
 local s={
	 grid=nil,
		width=width or 10,
		height=height or 16,
		animDelay=100,
		animTime=0,
		isAnim=false
	}
	
	function s.Create()
	 s.grid={}
	 for y=1,s.height do
		 s.grid[y]={}
		 for x=1,s.width do
			 s.grid[y][x]=EMPTY_CELL_SPR
			end
		end
	end
	
	function s.Draw(cx,cy)
	 for y=1,s.height do
 	 for x=1,s.width do
			 DrawToCell(s.grid[y][x],cx+x-1,cy+y-1)
			end
		end
	end
	
	function s.Add(x,y,sprId)
	 s.grid[y][x]=sprId
	end
	
	function s.UpdateLine()
	 local res=0
	 for y=1,s.height do
		 local isLine = true
 	 for x=1,s.width do
    isLine = isLine and not (s.grid[y][x] == EMPTY_CELL_SPR)
				if not isLine then break end
			end
			
			if isLine then
			 res=res+1
			 s.isAnim = true
				s.animTime=time()
			 for x=1,s.width do
     s.grid[y][x] = DIE1_CELL_SPR
				end
			end
			
		end
		return res
	end
	
	local function replace(oldSprId, newSprId)
	 local res = false
		for y=1,s.height do
 	 for x=1,s.width do
			 if s.grid[y][x] == oldSprId then
				 s.grid[y][x]=newSprId
					res = true
				end
			end
		end
		
		return res
	end
	
	local function removeLine(gridLine)
		table.remove(s.grid,gridLine)
		local newLine={}
		for x=1,s.width do
			newLine[x]=EMPTY_CELL_SPR
		end
		table.insert(s.grid,1,newLine)
	end
	
	local function remove(oldSprId)
 	local process=true
  while process do
		 process=false	
			for y=1,s.height do
				 if s.grid[y][1] == oldSprId then
					 removeLine(y)
						process=true
						break
					end 
				end
		end
	end
	
	function s.AnimationUpdate()
	 if not s.isAnim then return end
		local now = time()
		if now > s.animTime + s.animDelay then
		 s.animTime=now
		 remove(DIE2_CELL_SPR)
			s.isAnim = replace(DIE1_CELL_SPR, DIE2_CELL_SPR)
		end	
	end
	
	return s
end

-- Tetrimino class
local function Tetrimino(sprId)
 local s={
	 sprId=sprId,
		width=0,
		height=0,
		pos={x=0,y=0},
		shape=0,
	 shapes={},
		wallkickGap=0,
		isFell=false
	}
	
	function s.Move(x,y)
	 s.pos.x=x
		s.pos.y=y
	end
	
	local function isCollision(grid,shape,pos)
	 local sh=s.GetShape(shape)
		for y=1,s.height do
	  for x=1,s.width do
			 if sh~=nil and sh[y][x]>0 then
				 local posX=x+pos.x
					local posY=y+pos.y 
					
					if posX<=0 or posX>grid.width then
					 return true 
					elseif posY>grid.height then
					 return true 
					elseif not (grid.grid[posY][posX]==EMPTY_CELL_SPR) then
					 return true 
					end
				end
			end
		end
		
		return false
	end
	
	function s.Create()end
	
	function s.Draw(cx,cy)
	 local shape=s.GetShape(s.shape)
		for y=1,s.height do
	  for x=1,s.width do
			 if shape~=nil and shape[y][x]>0 then
				 local posX=cx+x-1+s.pos.x
					local posY=cy+y-1+s.pos.y 
				 DrawToCell(s.sprId,posX,posY)
				end
			end
		end
	end
	
	function s.GetShape(shape)
	 if shape>0 then
		 return s.shapes[shape]
		end
		return nil
	end
	
	function s.GoDown(grid)
  local p={x=s.pos.x,y=s.pos.y}
		p.y=p.y+1	
	 
		if not isCollision(grid,s.shape,p) then
		 s.pos=p
		else
		 s.isFell=true
		end
	end
	
	function s.GoLeft(grid)
  local p={x=s.pos.x,y=s.pos.y}
		p.x=p.x-1	
	 
		if not isCollision(grid,s.shape,p) then
		 s.pos=p
		end
	end
	
	function s.GoRight(grid)
  local p={x=s.pos.x,y=s.pos.y}
		p.x=p.x+1
	 
		if not isCollision(grid,s.shape,p) then
		 s.pos=p
		end
	end
	
	local function doWallkick(grid,shape)
	 if shape==2 or shape==4 then return end
	
  for gap=1,s.wallkickGap do
			-- try right
	  local pRight={x=s.pos.x+gap,y=s.pos.y}
	  if not isCollision(grid,shape,pRight) then
		  s.pos = pRight
			 s.shape = shape
			 return
		 end
			
	 	-- try left
   local pLeft={x=s.pos.x-gap,y=s.pos.y}
	  if not isCollision(grid,shape,pLeft) then
			 s.pos = pLeft
			 s.shape = shape
			 return
		 end			
		end	

	end
	
	function s.DoRotation(clockwise,grid)
 	local shape=s.shape;
	 if clockwise then
		 shape=shape+1
		else
		 shape=shape-1
		end
		
		if shape>4 then
		 shape=1
		elseif shape<1 then
		 shape=4
		end

		if not isCollision(grid,shape,s.pos) then
		 s.shape=shape
		else
		 doWallkick(grid,shape)
		end
	end
	
	function s.CopyToGrid(grid)
	 local sh=s.GetShape(s.shape)
			for y=1,s.height do
				for x=1,s.width do
			 	if sh~=nil and sh[y][x]>0 then
						local posX=x+s.pos.x
						local posY=y+s.pos.y
			
     	grid.grid[posY][posX]=s.sprId
				end
			end
		end
	end
	
	function s.IsGameOver(grid)
	 return isCollision(grid,s.shape,s.pos)
	end
	
	return s
end

local function ZTetrimino(sprId)
 local s=Tetrimino(sprId)
	
	function s.Create()
		s.width=3
	 s.height=3
	 s.shape=1
		s.wallkickGap=1
	 s.shapes[1]={
			{1,1,0},
			{0,1,1},
			{0,0,0}
		}
		
	 s.shapes[2]={
		 {0,0,1},
			{0,1,1},
			{0,1,0}
		}

	 s.shapes[3]={
		 {0,0,0},
			{1,1,0},
			{0,1,1}
		}
		
	 s.shapes[4]={
		 {0,1,0},
			{1,1,0},
			{1,0,0}
		}				
	end
	
	return s
end

local function STetrimino(sprId)
 local s=Tetrimino(sprId)
	
	function s.Create()
		s.width=3
	 s.height=3
	 s.shape=1
		s.wallkickGap=1		
	 s.shapes[1]={
			{0,1,1},
			{1,1,0},
			{0,0,0}
		}
		
	 s.shapes[2]={
		 {0,1,0},
			{0,1,1},
			{0,0,1}
		}

	 s.shapes[3]={
		 {0,0,0},
			{0,1,1},
			{1,1,0}
		}
		
	 s.shapes[4]={
		 {1,0,0},
			{1,1,0},
			{0,1,0}
		}				
	end
	
	return s
end

local function LTetrimino(sprId)
 local s=Tetrimino(sprId)
	
	function s.Create()
		s.width=3
	 s.height=3
	 s.shape=1
		s.wallkickGap=1		
	 s.shapes[1]={
			{0,0,1},
			{1,1,1},
			{0,0,0}
		}
		
	 s.shapes[2]={
		 {0,1,0},
			{0,1,0},
			{0,1,1}
		}

	 s.shapes[3]={
		 {0,0,0},
			{1,1,1},
			{1,0,0}
		}
		
	 s.shapes[4]={
		 {1,1,0},
			{0,1,0},
			{0,1,0}
		}				
	end
	
	return s
end

local function JTetrimino(sprId)
 local s=Tetrimino(sprId)
	
	function s.Create()
		s.width=3
	 s.height=3
	 s.shape=1
		s.wallkickGap=1		
	 s.shapes[1]={
			{1,0,0},
			{1,1,1},
			{0,0,0}
		}
		
	 s.shapes[2]={
		 {0,1,1},
			{0,1,0},
			{0,1,0}
		}

	 s.shapes[3]={
		 {0,0,0},
			{1,1,1},
			{0,0,1}
		}
		
	 s.shapes[4]={
		 {0,1,0},
			{0,1,0},
			{1,1,0}
		}				
	end
	
	return s
end

local function TTetrimino(sprId)
 local s=Tetrimino(sprId)
	
	function s.Create()
		s.width=3
	 s.height=3
	 s.shape=1
		s.wallkickGap=1		
	 s.shapes[1]={
			{0,1,0},
			{1,1,1},
			{0,0,0}
		}
		
	 s.shapes[2]={
		 {0,1,0},
			{0,1,1},
			{0,1,0}
		}

	 s.shapes[3]={
		 {0,0,0},
			{1,1,1},
			{0,1,0}
		}
		
	 s.shapes[4]={
		 {0,1,0},
			{1,1,0},
			{0,1,0}
		}				
	end
	
	return s
end

local function ITetrimino(sprId)
 local s=Tetrimino(sprId)
	
	function s.Create()
		s.width=4
	 s.height=4
	 s.shape=1
		s.wallkickGap=2		
	 s.shapes[1]={
			{0,0,0,0},
			{1,1,1,1},
			{0,0,0,0},
			{0,0,0,0}
		}
		
	 s.shapes[2]={
			{0,0,1,0},
			{0,0,1,0},
			{0,0,1,0},
			{0,0,1,0}
		}

	 s.shapes[3]={
			{0,0,0,0},
			{0,0,0,0},
			{1,1,1,1},
			{0,0,0,0}
		}
		
	 s.shapes[4]={
			{0,1,0,0},
			{0,1,0,0},
			{0,1,0,0},
			{0,1,0,0}
		}				
	end
	
	return s
end

local function OTetrimino(sprId)
 local s=Tetrimino(sprId)
	
	function s.Create()
		s.width=4
	 s.height=3
	 s.shape=1
		s.wallkickGap=0		
	 s.shapes[1]={
			{0,1,1,0},
			{0,1,1,0},
			{0,0,0,0}
		}
		
	 s.shapes[2]={
			{0,1,1,0},
			{0,1,1,0},
			{0,0,0,0}
		}

	 s.shapes[3]={
			{0,1,1,0},
			{0,1,1,0},
			{0,0,0,0}
		}
		
	 s.shapes[4]={
			{0,1,1,0},
			{0,1,1,0},
			{0,0,0,0}
		}				
	end
	
	return s
end

local function Spawner()
 local s={
 	items={}
	}
	
	function s.AddTetrimino(tetrimino,sprId)
	 table.insert(s.items,{tetrimino=tetrimino,sprId=sprId})
	end
	
	function s.Spawn()
	 local item = s.items[math.random(#s.items)]
		return item.tetrimino(item.sprId)
	end
	
	return s
end
-- UI --

-- Menu class --
local function Menu(gap,color)
 local s={
	 items={},
		gap = gap or 1,
		color = color or 15,
		idx=1
	}
	
	local function change(move)
	 local idx=s.idx
	
	 s.idx = s.idx + move
		if s.idx > #s.items then s.idx = #s.items end
		if s.idx < 1 then s.idx = 1 end
		
		if idx~=s.idx then
		 soundMng.PlaySfx(MENU_SFX)
		end
	end
	
	local	function action()
	 soundMng.PlaySfx(SEL_MENU_SFX)
	 s.items[s.idx].Action()
	end
	
	function s.Add(item)
	 table.insert(s.items,item)
	end
	
	function s.Draw(x,y)
	 for i,item in pairs(s.items) do
		 local offset = (i-1)*s.gap
		 item.isSel = i == s.idx
		 item.Draw(x,y+offset,s.color)
		end
	end
	
	function s.Update()
		if btnp(0) then
		 change(-1)
		elseif btnp(1) then
		 change(1)
		elseif btnp(4) or btnp(5) then
		 action()
		end
	end
	
	function s.Reset()
	 s.idx=1
	end

	return s
end

-- MenuItem class
local function MenuItem(name,action,selClr,sel)
	local s={
		sel=sel or ">",
		selClr = selClr or 15,
		name=name or "item",
		isSel=false,
		action=action
	}
	
	function s.Draw (x,y,color)
	 local clr = color
	 if s.isSel then
		 clr = s.selClr
		 PrintToCell(s.sel,x,y,clr)
		else
		
		end
	 PrintToCell(s.name,x+2,y,clr)
 end
	 
	function s.Action()
	 if s.action~=nil then s.action(s) end
	end
	
	return s
end

-- ToggleMenuItem class : MenuItem
local function ToggleMenuItem(name,isOn,action,sel,on,off)
 local s = MenuItem(name,action,sel)
	s.rawName = name
	s.isOn = isOn or false
	s.On = on or "On"
	s.Off = off or "Off"
	s.DrawBase = s.Draw
	s.ActionBase = s.Action
	
	function s.Action()
	 s.isOn = not s.isOn
		s.ActionBase()
	end
	
	function s.Draw(x,y,color)
		local toggle = s.Off
		if s.isOn then toggle = s.On	end
	 s.name = s.rawName.." "..toggle
		s.DrawBase(x,y,color)
 end
	
	return s
end

-- Screen --
local function ScreenManager()
 local s={
	 items={},
		current=nil
	}
	
	local function leave()
	 if s.current~=nil then s.current.OnLeave() end
	end
	
	local function enter()
	 if s.current~=nil then s.current.OnEnter() end
	end
	
	function s.Add(item)
		table.insert(s.items,item)
	end
	
	function s.GoTo(name)
	 for i,screen in pairs(s.items) do
		 if screen.name == name then
			 leave() 
			 s.current = screen
				enter() 
			end
		end
	end
	
	function s.Update()
	 s.current.Update()
	end
	
	function s.Draw()
	 s.current.Draw()
	end
	
	return s
end

local function Screen(name)
 local s={
	 name = name or "scr#1"
	}
	
	function s.Update()	end
	function s.Draw()	end
	function s.OnEnter() end
	function s.OnLeave() end
	
	return s
end

-- Menu Screen
local function MenuScreen()
 local s = Screen(MENU_SCR)
	s.menu = Menu(2)
	
	local function onNewGame()
 	gameScreen.Reset()
	 screenMng.GoTo(GAME_SCR) 
	end
	
	local function onSoundChanged(itemMenu)
	 soundMng.isOn=itemMenu.isOn
		soundMng.SaveSettings()
	end
	
	s.menu.Add(MenuItem("New Game",onNewGame,14))
	s.menu.Add(ToggleMenuItem("Sound",soundMng.isOn,onSoundChanged,14))
		
	function s.OnEnter()
		s.menu.Reset()
	end
	
	function s.Update()
	 s.menu.Update()
	end
	
	function s.Draw()
		map(30,0,30,17)
	 s.menu.Draw(10,10,15)
	end
	
	return s
end

-- Pause Screen
local function PauseScreen()
 local s=Screen(PAUSE_SCR)
	
	s.menu = Menu(2)
	
	local function onContinue()
	 screenMng.GoTo(GAME_SCR) 
	end
	
	local function onMenu()
	 screenMng.GoTo(MENU_SCR)
	end
	
	s.menu.Add(MenuItem("Continue",onContinue,14))
	s.menu.Add(MenuItem("Main Menu",onMenu,14))
 
	function s.OnEnter()
	 s.menu.Reset()
	end
	 
	function s.Update()
	 s.menu.Update()
	end
	
	function s.Draw()
		map(60,0,30,17)
	 s.menu.Draw(10,10,15)
	end
		
	return s
end

-- Game Screen
local function GameScreen()
 local s=Screen(GAME_SCR)
	s.grid=GameGrid()
	s.spawner=Spawner()
	s.current=nil
	s.next=nil
	s.level=1
	s.fallSpeed=550
	s.levelSpeed=45*1000
	s.fallTime=0
	s.levelTime=0
	s.pauseTime=0
	s.isGameOver=false
	
	local function onPause()
	 screenMng.GoTo(PAUSE_SCR) 
		s.pauseTime=time()
	end
	
	local function onGameOver()
		UpdateBestScore(playerScore)
	 screenMng.GoTo(OVER_SCR)
	end
	
	local function spawn()
	 if s.current==nil and not s.grid.isAnim then
	  s.current =	s.next
		 s.current.Move(3,0)
			s.fallTime=time()
			
	 	s.next =	s.spawner.Spawn()	
 		s.next.Create()
		end
	end
	
	local function levelUp()
	 if s.level < 10 then
		 s.level = s.level + 1
			s.fallSpeed = s.fallSpeed-50
		end
	end
	
	local function fall()
 	local now=time()
	 if s.current~=nil and s.fallTime+s.fallSpeed < now then
		 s.fallTime = now
			s.current.GoDown(s.grid)
		end
		
		if s.levelTime+s.levelSpeed < now then
		 s.levelTime = now
			levelUp()
		end
	end
	
	local function completion()
	 if s.current==nil then return end

  if s.current.IsGameOver(s.grid) then
		 s.isGameOver=true
		 return
		end
		
	 if s.current.isFell then
		 s.current.CopyToGrid(s.grid)
	  s.current = nil
			local res = s.grid.UpdateLine()
			
			local score=0
			
			if     res==1 then score=100
			elseif res==2 then score=300
			elseif res==3 then score=500
			elseif res==4 then score=800
			end
			
			if score==0 then
 			soundMng.PlaySfx(FELL_SFX)
			else
 			soundMng.PlaySfx(LINE_SFX)			 
			end
	
	  score=score+4*s.level
	
			playerScore = playerScore + s.level*score
		end
	end
	
	function s.Create()
 	s.spawner.AddTetrimino(ZTetrimino,RED_CELL_SPR)
 	s.spawner.AddTetrimino(STetrimino,GREEN_CELL_SPR)
 	s.spawner.AddTetrimino(LTetrimino,YELLOW_CELL_SPR)
 	s.spawner.AddTetrimino(JTetrimino,DARKBLUE_CELL_SPR)
 	s.spawner.AddTetrimino(TTetrimino,BLUE_CELL_SPR)
 	s.spawner.AddTetrimino(OTetrimino,BROWN_CELL_SPR)
 	s.spawner.AddTetrimino(ITetrimino,LIGHTBLUE_CELL_SPR)
	end
	
	function s.Reset()
	 s.grid.Create()

		s.next =	s.spawner.Spawn()
		s.next.Create()
		s.current=nil
		
		s.levelTime=time()
		s.fallTime=time()
		playerScore=0
		s.level=1
		s.fallSpeed=550
		s.isGameOver=false
	end
	
	function s.OnEnter()
	 spawn()
		s.fallTime=time()
		
		if s.pauseTime>0 then
		 local passTime=s.levelTime-s.pauseTime
			s.levelTime=time()+passTime
		end
		
		s.pauseTime=0
	end
	
	function s.Update()
		completion()
		spawn()
		
		if s.isGameOver then
		 onGameOver()
		 return
		end
	
		if btnp(5) then onPause() end
  
		if s.current ~= nil then
 		if btnp(4,10,10) or btnp(0,10,10) then s.current.DoRotation(true,s.grid) end
 		if btnp(1,5,5) then s.current.GoDown(s.grid) end
 		if btnp(2,5,5) then s.current.GoLeft(s.grid) end
 		if btnp(3,5,5) then s.current.GoRight(s.grid) end
		end
		
		s.grid.AnimationUpdate()
		fall()
	end
	
	function s.Draw()
	
	--Background
 	map(0,0,30,17)
	
	--Score
  local score = string.format("%07d", playerScore)
 	PrintToCell("Score: ",1,1,6)
 	PrintToCell(score,1,2,15)

	--BestScore
	 local best = string.format("%07d", bestScore)
 	PrintToCell("Best: ",1,4,6)
 	PrintToCell(best,1,5,15)
		
	--Level	
	 local level = string.format("%02d", s.level)
 	PrintToCell("Level: ",1,13,6)
 	PrintToCell(level,2,14,15)	
	
	 s.grid.Draw(9,0)
		s.next.Draw(23,2)		
		if s.current~=nil then
		 s.current.Draw(9,0)
		end
	end
		
	return s
end

-- Game Over Screen
local function GameOverScreen()
 local s=Screen(OVER_SCR)
	local start
	local delay=5000
	
	local function onMenu()
	 screenMng.GoTo(MENU_SCR)
	end
	
	function s.OnEnter()
		start=time()
		soundMng.PlaySfx(GAME_OVER_SFX)
	end
	
	function s.OnLeave()
		soundMng.BreakSfx(GAME_OVER_SFX)
	end	
	 
	function s.Update()
		local now=time()
		local isTime=(now>start+delay)
		local isBtn=btnp(4) or btnp(5)
		if isTime or isBtn  then 
			onMenu()
		end
	end
	
	function s.Draw()
		map(90,0,30,17)
		
			--Score
  local score = string.format("%07d", playerScore)
 	PrintToCell("Score: ",4,11,6)
 	PrintToCell(score,4,12,15)

	--BestScore
	 local best = string.format("%07d", bestScore)
 	PrintToCell("Best: ",22,4,6)
 	PrintToCell(best,22,5,15)
	end
		
	return s
end

-- SoundManager
local function SoundManager()
 local s={
		sounds={},
		isOn=true,
		volume=15
	}
	
	function s.LoadSettings()
		local isOn=pmem(saveSfxOnIdx)
		
	 if isOn<2 then
	  s.isOn=true
	 else
	  s.isOn=false
	 end
	end
	
	function s.SaveSettings()
	 local isOn=0
	
	 if s.isOn then
	  isOn=1 --On
	 else
	  isOn=2 --Off
	 end
	
  pmem(saveSfxOnIdx,isOn)
	end
	
	local function findByName(name)
	 for i,sound in pairs(s.sounds) do
		 if sound.name == name then
			 return sound
			end
		end
		
		return nil
	end
	
	local function foreach(condition,action)
	 for i,sound in pairs(s.sounds) do
		 if condition(sound) then
			 action(sound)
			end
		end
	end
	
	function s.Add(sound)
	 table.insert(s.sounds,sound)
 end
	
	function s.PlaySfx(name)
  local sound = findByName(name)
		if sound==nil then return end
		
		foreach(function(item)
           return item.channel==sound.channel
			       end,
										function(item)
										 item.Break()
										end)
										
		sound.StartPlay()
	end
	
	function s.BreakSfx(name)
  local sound = findByName(name)
		if sound==nil then return end
		
		sound.Break()
	end
	
	function s.PlayProcess()
	 local vol = 0
		
		if s.isOn then
		 vol = s.volume
		end
		
	 foreach(function(sound)
           return sound.isFinished==false
	         end,
										function(sound)
											sound.PlayProcess(vol)
										end)
	end
	
	return s
end

-- SoundEffect
local function SoundEffect(name)
 local s={
	 name=name or "sound#1",
	 currentTic=0,
		isFinished=true,
		channel=0
	}
	
	function s.Test() end
	
	function s.PlayNote(volume) end
	
	function s.StartPlay()
	 s.currentTic=0
		s.isFinished=false
		sfx(-1,0,s.channel)
	end
	
	function s.PlayProcess(volume)
	 if s.isFinished then return end
		
	 if s.currentTic>0 then
		 s.PlayNote(volume)
		end

		s.currentTic=s.currentTic+1
	end
	
	function s.Break()
	 s.currentTic=0
		s.isFinished=true
		sfx(-1,0,s.channel)
	end
	
	return s
end

-- Fell sound
function FellSfx()
 local s=SoundEffect(FELL_SFX)
	s.channel=2
	
	function s.PlayNote(volume)
	 if s.currentTic==1 then
 	 sfx(0,12,s.channel,volume,3)
		end
	end
	
	return s
end

function LineSfx()
 local s=SoundEffect(LINE_SFX)
	s.channel=2
	
	function s.PlayNote(volume)
	 if s.currentTic==1 then
 	 sfx(2,23,s.channel,volume,2)
		end
	end
	
	return s
end

function GameOverSfx()
 local s=SoundEffect(GAME_OVER_SFX)
	s.channel=3
	
	function s.PlayNote(volume)
	 if s.currentTic==1 then
 	 sfx(3,55,s.channel,volume,-2)
		end
	end
	
	return s
end

function MenuSfx()
 local s=SoundEffect(MENU_SFX)
	s.channel=0
	
	function s.PlayNote(volume)
	 if s.currentTic==1 then
 	 sfx(1,4,s.channel,volume,3)
		end
	end
	
	return s
end

function SelMenuSfx()
 local s=SoundEffect(SEL_MENU_SFX)
	s.channel=3
	
	function s.PlayNote(volume)
	 if s.currentTic==1 then
 	 sfx(1,4,s.channel,volume,3)
		end
	end
	
	return s
end

local function Init()
 if not isFirstTic then return end
	isFirstTic=false
	
	LoadBestScore()
	
	soundMng = SoundManager()
	soundMng.LoadSettings()
	soundMng.Add(FellSfx())
	soundMng.Add(LineSfx())
	soundMng.Add(GameOverSfx())	
	soundMng.Add(MenuSfx())
	soundMng.Add(SelMenuSfx())
	
	screenMng = ScreenManager()
	screenMng.Add(MenuScreen())
	screenMng.Add(PauseScreen())
	gameScreen = GameScreen()
	gameScreen.Create()
	screenMng.Add(gameScreen)
	screenMng.Add(GameOverScreen())
	screenMng.GoTo(MENU_SCR)
	
end

local function Update()
 screenMng.Update()
	soundMng.PlayProcess()
end

local function Draw()
 screenMng.Draw()
end

function TIC()
	Init()
	Update()
	Draw()
end

-- <TILES>
-- 001:0000000101111113011111130111111301111113011111130111111313333333
-- 002:fffffffafaaaaaa0faaaaaa0faaaaaa0faaaaaa0faaaaaa0faaaaaa0a0000000
-- 003:8888888282222220822222208222222082222220822222208222222020000000
-- 004:bbbbbbb5b5555550b5555550b5555550b5555550b5555550b555555050000000
-- 005:eeeeeee9e9999990e9999990e9999990e9999990e9999990e999999090000000
-- 006:ccccccc6c6666660c6666660c6666660c6666660c6666660c666666060000000
-- 007:ddddddd8d8888880d8888880d8888880d8888880d8888880d888888080000000
-- 008:fffffffefeeeeee0feeeeee0feeeeee0feeeeee0feeeeee0feeeeee0e0000000
-- 009:fffffffdfdddddd0fdddddd0fdddddd0fdddddd0fdddddd0fdddddd0d0000000
-- 016:0000000101111113011111130111111301111113011111130111111313333333
-- 017:fffffffcfcccccc0fcccccc0fcccccc0fcccccc0fcccccc0fcccccc0c0000000
-- 018:0000000a0aaaaaaf0aaaaaaf0aaaaaaf0aaaaaaf0aaaaaaf0aaaaaafafffffff
-- 019:0000000202222228022222280222222802222228022222280222222828888888
-- 020:000000050555555b0555555b0555555b0555555b0555555b0555555b5bbbbbbb
-- 021:000000090999999e0999999e0999999e0999999e0999999e0999999e9eeeeeee
-- 022:000000060666666c0666666c0666666c0666666c0666666c0666666c6ccccccc
-- 023:000000080888888d0888888d0888888d0888888d0888888d0888888d8ddddddd
-- 024:0000000404444449044444490444444904444449044444490444444949999999
-- 025:0000000d0ddddddf0ddddddf0ddddddf0ddddddf0ddddddf0ddddddfdfffffff
-- 033:9999999494444440944444409444444094444440944444409444444040000000
-- </TILES>

-- <MAP>
-- 000:000000000000000020101010101010101010102000202020202020202000202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 001:000000000000000020101010101010101010102000201010101010102000201010101010101010101010101010101010101010101010101010101020201010101010101010101010101010101010101010101010101010101020201010101010101010101010101010101010101010101010101010101020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 002:000000000000000020101010101010101010102000201010101010102000201010103030301040404010505050106060601070707010505050101020201010101030303010105050106010601070707010404040106010101020201010101010101010101010101010101010101010101010101010101020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 003:000000000000000020101010101010101010102000201010101010102000201010101030101040101010105010106010601010701010501010101020201010101030103010501050106010601070101010401010106010101020201010303030101040401050501050101060606010000000000000001020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 004:000000000000000020101010101010101010102000201010101010102000201010101030101040404010105010106060101010701010505050101020201010101030303010501050106010601070707010404040106010101020201010301010104010401050105010501060101010000000000000001020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 005:000000000000000020101010101010101010102000202020202020202000201010101030101040101010105010106010601010701010101050101020201010101030101010505050106010601010107010401010101010101020201010301010104010401050105010501060606010000000000000001020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 006:000000000000000020101010101010101010102000000000000000000000201010101030101040404010105010106010601070707010505050101020201010101030101010501050106060601070707010404040106010101020201010301030104040401050105010501060101010000000000000001020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 007:000000000000000020101010101010101010102000000000000000000000201010101010101010101010101010101010101010101010101010101020201010101010101010101010101010101010101010101010101010101020201010303030104010401050105010501060606010101010101010101020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 008:000000000000000020101010101010101010102000000000000000000000201010101010101010101010101010101010101010101010101010101020201010101010101010101010101010101010101010101010101010101020201010101010101010101010101010101010101010101010101010101020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 009:000000000000000020101010101010101010102000000000000000000000201010101010100000000000000000000000000000000010101010101020201010101010100000000000000000000000000000000010101010101020201010101010101010101010606060103010301040404010707070101020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 010:000000000000000020101010101010101010102000000000000000000000201010101010100000000000000000000000000000000010101010101020201010101010100000000000000000000000000000000010101010101020201010000000000000001010601060103010301040101010701070101020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 011:000000000000000020101010101010101010102000000000000000000000201010101010100000000000000000000000000000000010101010101020201010101010100000000000000000000000000000000010101010101020201010000000000000001010601060103010301040404010707010101020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 012:000000000000000020101010101010101010102000000000000000000000201010101010100000000000000000000000000000000010101010101020201010101010100000000000000000000000000000000010101010101020201010000000000000001010601060103010301040101010701070101020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 013:000000000000000020101010101010101010102000000000000000000000201010101010100000000000000000000000000000000010101010101020201010101010100000000000000000000000000000000010101010101020201010000000000000001010606060101030101040404010701070101020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 014:000000000000000020101010101010101010102000000000000000000000201010101010101010101010101010101010101010101010101010101020201010101010101010101010101010101010101010101010101010101020201010101010101010101010101010101010101010101010101010101020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 015:000000000000000020101010101010101010102000000000000000000000201010101010101010101010101010101010101010101010101010101020201010101010101010101010101010101010101010101010101010101020201010101010101010101010101010101010101010101010101010101020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 016:000000000000000020202020202020202020202000000000000000000000202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020202020000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- </MAP>

-- <PALETTE>
-- 000:140c1c44243430346d4e4a4e854c30346524d04648757161597dced27d2c8595a16daa2cd2aa996dc2cadad45edeeed6
-- </PALETTE>

