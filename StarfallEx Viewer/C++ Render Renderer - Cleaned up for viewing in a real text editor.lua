--[[
	Cleaned up version of the actual SF script designed for viewing in a real text editor
	You still need to use the txt in your starfall data folder though

	If you're using vscode I recommend getting the GLua syntax extension,
	it wont match Starfall's syntax exactly, but it'll do better than plain Lua highlighting
]]

--@name C++ Render Renderer
--@author Derpius
--@shared

IMAGE = {pixels = "", width = 0, height = 0}
SAMPLES = 0
RENDERTIME = 0
DENOISETIME = 0
scale = 1

if SERVER then
	net.receive("header", function()
		SAMPLES = net.readInt(32)
		RENDERTIME = net.readFloat()
		DENOISETIME = net.readFloat()
		IMAGE.width = net.readInt(32)
		IMAGE.height = net.readInt(32)
	end)
	
	net.receive("p", function()
		net.readStream(function(data)
			IMAGE.pixels = data
			
			timer.create("cooldown", 1, 1, function()
				print("Render received, distributing...")
				net.start("d")
				net.writeInt(SAMPLES, 32)
				net.writeFloat(RENDERTIME)
				net.writeFloat(DENOISETIME)
				net.writeInt(IMAGE.width, 32)
				net.writeInt(IMAGE.height, 32)
				net.writeStream(IMAGE.pixels)
				net.send()
			end)
		end)
	end)
end

if CLIENT then
render.createRenderTarget("rt")

hook.add("render", "drawRTToScreen", function()
	render.setRenderTargetTexture("rt")
	if IMAGE.width >= IMAGE.height then
		render.drawTexturedRect(0, (1024 - IMAGE.height*scale)/4, 512, 512)
	else
		render.drawTexturedRect((1024 - IMAGE.width*scale)/4, 0, 512, 512)
	end
	
	render.setColor(Color(10, 10, 10, 255))
	render.drawRect(0, 0, 350, 45)
	
	render.setColor(Color(255))
	render.drawText(10, 10, string.format("Resolution: %ix%i, Samples: %i", IMAGE.width, IMAGE.height, SAMPLES))
	render.drawText(10, 25, string.format("Render Time: %f, Denoise Time: %f", RENDERTIME, DENOISETIME))
end)

function yield(percent)
	percent = percent or 0.2
	if quotaTotalUsed()/quotaMax() >= percent or quotaTotalAverage()/quotaMax() >= percent then coroutine.yield() end
end

function getPixel(x, y)
	local pntr = (y*IMAGE.width + x)*3 + 1
	return Vector(string.byte(IMAGE.pixels, pntr, pntr + 2))/255
end

function drawRenderToRT()
	render.setColor(Color(0, 0, 0))
	render.drawRectFast(0, 0, 1024, 1024)
	render.setColor(Color(255, 255, 255))

	scale = 1
	if IMAGE.width >= IMAGE.height then
		scale = 1024/IMAGE.width
	else
		scale = 1024/IMAGE.height
	end
	
	for y = 1, IMAGE.height do
		for x = 1, IMAGE.width do
			yield(0.6)
			--if SCENE[y][x].x > 1 or SCENE[y][x].y > 1 or SCENE[y][x].z > 1 then throw("invalid colour") end
			local colour = getPixel(x - 1, y - 1):getColor()
			render.setColor(colour)
			render.drawRect((x - 1)*scale, (y - 1)*scale, scale, scale)
		end
	end
	
	if player() == owner() then
		print("Sending render to server...")
		
		net.start("header")
		net.writeInt(SAMPLES, 32)
		net.writeFloat(RENDERTIME)
		net.writeFloat(DENOISETIME)
		net.writeInt(IMAGE.width, 32)
		net.writeInt(IMAGE.height, 32)
		net.send()
		
		timer.create("startUpload", 1, 1, function()
			net.start("p")
			net.writeStream(IMAGE.pixels)
			net.send()
		end)
	end
end

function drawImage()
	co = coroutine.create(drawRenderToRT)
	
	hook.add("renderoffscreen", "drawToRT", function()
		if coroutine.status(co) == "suspended" then
			render.selectRenderTarget("rt")
			coroutine.resume(co)
			render.selectRenderTarget()
		elseif coroutine.status(co) == "dead" then
			hook.remove("renderoffscreen", "drawToRT")
		end
	end)
end

if player() == owner() then
	function parsePPM(path)
		local data = file.read(path)
		if not data then throw("Failed to read PPM file "..path) end
		
		if string.sub(data, 1, 2) ~= "P6" then throw("PPM reader requires P6 format") end
		local dims = string.split(string.split(data, "\n")[2], " ")
		return {
			pixels = table.concat(string.split(data, "\n"), "\n", 4),
			width = tonumber(dims[1]),
			height = tonumber(dims[2])
		}
	end
	
	-- Called by GLua when a new render has been placed in sf_filedata
	function onRender(renderTime, denoiseTime, samples)
		IMAGE = parsePPM("denoised.ppm")
		RENDERTIME = renderTime
		DENOISETIME = denoiseTime
		SAMPLES = samples
		
		drawImage()
	end
else
	net.receive("d", function()
		SAMPLES = net.readInt(32)
		RENDERTIME = net.readFloat()
		DENOISETIME = net.readFloat()
		IMAGE.width = net.readInt(32)
		IMAGE.height = net.readInt(32)
		
		net.readStream(function(data)
			IMAGE.pixels = data
			drawImage()
		end)
	end)
end
end