-- title:  fire
-- author: by Filippo
-- desc:   fire demo
-- script: lua
-- input:  gamepad

t=0
x=120
y=120

particle = {}
palette = {14,9,6,3,10,15}

function addParticle(x,y)
 local p = {}
	p.x = x
	p.y = y
	p.dx = math.random(-10.0,10.0)/150.0
	p.dy = math.random(-10.0,-2)/50
	p.t = 0
	table.insert(particle,p)
end

function ticParticle()	
	--print("#"..#particle)
	local s=0
	local s2=0
	local c=0
	for k,p in pairs(particle) do
		p.t = p.t + 1
		s = math.log(p.t / 2.0)
		s2 = s/2.0
		c = palette[math.ceil(p.t/70)]
	 p.x = p.x + p.dx
		p.y = p.y + p.dy

		rect(p.x-s2,p.y-s2,s,s,c)

		--remove old ones
		if p.t > 300 then
			table.remove(particle,k)
		end		
	end		
end

function TIC()
	
	if btn(0) then y=y-1 end
	if btn(1) then y=y+1 end
	if btn(2) then x=x-1 end
	if btn(3) then x=x+1 end	
	
	--warp space
	x = x % 240
	y = y % 136
	
	--reset
	if btn(4) then 
			x = 120
			y = 120
	end
	
	addParticle(x,y)		
	addParticle(30,130)		
 addParticle(210,130)		

	cls(8)

	--Update & Draw particles	
	ticParticle()

	--cursor	
	pix(x,y,7)
	
	print("! FIRE !",94,64)
	t=t+1
end

-- <TILES>
-- 001:a000000aa0f00f0aaaaaaaaaaafaaa6aafffaaaaaafaa6aaaaaaaaaa33333333
-- 002:a000000aa060060aaaaaaaaaaafaaa6aafffaaaaaafaa6aaaaaaaaaa33333333
-- 017:e963af00e963af00e963af00e963af00e963af00e963af00e963af00e963af00
-- </TILES>

-- <PALETTE>
-- 000:140c1c44243430346d4e4a4e854c30346524d04648757161597dced27d2c8595a16daa2cd2aa996dc2cadad45edeeed6
-- </PALETTE>

