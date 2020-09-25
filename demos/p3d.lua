-- title:  3D demo
-- author: by Filippo
-- desc:   3D demo
-- script: lua
-- input:  gamepad

t=0

cx=0
cy=0
cz=200
pivot={x=0,y=0,z=0}
angle=0
points={}
pal={15,14,9,6,8,2,1}

function createCube(t)
	--Init points for cube
	points={}
	local l
	l = 5 + math.abs((math.sin(t/100))*10)
	local p
	for x=-3,3 do
		for y=-3,3 do
			for z=-3,3 do
					p={x=x*l,
								y=y*l,
								z=z*l,
								c=pal[4+z]}
					table.insert(points,p)
			end
		end
	end
end

function createCurve(t)
	--Init points for 3d curve
	points={}
	local i=0
	local l
	l1 = math.abs(5*(math.cos(t/50)))
	for u=-70,70,15 do
		local r1 = math.cos(math.rad(u))*30
		for v=1,360,15 do
			local x = math.sin(math.rad(v))
			local y = math.cos(math.rad(v))
				p={x=x*r1,
							y=y*r1,
							z=(u/(l1+0.7)+v/5),
							c=pal[1+i%7]}		
							table.insert(points,p)
		end	
	i=i+1
	end
end


function p2d(p3d)
	local fov = 180
	local x0 = p3d.x + cx
	local y0 = p3d.y + cy
	local z0 = p3d.z + cz
	local x2d = fov * x0 / z0
	local y2d = fov * y0 / z0
	
	x2d = x2d + 120 --center w
	y2d = y2d + 68  --center h
	
	return x2d,y2d
end

function rotate(p3d,center,ax,ay,az)
	local a,b,c
	local a1,b1,c1
	local a2,b2,c2
	local a3,b2,c3
	local np3d={x=0,y=0,z=0,c=0}

	a = p3d.x-center.x
	b = p3d.y-center.y
	c = p3d.z-center.z
	
	a1 = a*math.cos(az)-b*math.sin(az) 
 b1 = a*math.sin(az)+b*math.cos(az)
	c1 = c

	c2 = c1*math.cos(ay)-a1*math.sin(ay) 	
	a2 = c1*math.sin(ay)+a1*math.cos(ay)
 b2 = b1
	
	b3 = b2*math.cos(ax)-c2*math.sin(ax) 	
	c3 = b2*math.sin(ax)+c2*math.cos(ax)
 a3 = a2		
			
	np3d.x=a3
	np3d.y=b3
	np3d.z=c3
	np3d.c=p3d.c
	return np3d
end

function zsort(p1,p2)
	return p1.z>p2.z
end

function TIC()

	if btn(0) then cy=cy+1 end
	if btn(1) then cy=cy-1 end
	if btn(2) then cx=cx+1 end
	if btn(3) then cx=cx-1 end
	if btn(4) then cz=cz+1 end
	if btn(5) then cz=cz-1 end

	cls(10)
	--Create points
	if(t%900<450) then
	 createCube(t)
	else
	 createCurve(t)
	end
	
	--Rotate points
	for k,p in pairs(points)do
	 pr = rotate(p,pivot,
														angle,
														angle/2,
														angle/4)
		points[k] = pr	
	end		
	
	--Z Sort
	table.sort(points,zsort)
	
	--Draw points
	for k,p in pairs(points)do
			i,j = p2d(p)		
			rect(i,j,6,6,p.c)
	end		
	
	angle = angle + 0.05
	t=t+1
end

-- <TILES>
-- 001:a000000aa0f00f0aaaaaaaaaaafaaa6aafffaaaaaafaa6aaaaaaaaaa33333333
-- 002:a000000aa060060aaaaaaaaaaafaaa6aafffaaaaaafaa6aaaaaaaaaa33333333
-- 016:000000000000000000000000fe96821300000000000000000000000000000000
-- </TILES>

-- <PALETTE>
-- 000:140c1c44243430346d4e4a4e854c30346524d04648757161597dced27d2c8595a16daa2cd2aa996dc2cadad45edeeed6
-- </PALETTE>

