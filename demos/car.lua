-- title:  ModelRenderer
-- author: FlamingPandas
-- desc:   Shows 3D models
-- script: lua

point={x=0,y=0,z=300}
fov=100
mesh={} --    Table that contains all the 3D points
polygon={} -- Table that contains all connections of 3D points to create triangle polygons, with their texture specified too
zorder={} --  Table used to know the rendering order of the triangles
rspeed=.1 --  Rotation speed, you can change it 
tspeed=10 --  Translation speed, you can change it

function addMesh(mx,my,mz)
 local m={x=mx,y=my,z=mz,u=0,v=0}
	table.insert(mesh,m)
end

function addPolygon(pp1,pp2,pp3,pu1,pv1,pu2,pv2,pu3,pv3)
 local p={p1=pp1,p2=pp2,p3=pp3,u1=pu1,v1=pv1,u2=pu2,v2=pv2,u3=pu3,v3=pv3,zOrd=0}
	table.insert(polygon,p)
end

function addZOrder(polygon,zor)
 local z={poly=polygon,zo=zor}
 table.insert(zorder,z)
end

function init()
 --"Loading" points and polygon data
 local m=addMesh
	m(-60,-60,5)
	m(60,-60,5)
	m(-60,-100,0)
	m(60,-100,0)
	m(-60,-105,-20)
	m(60,-105,-20)
	m(-60,-90,-30)
	m(60,-90,-30)
	m(-60,95,-30)
	m(60,95,-30)
	m(-60,100,-25)
	m(60,100,-25)
	m(-60,100,0)
	m(60,100,0)
	m(-60,50,5)
	m(60,50,5)
	m(-55,10,30)
	m(55,10,30)
	m(-55,-50,30)
	m(55,-50,30)
	local p=addPolygon
	--UnderNeath Surface
	p(7,8,10, 32,0,48,0,48,16)
	p(7,10,9, 32,0,48,16,32,16)
	--Back Protection
	p(5,6,8, 16,12,32,12,32,16)
	p(5,8,7, 16,12,32,16,16,16)
	--Back lights
	p(3,4,6, 16,8,32,8,32,12)
	p(3,6,5, 16,8,32,12,16,12)
	--Back Capo... maletera
	p(1,2,4, 0,0,16,0,16,8)
	p(1,4,3, 0,0,16,8,0,8)
	--Back WindShield
	p(19,20,2, 48,8,64,8,64,14)
	p(19,2,1, 48,8,64,14,48,14)
	--Roof
	p(20,19,17, 0,7,16,7,16,16)
	p(20,17,18, 0,7,16,16,0,16)
	--Front WindShield
	p(18,15,16, 48,0,64,8,48,8)
	p(18,17,15, 48,0,64,0,64,8)
	--Front Capo
	p(16,13,14, 0,0,16,8,0,8)
	p(16,15,13, 0,0,16,0,16,8)
	--Front Lights
	p(14,13,11, 16,0,32,0,32,4)
	p(14,11,12, 16,0,32,4,16,4)
	--Front Protection
	p(12,11,9, 16,5,32,5,32,8)
	p(12,9,10, 16,5,32,5,16,8)
	--Left WindShield
	p(16,20,18, 64,8,80,0,64,0)
	p(16,2,20, 64,8,80,8,80,0)
	--Right WindShield
	p(15,17,19, 64,8,64,0,80,0)
	p(15,19,1, 64,8,80,0,80,8)
	
 --Right side
	p(4,2,6, 32,24,24,24,32,28)
	p(6,2,8, 32,28,24,24,28,32)
	p(2,16,8, 24,24,10,24,28,32)
	p(8,16,10, 28,32,10,24,1,32)
	p(16,12,10, 10,24,0,31,1,32)
	p(16,14,12, 10,24,0,24,0,31)
	
	--Left side
	p(1,3,5, 24,16,32,16,32,20)
	p(1,5,7, 24,16,32,20,28,24)
	p(15,1,7, 10,16,24,16,28,24)
	p(15,7,9, 10,16,28,24,1,24)
	p(15,9,11, 10,16,1,24,0,23)
	p(15,11,13, 10,16,0,23,0,16)
	for i,p in pairs(polygon) do
	 addZOrder(i,0)
	end
end
init()

mode=1

function TIC()
 if btn(6) then
	 if btn(0) then translate(0,-tspeed,0) end
	 if btn(1) then translate(0,tspeed,0) end
	 if btn(2) then translate(-tspeed,0,0) end
	 if btn(3) then translate(tspeed,0,0) end
	else
	 if btn(0) then rotatateX(-rspeed) end
	 if btn(1) then rotatateX(rspeed) end
	 if btn(2) then rotatateY(-rspeed) end
	 if btn(3) then rotatateY(rspeed) end
	end
	if btn(4) then if fov<800 then fov=fov+10 end end
	if btn(5) then if fov>10 then fov=fov-10 end end
 if btnp(7) then if mode<3 then mode=mode+1 else mode=1 end end
	cls(13)
	--Shows info from the zorder table
-- for i,z in pairs(zorder) do
--	 print(z.poly.." - "..z.zo,10,10*i)
-- end
	calculateMeshUV()
	calculateZOrder()
	table.sort(zorder,function(a,b) return a.zo > b.zo end)

	if mode==1 then showMesh() end
 if mode==2 then	showWireFrame() end
	if mode==3 then showMesh() showWireFrame() end
end

--Given this 3D points calculate how they would look in a 2D screen
function calculateMeshUV()
 for i,m in pairs(mesh) do
	m.u=((m.x+point.x)*(fov/(m.z+point.z)))+120
	m.v=((m.y+point.y)*(fov/(m.z+point.z)))+70
	end
end

-- I made this up but not works... it gives the average of the depth of the three points of each triangle, it works good enough to know witch triangles to draw first (the further ones)
function calculateZOrder()
 m=mesh
 for i,p in pairs(polygon) do
	 p.zord=(m[p.p1].z+m[p.p2].z+m[p.p3].z)/3
	 updateZOrder(i,p.zord)
	end
end

-- Once calculated the zorder you need to update the zorder table with those values
-- Why do we need this values in another table? well... lua can sort this table and now we have the order in witch to draw the triangles
function updateZOrder(polygon,zor)
 for i,z in pairs(zorder) do
	 if z.poly==polygon then
	  z.zo=zor
		end
	end
end

-- Using the textri function you can show triangles with textures
-- This function draws all triangles that are facimg the camera with the textures defined on the init function
-- The function draws them in the zorder from further to closest so when a triangle is closer it shows "Above" othee triangles
function showMesh()
 m=mesh
	for i,z in pairs(zorder) do
	 p=polygon[z.poly]
		-- If the triangle points are clockwise they are facing the camera
		if isClockWise(p) then
		 --Draw the textured triangle
   ttri(m[p.p1].u,m[p.p1].v,
   		m[p.p2].u,m[p.p2].v,
   		m[p.p3].u,m[p.p3].v,
   		p.u1,p.v1,
   		p.u2,p.v2,
   		p.u3,p.v3,
   		0,{},
   		(m[p.p1].z+point.z), 
   		(m[p.p2].z+point.z), 
   		(m[p.p3].z+point.z))
		end
 end
end

-- Given a triangle return true if it's points are clockwise false if they aren't
function isClockWise(poly)
 p=poly
	m=mesh
 if ((m[p.p2].v-m[p.p1].v)*
	(m[p.p3].u-m[p.p2].u))-
	((m[p.p3].v-m[p.p2].v)*
	(m[p.p2].u-m[p.p1].u))>0 then
	 return false
	else
	 return true
	end
end

--Function that draws all lines in the three points of a triangle (it just shpws the WireFrame and also the individual points)
function showWireFrame()
 m=mesh
 for i,p in pairs(polygon) do
		line(m[p.p1].u,m[p.p1].v,m[p.p2].u,m[p.p2].v,15)
		line(m[p.p2].u,m[p.p2].v,m[p.p3].u,m[p.p3].v,15)
		line(m[p.p3].u,m[p.p3].v,m[p.p1].u,m[p.p1].v,15)
	end
	for i,m in pairs(mesh) do
	 circ(m.u,m.v,1,0)
	 print(i,m.u,m.v+5,3)
	end
end

-- Rotates all pf the points in the mesh by an angle in the x axes
function rotatateX(angle)
 cos=math.cos(angle)
	sin=math.sin(angle)
	for i,p in pairs(mesh) do
	 y=p.y*cos-p.z*sin
		z=p.z*cos+p.y*sin
		p.y=y
		p.z=z
	end
end

function rotatateY(angle)
 cos=math.cos(angle)
	sin=math.sin(angle)
	for i,p in pairs(mesh) do
	 x=p.x*cos-p.z*sin
		z=p.z*cos+p.x*sin
		p.x=x
		p.z=z
	end
end

function rotatateZ(angle)
 cos=math.cos(angle)
	sin=math.sin(angle)
	for i,p in pairs(mesh) do
	 x=p.x*cos-p.y*sin
		y=p.y*cos+p.x*sin
		p.x=x
		p.y=y
	end
end

-- Translate all of the points an x,y and z distance
function translate(dx,dy,dz)
		point.x=point.x+dx
		point.y=point.y+dy
		point.z=point.z+dz
end
-- <TILES>
-- 000:66666ff666666ff666666ff666666ff666666ff666666ff666666ff666666ff6
-- 001:6ff666666ff666666ff666666ff666666ff666666ff666666ff666666ff66666
-- 002:66666ff66ee373736ee737376666666663333333633333336333333363333333
-- 003:6ff6666673737ee637373ee66666666633333336333333363333333633333336
-- 004:63337733633377336333377363333773633aaaa3633aaaa3633aaaa3633aaaa3
-- 005:337733363377333637733336377333363aaaa3363aaaa3363aaaa3363aaaa336
-- 006:66666ff66888dd33688ddd8868ddd8886ddd888d6dd888dd6d00000d66666ff6
-- 007:6ff6666633ddd8868ddd8886ddd88886dd888886d8888886800000866ff66666
-- 008:6666666668888888688888886888888868888888688888886888888866666666
-- 009:6666666688888886888888868888888688888886888888868888888666666666
-- 010:6666666666666666666666666666666666666666666666666666666666666666
-- 011:6666666666666666666666666666666666666666666666666666666666666666
-- 016:66666f00666660ff66660ff066660fff66660fff666660ff66666f0066666ff6
-- 017:00f66666ff0666660ff066660ff066660ff06666ff06666600f666666ff66666
-- 018:66666ff669996faa69996faa666600f663337733633377336333773363337733
-- 019:6ff66666aaf69996aaf699966f00666633773336337733363377333633773336
-- 020:6333337363333373633337336333373363333733633337336333333363333333
-- 021:3733333637333336337333363373333633733336337333363333333633333336
-- 022:66666ff66888ddd8688ddd8868ddd8886dd0000066666ff60000000000000000
-- 023:6ff6666688ddd8868ddd8886ddd88886dd8888866ff666660000000000000000
-- 026:6666666666666666666666666666666666666666666666666666666666666666
-- 027:6666666666666666666666666666666666666666666666666666666666666666
-- 032:6666666666666666666666666666666666666666666666336666630066663000
-- 033:6666666666666666666006666666666666666666336666660036666600036666
-- 034:6666666666666666666666666666666666666666666666336666630066663000
-- 035:6666666666666666666666666666666666666666336666660036666600036666
-- 048:6666666666666666666666666666666666666666666666336666630066663000
-- 049:6666666666666666666006666666666666666666336666660036666600036666
-- 050:6666666666666666666666666666666666666666666666336666630066663000
-- 051:6666666666666666666666666666666666666666336666660036666600036666
-- </TILES>

-- <PALETTE>
-- 000:140c1c44243430346d4e4a4e854c30346524d04648757161597dceb23c408595a16daa2cd2aa996dc2cadad45edeeed6
-- </PALETTE>
