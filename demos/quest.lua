-- title:  QUEST FOR GLORY
-- author: Deck http://deck.itch.io
-- desc:   roguelike game
-- script: lua
-- input:  gamepad

-- constants
local CAM_W=240
local CAM_H=136
local MAP_W=1920
local MAP_H=1088
local CELL=8

local tiles={
	KEY=176,
	HEART=177,
	POTION=178,
	SPIKES_HARMLESS=192,
	SPIKES_HARM=193,
	CLOSED_COFFER=194,
	OPENED_COFFER=195,
	CLOSED_DOOR=196,
	OPENED_DOOR=197,
	CLOSED_GRATE=198,
	OPENED_GRATE=199,
	SKULL=208,
	DESK=209,
	STATUE_1=210,
	COLUMN=211,
	CRACK=212,
	MIMIC_HARMLESS=213,
	MIMIC_HARM=214,
	KEY_FLOOR=215,
	ALTAR=217,
	CHAIR=218,
	STATUE_2=219,
	KEY_FLOOR_PICKED_UP=216,
	WALL_1=224,
	WALL_2=225,
	VINE_1=226,
	VINE_2=227,
	VINE_3=228,
	FOOTBOARD_DOWN=229,
	FOOTBOARD_UP=230,
	MAGIC_FLUID_1=237,
	MAGIC_FLUID_2=238,
	MAGIC_FLUID_3=239,
	CRYSTAL_WHOLE=251,
	CRYSTAL_BROKEN=252,
	LAVA_1=253,
	LAVA_2=254,
	LAVA_3=255
}

-- global variables
local t=0	-- global time, used in TIC() method
local cam={x=0,y=0}
local mobs={}
local animTiles={}
local traps={}
local bullets={}
local firstRun=true

-- debug utilities
local function PrintDebug(object)
	for i,v in pairs(object) do
		-- printable elements
		if type(v)~="table" and type(v)~="boolean" and type(v)~="function" and v~=nil then
			trace("["..i.."] "..v)
		-- boolean
		elseif type(v)=="boolean" then
			trace("["..i.."] "..(v and "true" or "false"))
		-- table, only ids
		elseif type(v)=="table" then
			local txt="["
			for y,k in pairs(v) do
				txt=txt..(y..",")
			end
			txt=txt.."]"
			trace("["..i.."] "..txt)
		-- function
		elseif type(v)=="function" then
			trace("["..i.."] ".."function")
		end
	end		
	trace("------------------")
end

------------------------------------------
-- collision class
local Collision={}

function Collision:GetEdges(x,y,w,h)
	local w=w or CELL-1
	local h=h or CELL-1
	local x=x+(CELL-w)/2
	local y=y+(CELL-h)/2
	
	-- get the map ids in the edges	
	local topLeft=mget(x/CELL,y/CELL)
	local topRight=mget((x+w)/CELL,y/CELL)
	local bottomLeft=mget(x/CELL,(y+h)/CELL)
	local bottomRight=mget((x+w)/CELL,(y+h)/CELL)
	
	return topLeft,topRight,bottomLeft,bottomRight
end

function Collision:CheckSolid(indx)
	return indx==tiles.WALL_1 or					
			  indx==tiles.WALL_2 or
			  indx==tiles.CLOSED_GRATE or
			  indx==tiles.MIMIC_HARM or
			  indx==tiles.COLUMN or
			  indx==tiles.DESK or
			  indx==tiles.STATUE_1 or
			  indx==tiles.STATUE_2 or
			  indx==tiles.ALTAR or
			  indx==tiles.CRYSTAL_WHOLE or
			  indx==tiles.CLOSED_DOOR or		
			  indx==tiles.CLOSED_COFFER	or	
			  indx==tiles.OPENED_COFFER
end

function Collision:CheckDanger(indx)
	return indx==tiles.LAVA_1 or
			  indx==tiles.LAVA_2 or
			  indx==tiles.LAVA_3 or
			  indx==tiles.MIMIC_HARM or
			  indx==tiles.SPIKES_HARM or
			  indx==tiles.FOOTBOARD_DOWN
end

------------------------------------------
-- animation class
local function Anim(span,frames,loop)
	local s={
		span=span or 60,
		frame=0,
		loop=loop==nil and true or false, -- this code sucks!
		tick=0,
		indx=0,
		frames=frames or {},
		ended=false
	}
	
	function s.Update(time)	
		if time>=s.tick and #s.frames>0 then
			if s.loop then
				s.indx=(s.indx+1)%#s.frames
				s.frame=s.frames[s.indx+1]
				s.ended=false
			else
				s.indx=s.indx<#s.frames and s.indx+1 or #s.frames
				s.frame=s.frames[s.indx]
				if s.indx==#s.frames then s.ended=true end
			end
			s.tick=time+s.span
		end 
	end
	
	function s.RandomIndx()
		s.indx=math.random(#s.frames)
	end
	
	function s.Reset()
			s.indx=0
			s.ended=false
	end
	
	return s
end

------------------------------------------
-- animated tile class
local function AnimTile(cellX,cellY,anim)
	local s={
		cellX=cellX or 0,
		cellY=cellY or 0,
		anim=anim
	}
	
	function s.Display(time)
		if s.anim==nil then return end
		s.anim.Update(time)
		mset(s.cellX,s.cellY,s.anim.frame)
	end
	
	return s
end

------------------------------------------
-- trap class
local function Trap(cellX,cellY,idSafe,idDanger)
	local s={
		cellX=cellX or 0,
		cellY=cellY or 0,
		timeSafe=180,
		timeDanger=120,
		idSafe=idSafe,
		idDanger=idDanger,
		dangerous=math.random(2)-1==0,
		frame=nil,
		tick=0
	}
	
	function s.Update(time)
		if time>=s.tick and s.dangerous then
			s.frame=s.idSafe
			s.dangerous=false
			s.tick=time+s.timeSafe
		elseif time>=s.tick and not s.dangerous then
			s.frame=s.idDanger
			s.dangerous=true
			s.tick=time+s.timeDanger
		end
	end
	
	function s.Display(time)
		mset(s.cellX,s.cellY,s.frame)
	end
	
	return s
end

------------------------------------------
-- bullet class
local function Bullet(x,y,vx,vy)
	local s={
		tag="bullet",
		x=x or 0,
		y=y or 0,
		vx=vx or 0,
		vy=vy or 0,
		anims={},	-- move, collided
		alpha=8,
		died=false,
		range=4*CELL,
		power=1,
		player=nil,
		xStart=x or 0,
		yStart=y or 0,
		curAnim=nil,
		flip=false,
		collided=false,
		visible=true
	}

	function s.Move()	
		-- detect flip
		if s.vx~=0 then s.flip=s.vx<0 and 1 or 0 end
		
		-- next position
		local nx=s.x+s.vx
		local ny=s.y+s.vy
		
		-- check the collision on the edges
		local tl,tr,bl,br=Collision:GetEdges(nx,ny,CELL-3,CELL-3)
		
		if not Collision:CheckSolid(tl) and not
				 Collision:CheckSolid(tr) and not
				 Collision:CheckSolid(bl) and not
				 Collision:CheckSolid(br) then
			s.x=nx
			s.y=ny
			if not s.collided then s.curAnim=s.anims.move end
		else
			s.Collide()
		end
	
    	-- bounds
		if s.x<0 then s.x=0 end
		if s.x>MAP_W-CELL then s.x=MAP_W-CELL end
		if s.y<0 then s.y=0 end
		if s.y>MAP_H-CELL then s.y=MAP_H-CELL end	
	end
	
	function s.Collide()
		if not s.collided then
			s.curAnim=s.anims.collided
			s.curAnim.Reset()
			s.collided=true
		end
	end
	
	function s.Update(time)
		-- detect if we are in the camera bounds
		s.visible=s.x>=cam.x and s.x<=cam.x+CAM_W-CELL and s.y>=cam.y and s.y<=cam.y+CAM_H-CELL

		-- do something at the end of the animation
		-- for the bullet means that it is in collided state
		if s.curAnim~=nil and s.curAnim.ended then
			s.died=true
			s.visible=false
		end
		
		-- block the movement with these conditions
		if s.died or s.collided then return end
		
		s.Move()
		
		-- detect collisions with the player
		local dx=s.player.x-s.x
		local dy=s.player.y-s.y
		local dst=math.sqrt(dx^2+dy^2)
		if dst<=CELL then
			s.Collide()
			s.player.Damaged(s.power)
		end

		-- if the range is exceeded, ends the life of the bullet
		local dx=s.x-s.xStart
		local dy=s.y-s.yStart
		if math.sqrt(dx^2+dy^2)>s.range then s.Collide() end		
	end
	
	function s.Display(time)
		if s.curAnim==nil then return end
		s.curAnim.Update(time)
		-- arrangement to correctly display it in the map
		local x=(s.x-cam.x)%CAM_W
		local y=(s.y-cam.y)%CAM_H
		if s.visible then
			spr(s.curAnim.frame,x,y,s.alpha,1,s.flip)
			-- rectb(x,y,CELL,CELL,14)
		end		
	end
	
	return s
end

------------------------------------------
-- mob class
local function Mob(x,y,player)
	local s={
		tag="mob",
		x=x or 0,
		y=y or 0,
		alpha=8,
		anims={},	-- idle, walk, attack, die, damaged
		health=3,
		power=1,
		fov=6*CELL,
		proximity=CELL-2,		
		died=false,
		dx=0,
		dy=0,
		flip=false,
		visible=true,
		curAnim=nil,
		attack=false,
		damaged=false,
		tick=0,
		player=player,
		damageable=true,	-- affected by traps or dangerous fluids
		minAttackDelay=30,
		maxAttackDelay=100,
		score=0,
		speed=0.3+math.random()*0.1
	}
	
	function s.Move(dx,dy)
		-- block movement with these conditions
		if s.died or s.attack then return end
	
		-- store deltas, they could be useful
		s.dx=dx
		s.dy=dy
		
		-- detect flip
		if dx~=0 then s.flip=dx<0 and 1 or 0 end
		
		-- next position
		local nx=s.x+dx
		local ny=s.y+dy
	
		-- check the collision on the edges
		local tl,tr,bl,br=Collision:GetEdges(nx,ny,CELL-2,CELL-2)
		
		if not Collision:CheckSolid(tl) and not
				 Collision:CheckSolid(tr) and not
				 Collision:CheckSolid(bl) and not
				 Collision:CheckSolid(br) then
			s.x=nx
			s.y=ny
			if not s.damaged then s.curAnim=s.anims.walk end
		end
	
    	-- bounds
		if s.x<0 then s.x=0 end
		if s.x>MAP_W-CELL then s.x=MAP_W-CELL end
		if s.y<0 then s.y=0 end
		if s.y>MAP_H-CELL then s.y=MAP_H-CELL end
	end
	
	function s.Attack()
		if not s.attack then
			if s.player~=nil then s.player.Damaged(s.power) end
			s.curAnim=s.anims.attack
			s.curAnim.Reset()
			s.attack=true
		end
	end
	
	function s.Damaged(amount)
		if not s.damaged and not s.died then
			s.curAnim=s.anims.damaged
			s.curAnim.Reset()
			s.damaged=true
			local amt=amount or 1
			s.health=s.health-amt
			if s.health<=0 then s.Die() end
		end
	end
	
	function s.Die()
		if not s.died then
			s.curAnim=s.anims.die
			s.curAnim.Reset()
			s.died=true
			if player~=nil then player.points=player.points+s.score end
		end
	end
	
	function s.Update(time)
		-- detect if we are in the camera bounds
		s.visible=s.x>=cam.x and s.x<=cam.x+CAM_W-CELL and s.y>=cam.y and s.y<=cam.y+CAM_H-CELL
		
		-- do something at the end of the animation
		-- int the case of the mob stop to stay in particular states
		if s.curAnim~=nil and s.curAnim.ended then
			-- s.died=false
			s.attack=false
			s.damaged=false
		end
	
		-- prevent update if it is in particular states
		if s.died or s.attack then return end
		
		-- default: idle
		if not s.died and not s.damaged then s.curAnim=s.anims.idle end
		
		-- move closer to the player if in range
		if s.player~=nil then
			local dx=s.player.x-s.x
			local dy=s.player.y-s.y
			local dst=math.sqrt(dx^2+dy^2)
			if dst<s.fov then
				if dst>=s.proximity then
					if math.abs(dx)>=s.speed then s.Move(dx>0 and s.speed or -s.speed,0) end
					if math.abs(dy)>=s.speed then s.Move(0,dy>0 and s.speed or -s.speed) end
				end
			end
			
			-- try to attack the player
			if dst<=s.proximity then
				s.flip=(s.x-s.player.x) > 0 and 1 or 0
				if time>s.tick then
					s.Attack()
					s.tick=time+math.random(s.minAttackDelay,s.maxAttackDelay)
				end
			-- first contact
			else
				s.tick=time+math.random(0,s.maxAttackDelay)
			end
		end
		
		-- check if damaged
		if s.damageable then
			local tl,tr,bl,br=Collision:GetEdges(s.x-s.dx,s.y-s.dy,2,2)
			if Collision:CheckDanger(tl) or
			   Collision:CheckDanger(tr) or
			   Collision:CheckDanger(bl) or
			   Collision:CheckDanger(br) then
				s.Damaged()
			end
		end
	end
	
	function s.Display(time)
		if s.curAnim==nil then return end
		s.curAnim.Update(time)
		-- arrangement to correctly display it in the map
		local x=(s.x-cam.x)%CAM_W
		local y=(s.y-cam.y)%CAM_H
		if s.visible then
			spr(s.curAnim.frame,x,y,s.alpha,1,s.flip)
			-- rectb(x,y,CELL,CELL,14)
		end		
	end

	return s
end

------------------------------------------
-- mob caster:mob class
local function MobCaster(x,y,player)
	local s=Mob(x,y,player)
	s.tag="mob_caster"
	s.minAttackDelay=90
	s.maxAttackDelay=180
	s.fov=8*CELL
	s.proximity=4*CELL
	s.CreateBullet=function(x,y,vx,vy) return nil end	-- override, must return a Bullet object
	
	function s.Attack()
		if not s.attack then
			-- calculate the direction
			local speed=0.3
			local angle=math.atan2(s.y-(s.player.y+CELL/2),s.x-(s.player.x+CELL/2))
			local dir=(s.x-s.player.x)>0 and -4 or 4
			-- create bullet object
			local b=s.CreateBullet(s.x+dir,s.y,-speed*math.cos(angle),-speed*math.sin(angle))
			table.insert(bullets,b)
			-- update the animation
			s.curAnim=s.anims.attack
			s.curAnim.Reset()
			s.attack=true
		end
	end
	
	return s
end

------------------------------------------
-- boss:mob caster class
local function Boss(x,y,player)
	local s=MobCaster(x,y,player)
	s.tag="boss"
	s.minAttackDelay=40
	s.maxAttackDelay=120
	s.health=10
	s.score=15
	s.damageable=false
	
	s.anims={
		idle=Anim(25,{128,129,130,131}),
		walk=Anim(15,{128,129,130,131}),	
		attack=Anim(15,{132,133,128},false),
		die=Anim(5,{132,137,138,139,140,141},false),
		damaged=Anim(20,{142,128},false)
	}
	
	function s.CreateBullet(x,y,vx,vy)
		local b=Bullet(x,y,vx,vy)
		b.player=s.player
		b.power=s.power
		b.anims={
			move=Anim(5,{118,119,120}),
			collided=Anim(15,{134,135,136},false)
		}
		return b
	end
	
	function s.Attack()
		if not s.attack then
			-- create bullets objects
			local num=6
			for i=1,num do
				-- the boss randomly spawn 6 bullets from his staff
				local speed=0.3
				local angle=math.random()*2*math.pi
				local dir=(s.x-s.player.x)>0 and -3 or 3
				-- create bullet object
				local b=s.CreateBullet(s.x+dir,s.y-CELL-2,-speed*math.cos(angle),-speed*math.sin(angle))
				table.insert(bullets,b)
			end
			-- update the animation
			s.curAnim=s.anims.attack
			s.curAnim.Reset()
			s.attack=true
		end
	end
	
	function s.Move(dx,dy)
		-- block movement with these conditions
		if s.died or s.attack then return end
	
		-- store deltas, they could be useful
		s.dx=dx
		s.dy=dy
		
		-- detect flip
		if dx~=0 then s.flip=dx<0 and 1 or 0 end
		
		-- next position
		local nx=s.x+dx
		local ny=s.y+dy
	
		-- check the collision on the edges
		local tl1,tr1,bl1,br1=Collision:GetEdges(nx,ny,CELL-2,CELL-2)
		local tl2,tr2,bl2,br2=Collision:GetEdges(nx,ny-CELL,CELL-2,CELL-2)
		
		if not Collision:CheckSolid(tl1) and not
				 Collision:CheckSolid(tr1) and not
				 Collision:CheckSolid(bl1) and not
				 Collision:CheckSolid(br1) and not
				 Collision:CheckSolid(tl2) and not
				 Collision:CheckSolid(tr2) and not
				 Collision:CheckSolid(bl2) and not
				 Collision:CheckSolid(br2) then
			s.x=nx
			s.y=ny
			if not s.damaged then s.curAnim=s.anims.walk end
		end
	end
	
	function s.Display(time)
		if s.curAnim==nil then return end
		s.curAnim.Update(time)
		-- arrangement to correctly display it in the map
		local x=(s.x-cam.x)%CAM_W
		local y=(s.y-cam.y)%CAM_H
		if s.visible then
			spr(s.curAnim.frame,x,y,s.alpha,1,s.flip)
			spr(s.curAnim.frame-16,x,y-CELL,s.alpha,1,s.flip)
		end		
	end
	
	return s
end

------------------------------------------
-- player:mob class
local function Player(x,y)
	local s=Mob(x,y)
	s.tag="player"
	s.keys=0
	s.potions=0
	s.health=5
	s.maxHealth=5
	s.points=0
	
	s.anims={
		idle=Anim(60,{16,17}),
		walk=Anim(15,{18,19}),	
		attack=Anim(15,{20,17},false),
		die=Anim(5,{21,22,23,24,25},false),
		damaged=Anim(20,{26,17},false)
	}
	
	-- store the super method
	local supMove=s.Move
	
	function s.Move(dx,dy)
		-- call super.move
		supMove(dx,dy)
		
		-- flip
		if dx~=0 then s.flip=dx<0 and 1 or 0 end
		
		-- detect collisions with special elements
		CheckDoors(dx,dy)
		CheckCoffers(dx,dy)
		CheckKeys(dx,dy)
	end
	
	function s.Update(time)		
		s.dx=0
		s.dy=0
		
		-- do something at the end of the animation
		-- in the case of player stop to stay in particular states
		if s.curAnim~=nil and s.curAnim.ended then
			-- s.died=false
			s.attack=false
			s.damaged=false
		end
		
		-- prevent update in these particular states
		if s.died or s.attack then return end	
		
		-- default: idle
		if not s.damaged then s.curAnim=s.anims.idle end
		
		-- manage the user input
		-- arrows [0,1,2,3].......movement
		-- z [4].........................attack
		-- x [5].........................healing
		if btn(5) and s.health<s.maxHealth and s.potions>0 then
			s.health=s.maxHealth
			s.potions=s.potions-1
			if s.health > s.maxHealth then s.health=s.maxHealth end
		end
		if btn(4) then s.Attack() end
		if btn(0) then s.Move(0,-1) end
		if btn(1) then s.Move(0,1) end
		if btn(2) then s.Move(-1,0) end
		if btn(3) then s.Move(1,0)	end
		
		-- check if damaged
		if s.damageable then
			local tl,tr,bl,br=Collision:GetEdges(s.x-s.dx,s.y-s.dy,2,2)
			if Collision:CheckDanger(tl) or
			   Collision:CheckDanger(tr) or
			   Collision:CheckDanger(bl) or
			   Collision:CheckDanger(br) then
				s.Damaged()
			end
		end
		
		-- check enemies
		for i,mob in pairs(mobs) do
			-- collision detected
			if math.abs(s.x-mob.x)<CELL and math.abs(s.y-mob.y)<CELL then
				-- if attacked then damaged
				if s.attack then mob.Damaged(s.power) end
			end
		end
	end
	
	function s.Display(time)
		if s.curAnim==nil then return end
		s.curAnim.Update(time)
		-- assure that the player is always in the camera bounds
		spr(s.curAnim.frame,s.x%(CAM_W-CELL),s.y%(CAM_H-CELL),s.alpha,1,s.flip)
		-- rectb(s.x+s.dx*3+(CELL-2)/2,s.y+s.dy*3+(CELL-2)/2,2,2,14)
	end
	
	function CheckDoors(dx,dy)
		-- open door if have the key
		tl,tr,bl,br=Collision:GetEdges(s.x+dx,s.y+dy)
		if tl==tiles.CLOSED_DOOR and s.keys>0 then
			s.keys=s.keys-1
			mset((s.x+dx)/CELL,(s.y+dy)/CELL,tiles.OPENED_DOOR)
		elseif tr==tiles.CLOSED_DOOR and s.keys>0 then
			s.keys=s.keys-1
			mset((s.x+dx+CELL-1)/CELL,(s.y+dy)/CELL,tiles.OPENED_DOOR)
		elseif bl==tiles.CLOSED_DOOR and s.keys>0 then
			s.keys=s.keys-1
			mset((s.x+dx)/CELL,(s.y+dy+CELL-1)/CELL,tiles.OPENED_DOOR)
		elseif br==tiles.CLOSED_DOOR and s.keys>0 then
			s.keys=s.keys-1
			mset((s.x+dx+CELL-1)/CELL,(s.y+dy+CELL-1)/CELL,tiles.OPENED_DOOR)
		end
	end
	
	function CheckCoffers(dx,dy)
		-- collect the potions in the coffers
		tl,tr,bl,br=Collision:GetEdges(s.x+dx,s.y+dy)
		local probability=70
		
		if tl==tiles.CLOSED_COFFER then
			mset((s.x+dx)/CELL,(s.y+dy)/CELL,tiles.OPENED_COFFER)
			if math.random(100)<probability then s.potions=s.potions+1 end			
		elseif tr==tiles.CLOSED_COFFER then
			mset((s.x+dx+CELL-1)/CELL,(s.y+dy)/CELL,tiles.OPENED_COFFER)
			if math.random(100)<probability then s.potions=s.potions+1 end			
		elseif bl==tiles.CLOSED_COFFER then
			mset((s.x+dx)/CELL,(s.y+dy+CELL-1)/CELL,tiles.OPENED_COFFER)
			if math.random(100)<probability then s.potions=s.potions+1 end
		elseif br==tiles.CLOSED_COFFER then
			mset((s.x+dx+CELL-1)/CELL,(s.y+dy+CELL-1)/CELL,tiles.OPENED_COFFER)
			if math.random(100)<probability then s.potions=s.potions+1 end
		end
		-- limit the amount of potions
		if s.potions>5 then s.potions=5 end
	end
	
	function CheckKeys(dx,dy)
		-- collect the keys
		tl,tr,bl,br=Collision:GetEdges(s.x+dx,s.y+dy)
		if tl==tiles.KEY_FLOOR then
			s.keys=s.keys+1
			mset((s.x+dx)/CELL,(s.y+dy)/CELL,tiles.KEY_FLOOR_PICKED_UP)			
		elseif tr==tiles.KEY_FLOOR then
			s.keys=s.keys+1
			mset((s.x+dx+CELL-1)/CELL,(s.y+dy)/CELL,tiles.KEY_FLOOR_PICKED_UP)
		elseif bl==tiles.KEY_FLOOR then
			s.keys=s.keys+1
			mset((s.x+dx)/CELL,(s.y+dy+CELL-1)/CELL,tiles.KEY_FLOOR_PICKED_UP)
		elseif br==tiles.KEY_FLOOR then
			s.keys=s.keys+1
			mset((s.x+dx+CELL-1)/CELL,(s.y+dy+CELL-1)/CELL,tiles.KEY_FLOOR_PICKED_UP)
		end
	end
	
	return s
end

------------------------------------------
-- HUD
local function DisplayHUD(player)
	-- format the score
	if player.points<=9 then print("score:"..player.points,CAM_W-43,0) end
	if player.points>9 and player.points<=99 then print("score:"..player.points,CAM_W-49,0) end
	if player.points>99 then print("score:"..player.points,CAM_W-55,0) end
	-- draw the player stats and equipment
	for i=0,player.health do spr(tiles.HEART,CAM_W-CELL*i,CELL,player.alpha) 	end
	for i=0,player.potions do spr(tiles.POTION,CAM_W-CELL*i,2*CELL,player.alpha) end
	for i=0,player.keys do spr(tiles.KEY,CAM_W-CELL*i,3*CELL,player.alpha) end
end

local function DisplayMessages(player,boss)
	-- if player is dead show message
	if player.died and not boss.died then
		spr(154,CAM_W/2-CELL*2,CAM_H/2-CELL*2,8,2)
		spr(155,CAM_W/2,CAM_H/2-CELL*2,8,2)
		spr(170,CAM_W/2-CELL*2,CAM_H/2,8,2)
		spr(171,CAM_W/2,CAM_H/2,8,2)
		print("YOU DIED",CAM_W/2-22,CAM_H/2+18)
		print("PRESS X TO RESTART",CAM_W/2-54,CAM_H/2+26)
	end
	
	-- if boss is dead show message
	if boss.died then
		spr(152,CAM_W/2-CELL*2,CAM_H/2-CELL*2,8,2)
		spr(153,CAM_W/2,CAM_H/2-CELL*2,8,2)
		spr(168,CAM_W/2-CELL*2,CAM_H/2,8,2)
		spr(169,CAM_W/2,CAM_H/2,8,2)
		print("YOU WIN",CAM_W/2-20,CAM_H/2+18)
		print("PRESS X TO RESTART",CAM_W/2-54,CAM_H/2+26)
	end
end

------------------------------------------
-- spawners
local function SpawnBlob(cellX,cellY,player)
	local m=Mob(cellX*CELL,cellY*CELL,player)
	m.tag="blob"
	m.anims={
		idle=Anim(20,{32,33,34}),
		walk=Anim(8,{34,35,36,37,38}),	
		attack=Anim(5,{39,40,41,32},false),
		die=Anim(5,{42,43,44},false),
		damaged=Anim(20,{45,32},false),
	}
	m.health=2
	m.score=1
	m.damageable=false
	return m
end

local function SpawnGoblin(cellX,cellY,player)
	local m=Mob(cellX*CELL,cellY*CELL,player)
	m.tag="goblin"
	m.score=2
	m.anims={
		idle=Anim(60,{48,49}),
		walk=Anim(15,{50,51}),	
		attack=Anim(15,{52,49},false),
		die=Anim(5,{53,54,55,56},false),
		damaged=Anim(20,{57,49},false),
	}
	return m
end

local function SpawnWraith(cellX,cellY,player)
	local m=MobCaster(cellX*CELL,cellY*CELL,player)
	m.tag="wraith"
	m.anims={
		idle=Anim(25,{80,81,82,81}),
		walk=Anim(15,{80,81,82,81}),	
		attack=Anim(20,{83,80},false),
		die=Anim(5,{90,91,92,93},false),
		damaged=Anim(20,{94,95,80},false)
	}
	m.health=6
	m.score=4
	m.power=2
	m.damageable=false
	
	function m.CreateBullet(x,y,vx,vy)
		local b=Bullet(x,y,vx,vy)
		b.player=m.player
		b.power=m.power
		b.anims={
			move=Anim(5,{84,85,86}),
			collided=Anim(15,{87,88,89},false)
		}
		return b	
	end
	
	return m
end

local function SpawnGolem(cellX,cellY,player)
	local m=Mob(cellX*CELL,cellY*CELL,player)
	m.tag="golem"
	m.health=7
	m.score=6
	m.power=2
	m.fov=4*CELL
	m.anims={
		idle=Anim(60,{64,65}),
		walk=Anim(15,{66,67}),	
		attack=Anim(15,{68,69,70,64},false),
		die=Anim(20,{71,72,73,74},false),
		damaged=Anim(20,{75,64},false),
	}
	return m
end

local function SpawnKnight(cellX,cellY,player)
	local m=Mob(cellX*CELL,cellY*CELL,player)
	m.tag="knight"
	m.health=10
	m.power=1
	m.score=7
	m.fov=7*CELL
	m.anims={
		idle=Anim(50,{96,97}),
		walk=Anim(15,{98,99}),	
		attack=Anim(15,{100,97},false),
		die=Anim(5,{101,102,103,104,105},false),
		damaged=Anim(20,{106,97},false)
	}
	return m
end

------------------------------------------
-- init
local p1,boss,golem1,golem2

local function Init()
	-- detect if run for the first time
	if not firstRun then return end
	firstRun=false
	
	-- clear tables
	mobs={}
	animTiles={}
	traps={}
	bullets={}
	
	-- create player
	p1=Player(8*CELL,6*CELL)

	-- create boss
	boss=Boss(124*CELL,24*CELL,p1)
	table.insert(mobs,boss)

	-- create monsters
	table.insert(mobs,SpawnBlob(16,23,p1))
	table.insert(mobs,SpawnBlob(17,24,p1))
	table.insert(mobs,SpawnBlob(18,26,p1))
	table.insert(mobs,SpawnBlob(7,25,p1))
	table.insert(mobs,SpawnBlob(6,28,p1))

	table.insert(mobs,SpawnGoblin(40,9,p1))
	table.insert(mobs,SpawnGoblin(38,4,p1))

	table.insert(mobs,SpawnGoblin(63,13,p1))
	table.insert(mobs,SpawnGoblin(67,12,p1))
	table.insert(mobs,SpawnBlob(76,7,p1))
	table.insert(mobs,SpawnBlob(75,8,p1))
	table.insert(mobs,SpawnBlob(79,9,p1))
	table.insert(mobs,SpawnBlob(82,7,p1))
	table.insert(mobs,SpawnBlob(77,11,p1))
	table.insert(mobs,SpawnBlob(87,14,p1))

	table.insert(mobs,SpawnBlob(82,26,p1))
	table.insert(mobs,SpawnBlob(77,24,p1))
	table.insert(mobs,SpawnBlob(68,26,p1))

	table.insert(mobs,SpawnGoblin(73,40,p1))
	table.insert(mobs,SpawnGoblin(81,46,p1))

	table.insert(mobs,SpawnWraith(34,41,p1))

	table.insert(mobs,SpawnGoblin(37,55,p1))

	table.insert(mobs,SpawnBlob(15,56,p1))
	table.insert(mobs,SpawnBlob(11,61,p1))
	table.insert(mobs,SpawnBlob(14,63,p1))
	table.insert(mobs,SpawnBlob(14,63,p1))
	table.insert(mobs,SpawnBlob(17,57,p1))

	table.insert(mobs,SpawnWraith(56,73,p1))
	table.insert(mobs,SpawnWraith(48,81,p1))

	table.insert(mobs,SpawnGoblin(69,96,p1))
	table.insert(mobs,SpawnGoblin(74,100,p1))
	table.insert(mobs,SpawnGoblin(72,103,p1))

	table.insert(mobs,SpawnGolem(76,108,p1))

	table.insert(mobs,SpawnBlob(97,101,p1))
	table.insert(mobs,SpawnBlob(102,103,p1))
	table.insert(mobs,SpawnGoblin(93,107,p1))

	-- golems chamber
	golem1=SpawnGolem(119,103,p1)
	table.insert(mobs,golem1)
	golem2=SpawnGolem(119,108,p1)
	table.insert(mobs,golem2)

	table.insert(mobs,SpawnBlob(95,86,p1))
	table.insert(mobs,SpawnBlob(96,90,p1))

	table.insert(mobs,SpawnKnight(102,69,p1))

	table.insert(mobs,SpawnKnight(131,60,p1))
	table.insert(mobs,SpawnKnight(131,65,p1))

	table.insert(mobs,SpawnWraith(121,42,p1))
	
	-- just for curiosity
	local maxScore=0
	for i,mob in pairs(mobs) do 
		maxScore=maxScore+mob.score
	end
	-- trace("maxScore:"..maxScore)
	
	-- cycle the map and manage the special elements
	for y=0,MAP_W/CELL do
		for x=0,MAP_H/CELL do	
			-- animated tiles
			if mget(x,y)==tiles.LAVA_1 or mget(x,y)==tiles.LAVA_2 or mget(x,y)==tiles.LAVA_3 then
				local tile=AnimTile(x,y,Anim(30,{tiles.LAVA_1,tiles.LAVA_2,tiles.LAVA_3}))
				tile.anim.RandomIndx()
				table.insert(animTiles,tile)
			end
			if mget(x,y)==tiles.MAGIC_FLUID_1 or mget(x,y)==tiles.MAGIC_FLUID_2 or mget(x,y)==tiles.MAGIC_FLUID_3 then
				local tile=AnimTile(x,y,Anim(30,{tiles.MAGIC_FLUID_1,tiles.MAGIC_FLUID_2,tiles.MAGIC_FLUID_3}))
				tile.anim.RandomIndx()
				table.insert(animTiles,tile)
			end
			if mget(x,y)==tiles.VINE_1 or mget(x,y)==tiles.VINE_2 or mget(x,y)==tiles.VINE_3 then
				local tile=AnimTile(x,y,Anim(20,{tiles.VINE_1,tiles.VINE_2,tiles.VINE_3}))
				tile.anim.RandomIndx()
				table.insert(animTiles,tile)
			end
			-- traps
			if mget(x,y)==tiles.SPIKES_HARMLESS or mget(x,y)==tiles.SPIKES_HARM then
				local trap=Trap(x,y,tiles.SPIKES_HARMLESS,tiles.SPIKES_HARM)
				trap.timeDanger=math.random(40,80)
				trap.timeSafe=math.random(40,80)
				table.insert(traps,trap)
			end
			if mget(x,y)==tiles.MIMIC_HARMLESS or mget(x,y)==tiles.MIMIC_HARM then
				local trap=Trap(x,y,tiles.MIMIC_HARMLESS,tiles.MIMIC_HARM)
				trap.timeDanger=math.random(40,60)
				trap.timeSafe=math.random(40,120)
				table.insert(traps,trap)
			end
			if mget(x,y)==tiles.FOOTBOARD_UP or mget(x,y)==tiles.FOOTBOARD_DOWN then
				local trap=Trap(x,y,tiles.FOOTBOARD_UP,tiles.FOOTBOARD_DOWN)
				trap.timeDanger=math.random(40,60)
				trap.timeSafe=math.random(210,240)
				table.insert(traps,trap)
			end
			-- reset doors
			if mget(x,y)==tiles.OPENED_DOOR then mset(x,y,tiles.CLOSED_DOOR) end
			-- reset coffers
			if mget(x,y)==tiles.OPENED_COFFER then mset(x,y,tiles.CLOSED_COFFER) end
			-- reset keys
			if mget(x,y)==tiles.KEY_FLOOR_PICKED_UP then mset(x,y,tiles.KEY_FLOOR) end
			-- reset golems chamber
			mset(110,105,tiles.CRYSTAL_BROKEN)
			-- reset boss grate
			mset(124,32,tiles.OPENED_GRATE)
		end
	end
end

local function SpecialEvents()
	-- when enter in the golems chamber close the entrance
	-- until both golems are died
	if p1.x>=111*CELL-1 and p1.x<=112*CELL+1 and p1.y>=104*CELL-1 and p1.y<=105*CELL+1 then
		mset(110,105,tiles.CRYSTAL_WHOLE)
	end
	
	if golem1.died and golem2.died then
		mset(110,105,tiles.CRYSTAL_BROKEN)
	end
	
	-- when player reach the boss close the grate
	if p1.x>=124*CELL-1 and p1.x<=125*CELL+1 and p1.y>=30*CELL-1 and p1.y<=31*CELL+1 then
		mset(124,32,tiles.CLOSED_GRATE)
	end
end

------------------------------------------				
-- main
function TIC()
	-- runs only the first time or to reset the game
	Init()
	-- reset the game if the player is died and is pressed x
	if btn(5) and p1.died then firstRun=true end
	if btn(5) and boss.died then firstRun=true end
	
	-- set the camera and draw the background
	cam.x=p1.x-p1.x%(CAM_W-CELL)
	cam.y=p1.y-p1.y%(CAM_H-CELL)
	-- cls(3)
	map(cam.x/CELL,cam.y/CELL,CAM_W/CELL,CAM_H/CELL)
		
	------------- UPDATE -------------
	SpecialEvents()
	
	-- mobs
	for i,mob in pairs(mobs) do mob.Update(t) end
	-- player
	p1.Update(t)
	-- bullets
	for i,v in pairs(bullets) do
		if bullets[i].died then table.remove(bullets,i)
		else bullets[i].Update(t) end
	end
	-- traps
	for i,trap in pairs(traps) do trap.Update(t) end
	
	------------- DISPLAY -------------
	-- animated tiles
	for i,tile in pairs(animTiles) do tile.Display(t) end	
	-- mobs
	for i,mob in pairs(mobs) do mob.Display(t) end
	-- player
	p1.Display(t)
	-- bullets
	for i,bullet in pairs(bullets) do bullet.Display(t) end
	-- traps
	for i,trap in pairs(traps) do trap.Display(t) end

	-- HUD
	DisplayHUD(p1)
	DisplayMessages(p1,boss)

	-- increment global time
	t=t+1
end

-- <TILES>
-- 016:884448888444448884c0c0888fcccc888fffff8844ffff48c33344c883888388
-- 017:884448888444448884c0c0888fcccc8844ffff48c333ffc88444448883888388
-- 018:884448888444448884c0c0888fcccc8844ffff88c333ffc88444443883888888
-- 019:884448888444448884c0c0888fcccc88844fff888c333f888434448888888388
-- 020:884448888444448884c0c0888fcccc88aa4fff8884aa33388444448883888388
-- 021:884448888444448884c6c6888fcccc888fffff8844ffff48c33344c883888388
-- 022:8888888888888a88884948888444498883f636834aff338cc4343a4c8c833388
-- 023:8888888888883888888888a88844888883f938388af3a8888336364cca3a3338
-- 024:888888888888888888888888888888888a8888888888a88883a938888a333388
-- 025:88888888888888888888888888888888888888888888888888a3388888333a38
-- 026:8844488884444488846060888f6666888fffff8844ffff486333446883888388
-- 032:8888888888888888888888888888888888868688888e9988889e9988889ee988
-- 033:8888888888888888888888888888888888868688888e9e8888999e888899e988
-- 034:88888888888888888888888888888888888686888889e98888999e8888e9e988
-- 035:8888888888888888888888888886e688889ee988889e9e88889ee88888888888
-- 036:8888888888888888888686888889998888e9ee8888eee8888888888888888888
-- 037:88888888888888888888888888868688888e9988888ee98888889e8888888888
-- 038:88888888888888888888888888888888888888888886e68888eee98888e99988
-- 039:8888888888888e888889889888e8888888868698898e9988889e9988889ee988
-- 040:88888888888888888888889888888e888886869e888e9988889e998e889ee988
-- 041:888888888888888888888888888888e888868688888e9988889e998e889ee988
-- 042:8888888888888888888888888888888888868688888ea988889e998888aeea88
-- 043:888888888888888888888888888888888888888888888888888a368888833a88
-- 044:8888888888888888888888888888888888888888888888888888888888883a88
-- 045:8888888888888888888888888888888888898988888e6988886699888896e688
-- 048:888888888858588888bbb8388865683888b5b838851115b88b11188888585888
-- 049:888888888858588888bbb8888865683888b5b83888111838851115b88b585888
-- 050:888888888858588888bbb8388865683888b5b838851115b88b11588888588888
-- 051:888888888858588888bbb8388865683888b5b838851115b88b51188888885888
-- 052:888888888858588888bbb88888656a8888b5baa885111baa8b11183388585888
-- 053:88888888888888888858588888b5983888a56838856118b88b1a15b88858a8a8
-- 054:88888888888888888888888888b8888888583888859338388b6a953888a8a8a8
-- 055:888888888888888888888888888888888888888888888888889a38888833a388
-- 056:8888888888888888888888888888888888888888888888888888a38888833a88
-- 057:88888888886868888899983888c6c83888969838861116988911188888686888
-- 064:888a388888a3338888d33d8883333338383dda8338afda83a83aa38a88388388
-- 065:888a388888a3338888d33d8888333388833dda3838afda83383aa383a838838a
-- 066:888a388888a3338888d33d8883333388383dda38383fda83a83aa3838888838a
-- 067:888a388888a3338888d33d8888333338833dda8338afd383383aa38aa8388888
-- 068:888a388888a33388a8d33d8a38333383333dda3388afda88883aa38888388388
-- 069:888a388888a3338888d33d8888333388833dda38a8afda8a883aa38888388388
-- 070:888a388888a3338888d33d8888333388833dda383aafdaa3383aa383a838838a
-- 071:888a388888a33388889339888a33333838399a8338ae9a83a83aa38a88388388
-- 072:888a388888a3338888933a888a3333883839aa3838aeaa83a83aa3838838838a
-- 073:888a388888a3338888933a8888333388833a3a3838a3aa83383aa383a838838a
-- 074:888a388888a3338888a3338888333388833a3a3883a3aa38833aa3388a3883a8
-- 075:888a388888a33388889339888a33333838399a8338ae9a83a83aa38a88388388
-- 080:88aaaf8888a0a088882faf888111a11881121128882212888882188888882888
-- 081:88aaaf8888a0a088882faf888221a11881121128812112888122888882888888
-- 082:88aaaf8888a0a088812faf888121a11881121288111128881228888888888888
-- 083:8888888888a22d8881a02088822f2d128112a122112118822218888888888888
-- 084:8888888888888888888888888d822888888d2888288888888888888888888888
-- 085:888888888888888888888888828d2888d8822888888888888888888888888888
-- 086:8888888888888888d8888888882228888882d888888888888888888888888888
-- 087:888888888d828888828888888d88888888d28888228d888888d8888888828888
-- 088:888888888888888888888888888d8888882888888d8d28888888888888888888
-- 089:8888888888888888888888888888888888828888888d88888888888888888888
-- 090:88aaaf8888a0a088882faf888811a18888121188888212888882188888882888
-- 091:8888888888888888882aaf8888a0a088881faf888882a2888882188888882888
-- 092:8888888888888188888882888881888888888888888aaf8888a0a088888faf28
-- 093:888888888888888888888888888888888888888888888888888aa88888a3a388
-- 094:88aaaf8818a9a981282faf212111a12182121122882812888888888888888888
-- 095:88aaaf8888a9a988882faf282111a12182121122112812811888888188888888
-- 096:808880888000008880060688800a0a888000008800000008a99900a883888388
-- 097:808880888000008880060688800a0a8800000008a99900a88000008883888388
-- 098:808880888000008880060688800a0a8800000008a99900a88000003883888888
-- 099:808880888000008880060688800a0a88800000888a9990888030008888888388
-- 100:808880888000008880060688800a0a8866000088806699988000008883888388
-- 101:808880888000008880090988800a0a888000008800000008a99900a883888388
-- 102:8888888888888a888809088880000988830636830a00338ca0303a0c8a833388
-- 103:8888888888883888888888a888008888830938388a03a8888336360aaa3a3338
-- 104:888888888888888888888888888888888a8888888888a88883a938888a333388
-- 105:88888888888888888888888888888888888888888888888888a3388888333a38
-- 106:838883888333338883393988833a3a888333338833333338a99933a883888388
-- 112:8888008888800888880008888000008d88606884881118848001008480000048
-- 113:8888008888800888880008888000008d88606884881118848001008480000048
-- 114:888888888888008888800888880008888000008d886068848811188480010048
-- 115:888888888888008888800888880008888000008d886068848811188480010048
-- 116:8888002888810811881008128000008d81606814821118848112028480012048
-- 117:8888008882800828880008888000008d88606824821118848100028480020048
-- 118:8888888888881888888888888181888888821818828218888888888888888888
-- 119:8888888888888888888828888888888882812888888212888888888888818888
-- 120:8888888888888888888888888881188282811888888288188888828888888888
-- 121:8888022888812211881028128000008d81a1a814821122828112118480212048
-- 122:888808888888882888288888808800888881a828881128888882d88481812088
-- 123:8888888888888888888888888888888888888288888888888888888888828888
-- 124:8888888888888888888888888888888888888888888888888888888888888888
-- 125:8888888888888888888888888888888888888888888888888888888888888888
-- 126:8888008888800888880008888000008d88e0e884882228848002008480000048
-- 128:8100014881010218810008418802084882002182812028888812888888888888
-- 129:8100024821210218810108488802184881100188821028888881288888888888
-- 130:8100004881200118810008228102084881002248821002888811188888812888
-- 131:8100001881002118810001428102024881201848821012888821188888118888
-- 132:8200114281110212812008418811024882010282822011888822188888888888
-- 133:8100014881010218812008488802084881002188821118888821288888888888
-- 134:888888888218821888888828881dd882828d1888881188288821818888888888
-- 135:8288888882888212888888882818888288881888888888888881888282282288
-- 136:8888288881888888888888888888888282888888888888888888888828818828
-- 137:8212114281111211811118118811111881121282822211888821288888888888
-- 138:8882288888211181882181888822888888821888881128188882188888888888
-- 139:88188888888e288888e22e8888e2de888882e888881128888882188888888888
-- 140:888888888888888888888888888e188888e22e8888e1de888882e88888888888
-- 141:88888888888888888888888888888888888ee88888e22e8888e2de88888ee888
-- 142:8100014881010218810008418802084881002182821028888821888888888888
-- 149:77777771777177117711711171117aaa7a11711177a17a11777a77a17777777a
-- 150:777777771771777711711777aa71117711711a771a71a777a77a777777777777
-- 152:8888888888dd88ee888deeee8d8eee22ddee22228dee22228ee222228ee12222
-- 153:8888dd88eedd8888eeee8d8822eeeddd2222eed82222ee8822222ee822221ee8
-- 154:8868688888668fff8866ffff866fffff886fffff68afffff88af333f86af0003
-- 155:68688888fff88888ffff8688fffff688fffff668fffffa88a333fa683000fa66
-- 156:777777777111717171a17171711171617aa1917177717111796a7aaa76776777
-- 157:777777777111711171a171aa7111711171aa9aa1711171117aaa7aaa77667796
-- 158:77777777717777777111777761aa7777717996777a11769767aa776777777777
-- 165:77777777711117777aaa17777771a777771a777771a77777711117777aaaa777
-- 166:7777777717777177a1771a777a11a7777711777771aa17771a77a177a7777a77
-- 168:8ee12222dee1122289ee112288ee1111dd9eee11d889eeee88dd99ee88888899
-- 169:22d21eed2dd11ee8dd11ee981111ee9d11eee988eeee9dd8ee998ddd998d8888
-- 170:86ff300388affff0866aaffa86666fff888666af888666888888868688868868
-- 171:a003ff660ffffa680ffaa686fff66888af666888686888888868688888888886
-- 172:776779677711711171aa71a17111716161aa6171717991117a697aaa77677777
-- 173:796777777711777771aa77776177767771777967716966777a77697777777997
-- 174:777777777777777777faaa77770a0a7777faf77a777a77777777777777777777
-- 176:88888888888888888888ee88eeee99e8ee8e88e8e989ee989888998888888888
-- 177:88888888866866886c6666686c66666866666668866666888866688888868888
-- 178:8888888888dddd88888dd88888d88d888df888d88df666d88d6666d888dddd88
-- 181:777777777777733777773a377773a377713a3777771377777471777747777777
-- 182:777777777777777777ddd777777d777777d7d7777df66d777d666d7777ddd777
-- 188:776777777111717771a16177711191777aa17169711171117aaa6aaa77767777
-- 189:767766777111971171a161aa7161717771717196711171767aaa7a7777777777
-- 190:777777777171766771717977711179777aa16967711a77777aa7777777777777
-- 192:7777777777777777707070707777777777777777777777777070707077777777
-- 193:7976797673737373737373737777777776797676737373737373737377777777
-- 194:77777777777777777944944979449449794494497999e9997944e44979444449
-- 195:77777777999999979444449791111197991111197999e9997944e44979444449
-- 196:771111777119911711944411194444411944444119e44e411944444119444441
-- 197:7177771711777711197777411977774119777741197777411977774119777741
-- 198:33333333a333333a33aaaa333a7aa7a33a3aa3a33a7aa7a33a3aa3a33a7aa7a3
-- 199:33333333a333333a333aa3333a7777a337777773377777733777777337777773
-- 208:777777777777777777faaa77770a0a7777faf77a777a77777777777777777777
-- 209:777777777444443474f3f43474f3f44474999994747777747477777477777777
-- 210:7733377777a3a7777333337773333377773337777a3a3a777aaaaa7773333377
-- 211:77aaaa777aaaa1a771a11a177113111771331317713113177133331777313177
-- 212:7777737777333377777337777773733777733377773377777377777777777777
-- 213:777777777777777777777777773337777afaafa7777373777777777777777777
-- 214:77733777733333377f6f6f6336666667366cc66776cccc6776fcfc6377337777
-- 215:777777777777797777ee97e777e97e7777777777777777777777777777777777
-- 216:777777777777737777aa37a777a37a7777777777777777777777777777777777
-- 217:77777777aa16e1a6aa6661aaa616e16a331ee166331ee1363331133333333333
-- 218:7777777777777777777777779999999999999999444444444433334443777734
-- 219:7788377777d3d7777338387778883377778337777a3a3a777aaaaa7773333377
-- 224:33333333a73a773aaa3a733a333333333a773a73aaa73a7333333333a73aa73a
-- 225:33333333aa733a73aaa73a3333333333a73733aa73a73aaa3333333373aa73aa
-- 226:7771777777111777711777711111771177771117777171777117117771117777
-- 227:7711777771177771711777177711771117771711111111177177711771171177
-- 228:7111777777711777171177771771711171771717771111771171777771117777
-- 229:6966696676966667796679666667796699996696669977776369366369696966
-- 230:69666966776677777aa777777a7777a777777777773377773339336369696966
-- 231:777d7d7d77788878777777777dd7dd77788788877777777777add7dd77788788
-- 232:d7777dd78777778777777777dd7dd77d8878877877777777d777777788777778
-- 233:777ddd7777888877777777777777777d777777887777a77777dd777d78887778
-- 234:d7dd77778788777777777777dd7d77d788777787777777777dd7dd7778777777
-- 235:d777dd778777887877777777dd777dd788778877777777777dd777d778877877
-- 237:1122111112111221122112211112112112111121111221222122121111112122
-- 238:1222111111121121122112211122112111122211122211212112212121111121
-- 239:1221111211112222222121211112212122212211221112211112212112221222
-- 240:7777777777777a777777777777777777777777777a777777777777a777777777
-- 241:7777777777777777777777777777777777777777777777777777777777777777
-- 242:7777777777777777777777777a77777777777777777777777777777777777777
-- 243:3737373373773377777777777777777777777777777777777777777777777777
-- 244:7777777777777777777777777777777777777777777777777377737733337373
-- 245:3777777777777777777777773777777733777777777777773377777737777777
-- 246:7777777377777733777777737777777777777733777777377777777377777733
-- 247:3373773337733777777777773777777773777777777777773777777777777777
-- 248:3733733377737773777777337777777777777773777777777777777377777777
-- 249:3777777777777777377777777777777733777777777777773777377733373337
-- 250:7777777377777777777777737777777777777737777777737777777373373733
-- 251:7777777777dd877778d887777dd8dd777dddd87778dddd777888887777777777
-- 252:77777777777777777877877777d7dd7777d777777777dd777788887777777777
-- 253:6966696696966669669969666666966696666696696696696669666669666966
-- 254:6996669669696669666696669669669666669666696666696696969699666966
-- 255:6969666969669696669666699666996669666669666996669966969666966669
-- </TILES>

-- <MAP>
-- 000:0e0e0e1e0e0e0e1e0e1e1e0e1e1e0000000000000000000000000000000000000000000000000e1e1e0e1e1e0e1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 001:0e1f1f1f1f1f1f2f1f2f1f1f4d1e1e00000000000000000000000000000000000000001e0e1e0e2d2d0e1e2d2d0e1e0e000000000000000000000000000000000000000000000000000000000e0e1e1e1e1e1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 002:0e1f1fc9d9e91f59691f1f1f1f1f0e000000000000000000000000000000000000000e0e2c1f1f1f1f1f1e1f1f1f1f1e0e0e000000000000000000000000000000000000000000000000001e0e0e0d1f1f1f0e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 003:1e1f1fcadaea1f5a5b1f1f1f0f2c1e000000000000000000000000000000000000000e1f1f1f1f1f0f1f0e1f1f1f1f1f1f0e1e0e0e0e1e1e1e0e1e1e1e00000000000000000000001e0e0e0e1f1f1f0f1f1f1f1e0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 004:1e1f1fcbdbeb1f6a6b0f1f1f1f1f0e1e0e0e1e0e1e000000000000000000000000000e1f1f1f1f1f1f1f2d1f1f1f1f1f1f1f0e1f1f1f4c1f1f1f1f1f1e0e0e1e1e0e0e1e1e001e1e1e1f1f1f1f4f4f4f4f1f1f1f1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 005:1e2f1f1f1f1f1f1f1f1f1f2f1f1f4c1f1f2f1f1f0e0e1e000000000000000000001e1e1f1f1f1f1f1f1f1f1f1f1f1f1f1f1f5d1f1f1e1e0e0e0e1f2f1f2f1f1f1f1f1f1f1e1e1e1e1f1f1f2fafefefdfef9f4f1f1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 006:1e1f1f1f1f1f2f1f1f1f1f1f1f1f1e0e1e1e0e1f2f1f1e0e0000000000000000000e0e7d1f1f1f1f1f1f1f1f0f3d4e4e1f1e0e0e0e0e0000001e1e1f1f1f1f2f1f0f1f1f1f1f1f0f1f1f1fafefffffefffefff5f0e1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 007:0e1e1e1e1f0f1f1f1f1f7d1f0f1e0e0000001e0e0e1f1f1e1e00000000001e1e0e1e1e0e0e1f0f1f1f1f1f1f1f2e2e1f1f0e00000000000000001e1e1e0e0e7c0e7c0e7c1e1e1f2f1f1f6fefdfdfffefefdfff5f1f1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 008:0000001e0e1e0e0e1e1e0e1f1f1e0000000000001e0e1f1f0e00000000000ead1f1f1d0f1e1e1e0e0e0e1f1f1f3d1f1f1e0e00000000000000000000001e1f1f1f1f2f1f1f1e1f1f1f4fafffffefdfffffef7f1f1f0e0e0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 009:000000000000000000001e0e0e0e000000000000001e2f1f1e1e000000001e1f0d1f1f1f1e2f1f1f1f1e1f1f3e4e1f1e0e0000000000000000000000000e1fadad1fadad1f0e1f1fafdfefefdfdfdfefefdf9f4f4f1f1e0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 010:0000000000000000000000000000000000000000000e1e1f0d1e000000001e1f1f1f1f4d1e1f1f1f0f1e1f1f1f1f1f0e000000000000000000000000001e1f1f1f1f1f1f0f0e1f6fffffffffefefdfefdfefffffdf9f1f0e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 011:000000000000000000000000000000000000000000001e1f1f0e000000000e0f1f1f1f1f0e1d2f1f1f0e1f1f1f1e0e0e000000000000000000000000001e1fadad1fadad1f1e1f1f3f3f3f3f8fefdfefdf7f3f8fefff9f1f1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 012:000000000000000000000000000000000000000000001e1f1f0e0e1e0e1e1e0e1e6c0e1e1e1e1e7c0e0e1f1f1f0e0000000000000000000000000000000e1f0f1f1f2f1f1f0e0e1e1e0e1e1e1f3f3f3f3f0f1f1f8fffff5f1e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 013:000000000000000000000000000000000000000000000e4d1f1f2d1f2d1f2d1f2f1f1f1f1f1f1f1f1f1f1f1f1e1e0000000000000000000000000000001e0d2f1f1f1f1f2e1e00000000001e1e2f1f1f1f1f1f0e1f8fdf9f1f1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 014:000000000000000000000000000000000000000000000e1f1f1f1f1f2f1f1f1f1f1f1f1f0e0e1f1f1e1e0e1e0e000000000000000000000000000000001e1f3d2e9d1f3d3e0e0000000000001e1f1f0f2c0d1f1e0e6fffff5f0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 015:000000000000000000000000000000000000000000001e1f1f0e1e1e1e0e0e0e1e0e0e0e1e0e0e0e0e0000000000000000000000000000000000000000000e1f4e4e3e3e1e000000000000001e1e1e0e1e1e0e1e1e1f8fef5f1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 016:000000000000000000000000000000000000000000001e7c7c1e000000000000000000000000000000000000000000000000000000000000000000000000001e0e0e0e0e0000000000000000000000000000001e0e1fafdf5f0e000000000000000000000000000000000000000000000000000000000000001e0e0e0e1e0e1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 017:0000000000000000000000000000000000000000001e0e1f1f1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1e1e0e1e1e000e1e1fafdfdf5f1e00000000000000000000000000000000000e1e1e0000000000000000000e0e0efededefefe0e0e1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 018:00000000000000000000000000000000000000000e0e1f1f1f1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1e1f4f0f2d1e1e1e1fafefff7f1f0e00000000000000000000000000000000001e1e0e1e000000000000000e1ededefe3d3f3ddedeee0e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 019:000000000000000000000000000000000e0e0e1e0e1f1f1f1f0e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1fafdf5f1f1f1f1f6fdfef7f1f2f1e00000000000000000000000000000000001e2c1f1e0000000000000e1eeefe7f3f1f2e4e3f8fdede1e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 020:00000000000000000000000000000e0e0e1f0d1f1f1f1f2f1f1e0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000e0e1e0e0e0e6fdfdf5f1f4f4f4fafdf7f1f1e1e0e00000000000000000000000000000000000e1e1f1e0000000000001edefe7f1f1f1f1f1f1f0d8fdefe0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 021:0000000000000e0e0e1e0e0e0e0e1eff9f1f1f2f1f1f1f0f1f0e1e000000000000000000000000000000000000000000000000000000000000000000000000000000001e1e1e4e2e1f1f0f6fdfef5fafef6edfefef5f0e0e00000000000000000000000000000000000000000e2f0e1e0e0000001e0ede7f0d1f1f2f1f1f1f1f1f8ffe1e0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 022:000000001e1e1edfdfefffffefdfffdfdf9f2f1f1f2f1f0f1f0e0e00000000000000000000000000000000000000000000000000000000000000000000000000001e1e0e1f2e3e1f4f4f1f1f3f3fafffdf6eefef7f1f1e00000000000000000000000000000000000000000e1e1f1f0d1e0e1e001eeede5f1f1f1f1f1f2f2f1f1f6ffefe1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 023:000000001edfefdfffffefffdfffffffdfdf9f4f1f1f1f1f1f1f0e1e000000000000000000000000000000000000000000000000000000000000000000000000000e1f1f4f1f4e6fffff9f1f1fafefefdf6eef7f2f2c0e00000000000000000000000000000000000000000e1f1f1f1f1f2d0e000eee3d1f1f3e1f1f1f1f1f1f2f3e3dde1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 024:0000001e1effefdfefffefefffefffffffefdfff5f1f1f2f1f0e0e0e000000000000000000000000000000000000000000000000000000000000000000000000001e0fafff5f0f6fffffff5f6fdfef6e6e6edf9f4f4f0e00000000000000000000000000000000000000000e1f1f1f0f1f1f0e000efe3e3e1f1f1f0f1f1f1f1f0d1f6fee0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 025:001e1e1e0e8fefdfffffffff6e6e6e6e6e6e6e6e1f1f1f1f0e1e1f1e000000000000000000000000000000000000000000000000000000000000000000000000001e6fdfef5f0d1f8fff7f1f6fefff6edfdfdfffffff1e00000000000000000000000000000000000000001e1f0f1f1f1f0e1e000ede3d2e1f1f1f1f1f1f1f1f1f3e3dfe1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 026:000e1f1f7d1f8fefefffefdf6effdfffdfffefdf5f1f1f0e0e0f1f0e0e0e00000000000000000000000000000000000000000000000000000000000000000000001e6fefef9f1f1f2f3f1f0f1f8fef6effffdfefffff1e00000000000000000000000000000000000000000e1f1f4e1f2f1e00001edefe5f0d1f0d1f1f2f0d1f1f2edefe0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 027:000e1f1e2f4faf6e6e6e6e6e6edfdfffefefff0e1e1e4c1e1f1f1f1f1f0e00000000000000000000000000000000000000000000000000000000000000000000000e6fffdfff5f1f3e1f1f1f1f1f3f8fffefefdfffef1e00000000000000000000000000000000000000000e0e3e2e1f1f1e00000e1eee9f1f2f1f1f1f1f1f3e1f4ede1e1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 028:001e0f0e1eefffffffefffffffdfdfff1e0e0e0e1f1f1f2f1f1f1f1f2c1e00000000000000000000000000000000000000000000000000000000000000000000000e1f3f8fdf5f3e4e2f4f4f1f1f1f2f3f3f3f3f3f0e1e0000000000000000000000000000000000000000001e0e0e1e0c0e0000001eeefe9f1f1f0d1f1f0f2eaffede0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 029:001e1f1f0e0eefdfffdfffdfff0e0e0e0e0e1f1f1f0f1f1f1f1f1f1f1f0e00000000000000000000000000000000000000000000000000000000000000000000001e1e1e0f3f1f1e1e6fefff5f1e1f0d1f0f1f1f1e1e000000000000000000000000000000000000000000000000000e1f0e0000000e0efeee2e3e1f4e3e3eaffede1e0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 030:001e1e1f0f1e0e1e0e0e0e0e0e1e1f1f1f1f1f1e0e1e1e0e0e2c1f1f1f1e000000000000000000000000000000000000000000000000000000000000000000000000000e1e1e1e0e1e6fdfdf5f1e0e1e0e7c0e0e1e00000000000000000000000000000000000000000000000000000e1c0e000000001e0eeeeefe3d1f3ddeeede1e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 031:00000e1f0f1f1f0c1f1c1f0f1f1f1f1f1e1e1e0e000000001e1e1f2c1f0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000001e0f3f3f2c0e00000e1f1e00000000000000000000000000000000000000000000000000000e1e0e1f1e0e000000001e0e1eeefe1feeee1e1e1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 032:00000e1e0e1e0e0e0e1e0e0e0e0e1e1e0e00000000000000001e0e1e0e1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000001e0e1e0e1e0e00001e3e0e00000000000000000000000000000000000000000000000000000e2d1f1f1e1e0000000000001e1e1e7c0e0e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 033:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e0c1e000000000000000000000000000000000000000000000000001e1e1f1f1f0d0e000000000000000e2d1f2d1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 034:00000000000000000000000000000000000000000000000000000000000000000000001e0e0e1e1e0e0e00000000000000000000000000000000000000000000000000000000000000000000000000000e1f1e000000000000000000000000000000000000000000001e0e1e0e1f1f0f1f1f1e000000000000000e1f1f0d0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 035:0000000000000000000000000000000000000000000000000000000000000000000e0e0eadadadadad1e0e000000000000000000000000000000000000000000000000000000000000000000000000000e1c0e000000000000000000000000000000000000000000000e1e1f1f1f1f1e0e0e0e000000000000001e2d2f2d1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 036:00000000000000000000000000000000000000000000000000000000000000001e1e1e1e1f1f1f1f1f1e0e1e00000000000000000000000000000000000000000000000e1e1e0e1e0e0e0e0e000000000e1f1e0000000000000000000000000000000000000000000e0e2e1f1f1e1e1e000000000000000000001e0f4e3e1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 037:000000000000000000000000000000000000000000000000000000000000001e0e0e0e0e7c7c7c7c7c0e0e1e0e0e0e0e1e0e0e1e000000000000000000000000000e1e0e1f6feefefede5f1e0e1e00000e1f1e0000000000000000000000000000000000000000001e1f0f1f1e1e0000000000000000000000001e2d3e2d0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 038:000000000000000000000000000000000000000000000000000000000000001e1f1f2d2d1f1f1f1f1f1f1f1f2d2d1f1f1f1f1f1e000000000000000000000000001e2f1f1f1f8ffefe7f1f2d1f1e0e1e1e0f0e00000000000000000000000000000000000000000e1e2f4d1f1e1e1e000000000000000000000e0e2e3e1f0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 039:000000000000000000000000000000000000000000000000000000000e0e0e1e0d1f1f1f1f1f1f1f3e3e1f1f1f1f1f1f1f0f1f1e0000000000000000000000000e1e1f3d1f3d2e3f3f1f1f1f1f1f1e0e0e1f1e000000000000000000000000000000000000001e0e1f1f4e2f1f0d0e0e000000000000000e1e1eee4e2e1f0e1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 040:0000000000000000000000000000000000000000000000000000000e1edfefff5f3d1f1f1f3d1f3e4e3d4e3e3e1f2f0f1f1f1f1e0000000000000000000000001e1f2f1f2e2e4e1f1f1f1f1f0f1f1e1f1f1f1e000000000000000000000000000000000000000e2f1f2e2e2f2f1f1f0e0e0e001e0e0e0e1efeeeee5f1f0f1f0e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 041:00000000000000000000000000000000000000000000000000001e0effff0e7f0d0d1f2e1f1f4e4e1f1f3e4e1f1f1f1f1f1f1f1e00000000000000000000001e0e1f1f3d2e3d2e4e3e0f2f1f1f1f1f1f0e1e0e000000000000000000000000000000000000000e2f1f3e3e1f1f0f1f1f5d1e1e1e0d1f6ffeeeee7f1f5d1f2e4e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 042:000000000000000000000000000000000000000000000000000e0eefdf0e1e0d0d1f1f1f2e3e3e1f1f1f1f4e4e1f1f5d1f4d1f0e000000000000000000001e1e1f0f1f3e4e1f1f1f1f1f1f1f1f1f0e1e1e0000000000000000000000000000000000000000000e1f0e0e1e0e1f1f3e1f1f1f1f1f1f4e2e3f3f3f1f4f4f1f4e4e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 043:00000000000000000000000000000000000000000000001e0e0edfef0e1e1e0d1f3d1f3e3e3d1f1f1f3d4d1f2e3e1f1f1f1f1f0e1e0e1e1e0e1e000e0e0e1e1f1f1f1f3d4e3d1f1f1f1f4d0e0e0e1e00000000000000000000000000000000000000000000001e1f0e00000e0e1e4e4e2e1f2f1ffe4e2e5d2f0faffefe3e4e1e0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 044:00000000000000000000000000000000000000000000001e0eefef0e0e000e1e2e1f1f3e1f1f1f1f1f1f1f5d1f1f1f1f0f1f1f1f1f1f1f1f4d0e1e1e0f4c1f1f0e0e4e4e1f1f1f1e0e0e0e0e00000000000000000000000000000000000000000000000000001e1f1e000000000e0e1e1e0e1fdefeeeee1f1fafeede0e0e1e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 045:00000000000000000000000000000000000000000000000effdf0e1e0000000e4e1f2d2d1f1f1f1f1f1f1f1f2d2d1f1f1f1f1f1f1f1f2f1f1f0e0e1f1f1e0e0e1e1e4e1f1f1f1f1e0e1e1e0e0e0e0e0e1e1e0e0e0000000000000000000000000000000000001e1f1e00000000000000000e0e1e1efede4e4efefefe1e0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 046:00000000000000000000000000000000000000000000000eef0e0e000000000e0e0e0e0e7c7c7c7c7c1e1e1e0e1e1f1f1f1f0e0e0e1e1f1f1f0f1f1f0e1e0000000e0e1e1f0f1f1c1f1f0f1f1f1f1f0f0f1f1f0e0000000000000000000000000000000000000e1f0e1e1e0000000000000000001eee4e3efefedeee0e0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 047:00000000000000000000000000000000000000000000001eef0e0000000000000e1e0e0e1f1f1f1f1f1e0e1e000e5d1f1e1e1e00000e1f0f1f1f0e1e0e0000000000001e1e0e0e0e6c0e0e0e1e7c1e1e0e1e7c1e1e00000000000000000000000000000000001e0f1f1f1e0e0e000000000000004efe0e1e0e1e0e0e1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 048:00000000000000000000000000000000000000000000001edf0e000000000000001e0e0eadadadadad1e1e00001e2f1f1e000000000e5d1f1f1f0e000000000000000000000e1f2f1f1f1e0d1f1f2f1e1f1f5d1f0e00000000000000000000000000000000000e2f1f1f1f1f1e000000000000000e0e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 049:000000000000000000000000000000000000000000001e1eff0e0000000000000000000e1e0e1e1e0e0e0000001e1f5d0e000000001e0e0e0e1e1e000000000000000000001e0d1f1f0f0e1d1f1f7d0ead1f1f2c0e00000000000000000000000000000000000e0e1f1f1f1f0e0e1e1e0e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 050:000000000000000000000000000000000000000000001eefff1e000000000000000000000000000000000000000e1f1f0e00000000000000000000000000000000000000000e1e1e1e0e1e1e0e1e1e1e0e0e1e1e0e0000000000000000000000000000000000001e1f1f1f1f1f1f1f4d1f1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 051:000000000000000000000000000000000000000000000eef0e0e00000000000000001e1e0e1e1e0e0e0e0000001e1f1f0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1f2f1f0f1f1f1f2f0f1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 052:000000000000000000000e1e0e0e0e000000000000000eef0e0e0000000000001e0e0e2f1d1d1f0f1f1e0000001e0f1f1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1f1f1f1f1f1f1f1f1f1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 053:000000000000000e0e1e0e4d1f0d1f1e1e0e000000000eefef1e0000000000000e2c1f1f2fad1f1f1f0e0000001e1f1f1e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e0e4d1f1f1f1e0e0e1e0e0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 054:0000000000001e0e1f1f1f1f1f1f0f1f1f0e1e1e00000e0edf0e1e00000000000e1f1f1f1f1f1f1f1e0e000000001e1f1f0e0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1f0f1f1f2f0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 055:00000000001e1e2c1f0f1fef6eff1f1f1f1f7d0e1e0e1e1eef0e0000000000001e1f0f1f1f1f1f2f1f1e000000000e1f1f1f1f1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1f1f1f1f1f0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 056:000000001e0e1f1f1fdfdfff6eefffdfff1f1f1f0e1effefef1e1e00000000001e1e0e0e1f1f0f1e0e0e0000000e0e1f1f1f1f0e1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e0e1f2e3e4e1f0e0000000000000000000000000000000000001e0e1e0e1e1e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 057:0000001e0e1f1f1f1fdfffdf6effffffffdf2f1fffefdf1f1f1f0e1e1e0000000000001e1f0e1e0e0e0000000e0e1f1f0f1f1f1f1e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1f1f1f3e1e1e1e1e0e0e000000000000000000000000000e1e1e2c1f1f1f0d0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 058:0000000e2c1f1f5d1fefefff6edfdfdfdfef1fdfef1f1f1f1f1f1f1f1e0e1e0000000e0e4c1e1f1f1e1e1e1e1e1f0f1f1f1f1f1f1f1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1f1f2f4e4e1f2f1f2e1e0e00000e1e1e1e1e0e0e1e0e0e1e1e0e1f2f1f1f1f1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 059:0000000e1f1f1f1f1f6e6e6e6e6e6effefffefdf1f1f1f1f0f1f1f1f1f2f1e1e1e1e0e1f1f0f1f2f1f1f1f1f1f1f1f1f2f1f1f1f4d0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1f2f1f1e1e0e1e0e2e1f1e1e0e0d1f6d5d6d1f6d5d4e3e1f1e1e7c7c7c7c7c1e0e1e1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 060:0000001e1f1f5d1f1fefffdfffef6effdfdfffdf1f2f1f1f1f1f1f0f1f1f0c1f1c1f0c1f1f1f1f2f1f1f1f0f1f1f1f1e0e0e1f1f1f1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1f1f1f0e0000001e1e1f5d5d5d1f2e1e0e0e1e1e0e4e2e2f0f1f1f1f1f1f1f1f0f1f0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 061:0000000e2c1f1f1f1f1fefffffef6e6e6e6e6e6e1f1f1f1f1f1f1f1f1f1f0e1e1e1e1e7c0e0e1e0e1e1e7c7c1e1e1e0e000e1f1f1f1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e7c7c0e1e000000000e0e0e1e1e0e0e0e000000000e1e1f0d1f1f3d1f3d1f3d3e1f2c0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 062:0000000e1f1f1f0f6d1fefffdfdfefffefff1f1f1f1f0f1f4d0e1e1e1e0e0e0000001e1f1e000000001e1f1f0e0e0000001e0e1c1e0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e0e1e1f1f1e1e0e000000000000000000000000000000001e1e1e1f4e1f1f3e2f1f2e3e1f0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 063:0000001e0e0d1f1f1f1f1f1fefdfffefdf1f1f1f2f1f1f0e0e1e00000000000e1e1e0e1f1e0e0e0e1e1e0c1c0e0e0e00001e0e1f0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e0e0d1f2e3e1f0d1e1e00000000000000000000000000000000001e2c1f4e3e1f1f1f1f3e1f1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 064:000000000e0e1f2f1f1f6d1f1f1fffdf6d1f1f1f0e1e0e1e000000000000001e1d1f2d1f2d1fad1ead1d1f0f2f1d1e0000001e1c0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1f0d2e3e3e1f3e1f1e00000000000000000000000000000000001e1e1e1e0e7c7c7c1e1e0e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 065:00000000000e1e1f0f1f1f1f1f6d6d5d1f1f1f1f1e000000000000000000001e1f1f1f1f1f1fad1e1f1f1f1f1f1f1e0000000e1f0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e0d4e3e9d9d2e3e4e1e0000000000000000000000000000000000000000000e0f1f0d0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 066:0000000000001e0e1e1f1f1f1f5d6d5d1f1f1e1e0e000000000000000000000e0f1f1f1f1f1f1f1e1f0f1f1f1f4d1e0000001e1c0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e3e3e2e4e2e3e0d2e1e0000000000000000000000000000000000000000000e2c1f1f1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 067:00000000000000000e0e0e0e0e0e0e1e0e0e1e0000000000000000000000000e1e0e1e1e1e1e0e0e1e1e1e1e1e1e1e0000000e1f1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1f1f0d3e3e3e1f1f1e0000000000000000000000000000000000000000001e0e1e1e1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 068:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1c1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e3d1fad4e1fad1f3d1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 069:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1f1e0000000e0e1e1e1e1e1e1e1e1e1e000000000000000000000000000000000000000000000000000000000000000e0f1f1f2e1f1f1f0d1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 070:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e0e0e1f1e0000001e0d1f2e2e4f4f4f1f1f1e0e0000000000000000000000000000000000000000000000000000000000001e3d1fad1f1fad1f3d0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 071:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e0e1f3e1f0e0e000e1e1f3e3edeeeeefe3e2e2e1e0000000000000000000000000000000000000000000000000000000000001e1f1f4d1f4e2e3e4e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 072:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1e1efeee3e4e3e1e001e1f1fdeeeeefedefe3e1f1f1e000000000e0e1e1e0e0000000000000000000000000000000000000000001e3d1fad1f2ead1f3d1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 073:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e2eeededeeefe4e0e1e1e2e3eeefede3f2e3f1f1e1e0e000e1e0e0e1fad1d1e1e00000000000000000000000000000000000000001e1f0d1f1f1f1f1f1f1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 074:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e2eeeeeeeeefede5f1f1e1f2eeeee4e1f1f0e1e0e0000000e1d1f1f1f1f1f2c0e00000000000000000000000000000000000000001e3d1fad1f0fad1f3d0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 075:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e3e4e2edeeefefe9f4f4e3e2efe4e4e0e1e1e00000000001ead1f2f1f1f2f1f1e00000000000000000000001e0e1e1e1e000000001e2e1f1f1f1f2e1f0d0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 076:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1e1f4e4e1e0e0e0e1eeefededede4e2e1f1e0e00000000001e1f1f1f1f1f2f1f0e0e0000000e0e1e1e1e1e0e0e1e1f7e0e0e0000000e3d3ead1f4ead1f3d0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 077:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1f3e2e1e0edede2e3f2edeeefe4e1f3e3e1f1e0e1e0000001e1f4d1f1f2f1f1f1f1e00000e1e1f1f1f2d1f2d0dbd8eaebe1e0e00000e1e1f1f1f1f1f1f1e0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 078:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1f2e1e0edeee2e1f6dfefede2e3e1e1e1e2f1f0d1e1e1e1e1e0e0e1f1f1f1f2f1e0e001e1e0e1e1f0f1f1f1f1f1f1fae1f0e1e1e0e001e0e0e7c7c0e1e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 079:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e0e0e1eeede6d3e4efedeeeeede0e1e000e4d1f2f1f1f1f0e1e0e0e0e1e0e7c0e0e0e0e0e0e1e1e0e0e7c7c7c7c7c7c7c7c0e0d1f1e0e00000e1f1f0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 080:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1f6fde3eeeeedede0e0e0e0e0e0e00000e0e0e0e1e1e1f1f1f2f1f1f1f1f1f1f1f1c1f1c1f1c1f1c1f1f1f1f4d1f1f1f1f1f1f1f1f0e0e1e0e3e1f0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 081:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e0e4e3e2e3efeeefeeeeede0e0000000000000000001e1e0e0e0e0e1e1f1f1f0e1e0e1e0e0e1e1e1e1f1fbe1f4f4f4f4f1f1f0f1f1f1f2e2e3e1f1f0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 082:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e2c1f1f1f4e3e2e3f3f3f1e0000000000000000000000001e0e0e0ebd1fbd0e0000000000000e0e0e1e4fafefffffff4d2f1f1f1f1f3e1f1f1f1f0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 083:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e0e1e0e0e1e1f2e1e0e1e0e00000000000000000000000000000e1e1f1f1f1e0000000000000000000effdfffefdfdf9f1f1f1f1f3e2e1f0f1f1e0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 084:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e0e0e0e0000000000000000000000000000000000001e1f2f1f1e0000000000000000001eefffef7f8fdfdf9f4f4f1f1f0f2f4f4f0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 085:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e7e1f1f1e0000000000000000001e0e7f3f1f4f8fefefdfff9f4f4fafefff0e0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 086:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1f1f9e1e000000000000000000001e0d7eafdf2eefdfefefdf6e6effdf7f3f1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 087:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e2f1f1f0e1e0000000000000000001e0e0edfffdfdfffdfefff7f1f3f3f1f1f1e1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 088:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e2f1f0f1f0e0000000000000000001eefdfff7f3f3f8fefff7f4f4f4d4f0d1f2f1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 089:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1e1f1f1f1f0e1e000000000000001e0eef1e0e1e4fafffdfefef6e6eefffdf9f1f0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 090:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1e0e0e1e0e1e0e0e1f1f1f1e1f1f0e000000000000000eefef0e000e1edfdfefefdf7f3f2e4eefefdf0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 091:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000ead1fad1f0e2f1f1f1f1f1e1e1f1f0e1e0000000000000eff0e1e00001e1edfef7f1f1f1f8e4e3e3e1e0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 092:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1f1f1f1f0e1f1f0e0e1e0e1e1e1f2f0e0000000000001edf1e000000000e0e2f1f0d1f0f1f1f1f1e0e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 093:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1f0f2f1f4c1f1f0c1f1d1f2c0e1f1f0e0000000000000eef0e0e000000001e1e0e1f1f1f1f0d1f1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 094:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1f1f1f1f1e1f1f0e1f1f0f1f1e1f1f1e0000000000000eefff1e0000000000001e2d1fbd1e0e1e1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 095:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e2c1f1f7d0e1f1f1e1f1f1f0d1e9e1f0e0000000000001e1eef1e1e1e0000001e0e1f1f1f0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 096:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1e1e0e1e1e1f1f0e1e1e1e1e0e1f1f1e000000000000001edfffef0e0e0e0e0e8ebd1f2d1e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 097:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001ead0fbeae0e2f1f0ead2f1d1d1e0f1f0e000000000000001e1e1eefffdf1e0e7e1f1f0d1f1f0e0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 098:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1f1f8e1f0e1f1f0e1f1f1f1f1e1f1f1e0000000000000000001e0e1eefff0e4f1fbd1fbd1f1f1e1e00000000000000000000001e0e0e1e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 099:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1f0d1f1f1e1f1f1e1f1f1f1f0e1f1f0e00000000000000000000000e0edfefff9f1f1f1f0d1f1f1f1e1e000000000000001e1e1e1fbe1f0e1e1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 100:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1f1f1f1f0e1f1f0e1f0f1f1f1e1e1f1e0000000000000000000000000e1e8fefff9f0d4f4f4f1f0f1f0e0000000000000e0eaeaeae1f1f7e1f0e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 101:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1d0f1f1f0f1f1f4c1f1f1f1fad0e1f0e00000000000000000000000e1ebe1f8fffdfff6e6edf9f1f1f1e1e000000000e0e0d1f1f9e1f2fbfbe1f0e0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 102:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1f1f2f1f0e1f1f0e1f1f1f1f1f1e1f1e00000000000000000000000e1fae8ebe8fefef7f3fefff9fafdf0e0e0000001e1f1f1f1f1f0d1f1f2f9e2c0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 103:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e0d1f1f7d0e0e1e0e1f1f1f0f1f1e1f1e00000000000000000000000e7e1f1f1f6fdf7f1fbeaeefffdfdfdf1e00000e1ebe9e0f1f1f2f1fbe2f9e1f0e1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 104:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e0e0e0e0e0e00001e2c1f9e2c1f0e2f1e00000000000000000000000e1f1f2f1f6f6e9f1f0f1f3f3f8fffdf1e1e1e1e1f1f1f1f1f1f2f1fbe1f1f1f2c1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 105:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1e1e0e1e0e0e7c1e1e1e00000000000000001e0e1f1f1f1f6f6e6e9f1f1f1f2f1f3f3f1f0f1fcfbe1f2f1f1f1f1f1f7e9e0f7e1f1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 106:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e0eae8e1f8ebd1fbd1f0e00000000000000001e1f1fbf1f4d1f8fdfdf7e8e1f1f1f1f1f0e0e0e0ebe1fbe1f1f1f1f1f9eaeae1f2c1e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 107:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1e1e1fbe1f7ebe0fae2fbe1f0e000000000000000e1f1f0f1f1f1f6fffffbe4f1f7d1f0f1f0e00001e1e7e9ebe2f1f1f1f0faeaeae0e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 108:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1f7e8e1f1f1f9e1f2f1fbf1f1e0e0000000000000e1f1f1f2f1f1f6fdfefdfef5f1f1f1f1f1e0000000eae1fbe0d1f1f1f9e1f1f2c0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 109:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e9e8e1f2f8e9ebe1f1f2f1f7e1f0e1e00000000001e1f1f1f1f1f1e1e0e1e1e0e1e1e1e1e1e1e0000000e1e1f2f0f1f7e1f1f0d1f1e0e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 110:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e7e0f2f0d8e8e1f1f0fbe1f8e1f8e1e0e0e1e1e0e1e1f1f1f0e0e1e0000000000000000000000000000000e0e1f9e9e1fbf1fbe1e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 111:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e0e0e7c6c7c7c6c7c0e1e0e1f1f9e1fae1f1f1f2f1f1f1e0e1e0000000000000000000000000000000000001e0e1e9e9e1f0e1e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 112:0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1e0e0f8e9e7eae9ebdbebd1f7e8e8e1f1f0f1f1f1e0e0e000000000000000000000000000000000000000000000e0e1e1e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 113:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e8e1f2f1fbf1f1f2f1f2f1f1e1e0e1e0e0e0e1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 114:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1f7e1f1f1f8ebe1f7e1f7e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 115:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e1e1e0d7e8e9e1f1f8e0d1e1e000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 116:000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e1ebe1f2f7e1f8e1e0e00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- 117:00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e0e0e0e1e1e1e1e0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- </MAP>

-- <FLAGS>
-- 000:f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f080f0800500050005000500060007000ac30b000c000c000c000c000b00094008030800080009000a000a000a00090008c3080007000700060005000500050000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
-- </FLAGS>

-- <PALETTE>
-- 000:140c1c44243430346d4e4a4e854c30346524d04648757161597dced27d2c8595a16daa2cd2aa996dc2cadad45edeeed6
-- </PALETTE>

