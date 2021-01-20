if SERVER then
	AddCSLuaFile()
else
	print("=====LOADING BINARY=====")
	require("Pathtracer")
	print("=====BINARY LOADED=====")

	PLR = LocalPlayer()
	OBJS = {}
	LIGHTS = {}
	SELECTED_MAT = nil
	SELECTED_LIGHT = nil
	MATS = {
		DEFAULTS = {
			perfectDiffuse = {
				roughness = 1,
				metalness = 0,
				emission = 0,
				ior = 1.5
			},
			perfectMirror = {
				roughness = 0,
				metalness = 0,
				emission = 0,
				ior = 1.5
			},
			emissiveLight = {
				roughness = 1,
				metalness = 0,
				emission = 50,
				ior = 1.5
			}
		}
	}
	LIGHT_TYPES = {
		DEFAULTS = {
			fallback = {
				colour = Vector(1, 1, 1),
				intensity = 150
			}
		}
	}
	HDRI = nil

	IS_RENDERING = false

	STARFALL = nil

	-- Denoiser vars
	KERNEL_SIZE = 3
	NORMAL_THRESH = 60
	ALBEDO_THRESH = 0.05
	DEPTH_THRESH = 10

	local unitMatrix = {
		{1, 0, 0},
		{0, 1, 0},
		{0, 0, 1},
	}

	local function GModMatrixToCPP(matrix)
		local ang = {
			{0, 0, 0},
			{0, 0, 0},
			{0, 0, 0},
		}
		local invAng = {
			{0, 0, 0},
			{0, 0, 0},
			{0, 0, 0},
		}

		local nonZero = false
		for row = 1, 3 do
			for col = 1, 3 do
				local cell = matrix:GetField(row, col)
				if cell != 0 then nonZero = true end

				ang[row][col] = cell
				invAng[col][row] = cell
			end
		end

		if not nonZero then
			ang, invAng = unitMatrix, unitMatrix
		end

		return ang, invAng
	end

	local function PassEntityToCPP(ent, mat)
		mat = mat or {
			roughness = 0,
			emission = 0,
			ior = 1.5
		}

		local meshes = util.GetModelMeshes(ent:GetModel())
		local materials = ent:GetMaterials()

		-- trim the mesh data for less crashing potential
		for i = 1, #meshes do
			for k, v in pairs(meshes[i]) do
				if k ~= "material" and k ~= "triangles" then
					meshes[i][k] = nil
				end
			end

			for j = 1, #meshes[i].triangles do
				for k, v in pairs(meshes[i].triangles[j]) do
					if k ~= "pos" and k ~= "normal" and k ~= "u" and k ~= "v" and k ~= "tangent" and k ~= "binormal" then
						meshes[i].triangles[j][k] = nil
					end
				end

				if not meshes[i].triangles[j].binormal then
					meshes[i].triangles[j].binormal = meshes[i].triangles[j].tangent:Cross(meshes[i].triangles[j].normal)
				end
			end

			local subMat = Material(ent:GetSubMaterial(i - 1) == "" and materials[i] or ent:GetSubMaterial(i - 1))
			
			meshes[i].material = {
				base = subMat:GetString("$basetexture") and string.Replace(subMat:GetString("$basetexture"), "/", "\\")..".ppm" or "missingtexture.ppm",
				normal = subMat:GetString("$bumpmap") and string.Replace(subMat:GetString("$bumpmap"), "/", "\\")..".ppm" or ""
			}
		end

		local ang, invAng = GModMatrixToCPP(ent:GetWorldTransformMatrix())
		local colour = ent:GetColor()

		CreateEntity(
			meshes,
			ang,
			invAng,
			ent:GetPos(),
			Vector(colour.r, colour.g, colour.b)/255,
			colour.a/255,
			mat.roughness,
			mat.emission,
			mat.ior,
			mat.metalness,
			ent:EntIndex()
		)
	end

	local function PassLightToCPP(ent, profile)
		CreateLight(ent:GetPos(), profile.colour, profile.intensity, ent:EntIndex())
	end

	local function DoTrace()
		ResetTracer()

		if HDRI then
			if not SelectHDRI("D:\\Steam\\steamapps\\common\\GarrysMod\\garrysmod\\data\\sf_filedata\\hdri\\"..HDRI) then
				PLR:ChatPrint("Failed to load HDRI")
			end
		else
			DeselectHDRI()
		end

		MoveCamera(PLR:EyePos())
		
		local matrix = Matrix()
		matrix:SetAngles(PLR:EyeAngles())
		local ang, _ = GModMatrixToCPP(matrix)

		AngleCamera(ang)

		SetFOV(80)

		local invalid = {}
		for k, v in pairs(OBJS) do
			if not k:IsValid() then invalid[k] = true; continue end
			PassEntityToCPP(k, MATS[v] or MATS.DEFAULTS[v])
		end

		for k, v in pairs(LIGHTS) do
			if not k:IsValid() then invalid[k] = true; continue end
			PassLightToCPP(k, LIGHT_TYPES[v] or LIGHT_TYPES.DEFAULTS[v])
		end

		for k, _ in pairs(invalid) do
			OBJS[k] = nil
			LIGHTS[k] = nil
		end

		if StartRender(KERNEL_SIZE, NORMAL_THRESH, ALBEDO_THRESH, DEPTH_THRESH) then
			PLR:ChatPrint("Render started")
			IS_RENDERING = true
		else
			PLR:ChatPrint("Render already in progress")
		end
	end

	hook.Add("Think", "pollCompletion", function()
		local completed, renderTimeNS, denoiseTimeNS = PollCompletion()
		if not completed then return end

		IS_RENDERING = false

		PLR:ChatPrint("Done!")
		PLR:ChatPrint("Completed in "..tostring(renderTimeNS / 1e9).." seconds.")
		PLR:ChatPrint("Denoised in "..tostring(denoiseTimeNS / 1e9).." seconds.")

		if STARFALL and STARFALL:IsValid() and STARFALL.instance then
			STARFALL.instance.env.onRender(renderTimeNS / 1e9, denoiseTimeNS / 1e9, GetSamples())
		end
	end)

	function markLight(ent)
		if not IsValid(ent) or ent:IsPlayer() then
			PLR:ChatPrint("Light invalid")
			return
		end
		
		if LIGHTS[ent] then
			PLR:ChatPrint("Light already marked!")
			return
		end
		
		LIGHTS[ent] = SELECTED_LIGHT or "fallback"
		PLR:ChatPrint(tostring(ent).." marked as light with profile "..(SELECTED_LIGHT or "fallback").."!")
	end
	
	function markProp(ent)
		if not IsValid(ent) or ent:IsPlayer() then
			PLR:ChatPrint("Prop invalid")
			return
		end
		
		if OBJS[ent] then
			PLR:ChatPrint("Prop already marked!")
			return
		end
		
		OBJS[ent] = SELECTED_MAT or "perfectDiffuse"
		PLR:ChatPrint(tostring(ent).." marked for tracing with material "..(SELECTED_MAT or "perfectDiffuse").."!")
	end

	local downKeys = {}
	hook.Add("PlayerButtonDown", "onKeypress", function(plr, key)
		if plr ~= PLR or downKeys[key] then return end
		downKeys[key] = true
		key = input.GetKeyName(key)
		
		if key == "l" then
			markLight(plr:GetEyeTrace().Entity)
		elseif key == "p" then
			markProp(plr:GetEyeTrace().Entity)
		elseif key == "DEL" then
			local ent = plr:GetEyeTrace().Entity
			if LIGHTS[ent] then LIGHTS[ent] = nil; PLR:ChatPrint("Light deleted") end
			if OBJS[ent] then OBJS[ent] = nil; PLR:ChatPrint("Prop deleted") end
		end
	end)

	hook.Add("PlayerButtonUp", "onKeypress", function(plr, key)
		if plr == PLR and downKeys[key] then downKeys[key] = false end
	end)

	hook.Add("EntityRemoved", "deleteEntsAutomatically", function(ent)
		if LIGHTS[ent] then LIGHTS[ent] = nil; print("Light deleted") end
		if OBJS[ent] then OBJS[ent] = nil; print("Prop deleted") end
	end)

	COMMANDS = {
		clearProps = {
			desc = "removes all marked props an lights",
			func = function()
				OBJS = {}
				LIGHTS = {}
				PLR:ChatPrint("Props and lights cleared")
			end
		},
		selectHDRI = {
			desc = "changes which HDRI the tracer should use, pass nothing to not use a HDRI",
			func = function(path)
				if not path then
					HDRI = nil
					PLR:ChatPrint("HDRI removed")
					return
				end

				if string.sub(path, -4) ~= ".pfm" then
					PLR:ChatPrint("Invalid file format, not a .pfm")
					return
				end
			
				if not file.Exists("sf_filedata/hdri/"..path, "DATA") then
					PLR:ChatPrint("File ".."sf_filedata/hdri/"..path.." does not exist")
					return
				end
				
				HDRI = path
				PLR:ChatPrint("HDRI selected")
			end
		},
		listHDRIs = {
			desc = "lists all HDRIs",
			func = function()
				PLR:ChatPrint("===== The following HDRIs are available to use =====")
				local files, _ = file.Find("sf_filedata/hdri/*.pfm", "DATA")
				for i = 1, #files do
					PLR:ChatPrint(files[i])
				end
				PLR:ChatPrint("====================================================")
			end
		},
		setRes = {
			desc = "sets the resolution of the camera, takes 'resX resY'",
			func = function(resX, resY)
				resX, resY = tonumber(resX), tonumber(resY)
				if not resX or not resY then
					PLR:ChatPrint("Arguments not numbers")
					return
				end
				
				if resX < 2 or resY < 2 then
					PLR:ChatPrint("Invalid resolution, minimum is 2x2")
					return
				end
				
				SetResolution(math.floor(resX), math.floor(resY))
				PLR:ChatPrint("Resolution set")
			end
		},
		setSamples = {
			desc = "sets the samples to use",
			func = function(samples)
				samples = tonumber(samples)
				if not samples then
					PLR:ChatPrint("Argument not a number")
					return
				end
				
				if samples < 1 then
					PLR:ChatPrint("Invalid number of samples, minimum is 1")
					return
				end
			
				SetSamples(math.floor(samples))
				PLR:ChatPrint("Samples set")
			end
		},
		hdriBrightness = {
			desc = "sets the brightness of the hdri used internally",
			func = function(brightness)
				brightness = tonumber(brightness)
				if not brightness then
					PLR:ChatPrint("Argument not a number")
					return
				end
			
				SetHDRIBrightness(brightness)
				PLR:ChatPrint("Brightness set")
			end
		},
		hdriBrightnessView = {
			desc = "sets the brightness of the hdri used in the viewport",
			func = function(brightness)
				brightness = tonumber(brightness)
				if not brightness then
					PLR:ChatPrint("Argument not a number")
					return
				end
			
				SetViewHDRIBrightness(brightness)
				PLR:ChatPrint("Brightness set")
			end
		},
		startRender = {
			desc = "starts the render",
			func = function()
				DoTrace()
			end
		},
		createMat = {
			desc = "creates a new material for use in the render",
			numArgs = 5,
			func = function(name, roughness, metalness, emission, ior)
				if name == "DEFAULTS" then
					PLR:ChatPrint("Protected name")
					return
				end

				if MATS[name] then
					PLR:ChatPrint("Material already created, please delete first with /deleteMat "..name)
					return
				end

				if MATS.DEFAULTS[name] then
					PLR:ChatPrint("Material already exists as a default")
					return
				end

				roughness, metalness, emission, ior = tonumber(roughness), tonumber(metalness), tonumber(emission), tonumber(ior)
				if not roughness or not metalness or not emission or not ior then
					PLR:ChatPrint("Invalid argument types")
					return
				end

				MATS[name] = {roughness = roughness, metalness = metalness, emission = emission, ior = ior}
				PLR:ChatPrint("Material '"..name.."' created")
			end
		},
		modifyMat = {
			desc = "modifies an existing material",
			numArgs = 5,
			func = function(name, roughness, metalness, emission, ior)
				if name == "DEFAULTS" then
					PLR:ChatPrint("Protected name")
					return
				end

				if MATS.DEFAULTS[name] then
					PLR:ChatPrint("You cannot edit default materials")
					return
				end

				if not MATS[name] then
					PLR:ChatPrint("Material does not exist")
					return
				end

				roughness, metalness, emission, ior = tonumber(roughness), tonumber(metalness), tonumber(emission), tonumber(ior)
				if not roughness or not metalness or not emission or not ior then
					PLR:ChatPrint("Invalid argument types")
					return
				end

				MATS[name] = {roughness = roughness, metalness = metalness, emission = emission, ior = ior}
				PLR:ChatPrint("Material '"..name.."' modified")
			end
		},
		deleteMat = {
			desc = "deletes a material",
			numArgs = 1,
			func = function(name)
				if name == "DEFAULTS" then
					PLR:ChatPrint("Protected name")
					return
				end

				if not MATS[name] then
					PLR:ChatPrint("Material does not exist")
					return
				end

				MATS[name] = nil
				PLR:ChatPrint("Material '"..name.."' deleted")
			end
		},
		listMats = {
			desc = "lists all available materials, including default ones",
			func = function(name)
				for k, v in pairs(MATS.DEFAULTS) do
					PLR:ChatPrint("DEF "..k..": roughness "..tostring(v.roughness)..", Emission "..tostring(v.emission)..", IoR "..tostring(v.ior))
				end

				for k, v in pairs(MATS) do
					if k == "DEFAULTS" then continue end
					PLR:ChatPrint(k..": roughness "..tostring(v.roughness)..", Emission "..tostring(v.emission)..", IoR "..tostring(v.ior))
				end
			end
		},
		setMat = {
			desc = "sets material to apply to newly created entities (change an existing entity's material by deleting then recreating it)",
			func = function(name)
				if name == "DEFAULTS" or (not MATS.DEFAULTS[name] and not MATS[name]) then
					PLR:ChatPrint("Unknown material '"..name.."'")
					return
				end

				SELECTED_MAT = name
				PLR:ChatPrint("Selected material '"..name.."'")
			end
		},
		createLight = {
			desc = "creates a new lighting profile for use in the renderer",
			numArgs = 5,
			func = function(name, r, g, b, intensity)
				if name == "DEFAULTS" then
					PLR:ChatPrint("Protected name")
					return
				end

				if LIGHT_TYPES[name] then
					PLR:ChatPrint("Profile already created, please delete first with /deleteLight "..name)
					return
				end

				if LIGHT_TYPES.DEFAULTS[name] then
					PLR:ChatPrint("Profile already exists as a default")
					return
				end

				r, g, b, intensity = tonumber(r), tonumber(g), tonumber(b), tonumber(intensity)
				if not r or not g or not b or not intensity then
					PLR:ChatPrint("Invalid argument types")
					return
				end

				LIGHT_TYPES[name] = {
					colour = Vector(
						math.Clamp(r, 0, 255)/255,
						math.Clamp(g, 0, 255)/255,
						math.Clamp(b, 0, 255)/255
					),
					intensity = intensity
				}
				PLR:ChatPrint("Light profile '"..name.."' created")
			end
		},
		modifyLight = {
			desc = "modifies an existing light profile",
			numArgs = 5,
			func = function(name, r, g, b, intensity)
				if name == "DEFAULTS" then
					PLR:ChatPrint("Protected name")
					return
				end

				if LIGHT_TYPES.DEFAULTS[name] then
					PLR:ChatPrint("You cannot edit default profiles")
					return
				end

				if not LIGHT_TYPES[name] then
					PLR:ChatPrint("Profile doesn't exist, please create first with /createLight "..name)
					return
				end

				r, g, b, intensity = tonumber(r), tonumber(g), tonumber(b), tonumber(intensity)
				if not r or not g or not b or not intensity then
					PLR:ChatPrint("Invalid argument types")
					return
				end

				LIGHT_TYPES[name] = {
					colour = Vector(
						math.Clamp(r, 0, 255)/255,
						math.Clamp(g, 0, 255)/255,
						math.Clamp(b, 0, 255)/255
					),
					intensity = intensity
				}
				PLR:ChatPrint("Light profile '"..name.."' modified")
			end
		},
		deleteLight = {
			desc = "deletes a light profile",
			numArgs = 1,
			func = function(name)
				if name == "DEFAULTS" then
					PLR:ChatPrint("Protected name")
					return
				end

				if not MATS[name] then
					PLR:ChatPrint("Profile does not exist")
					return
				end

				LIGHT_TYPES[name] = nil
				PLR:ChatPrint("Light profile '"..name.."' deleted")
			end
		},
		listLights = {
			desc = "lists all available light profiles, including default ones",
			func = function(name)
				for k, v in pairs(LIGHT_TYPES.DEFAULTS) do
					PLR:ChatPrint("DEF "..k..": Colour "..tostring(v.colour * 255)..", Intensity "..tostring(v.intensity))
				end

				for k, v in pairs(LIGHT_TYPES) do
					if k == "DEFAULTS" then continue end
					PLR:ChatPrint(k..": Colour "..tostring(v.colour * 255)..", Intensity "..tostring(v.intensity))
				end
			end
		},
		setLight = {
			desc = "sets light profile to apply to newly created lights (change an existing light's profile by deleting then recreating it)",
			func = function(name)
				if name == "DEFAULTS" or (not LIGHT_TYPES.DEFAULTS[name] and not LIGHT_TYPES[name]) then
					PLR:ChatPrint("Unknown profile '"..name.."'")
					return
				end

				SELECTED_LIGHT = name
				PLR:ChatPrint("Selected light profile '"..name.."'")
			end
		},
		kernelSize = {
			desc = "sets the radius of the kernels to use when denoising",
			func = function(size)
				if not tonumber(size) then
					PLR:ChatPrint("Input not a number")
					return
				end

				KERNEL_SIZE = tonumber(size)
				PLR:ChatPrint("Kernel size set")
			end
		},
		normalThresh = {
			desc = "sets the maximum angle between normals at which they won't be blurred (in degrees)",
			func = function(ang)
				if not tonumber(ang) then
					PLR:ChatPrint("Input not a number")
					return
				end

				NORMAL_THRESH = math.Clamp(tonumber(ang), 0, 90)
				PLR:ChatPrint("Normal threshold set")
			end
		},
		albedoThresh = {
			desc = "sets the maximum distance between colours at which they won't be blurred (0-1)",
			func = function(dist)
				if not tonumber(dist) then
					PLR:ChatPrint("Input not a number")
					return
				end

				ALBEDO_THRESH = math.Clamp(tonumber(dist), 0, 1)
				PLR:ChatPrint("Albedo threshold set")
			end
		},
		depthThresh = {
			desc = "sets the maximum difference in depth at which pixels won't be blurred (in hammer units)",
			func = function(dist)
				if not tonumber(dist) then
					PLR:ChatPrint("Input not a number")
					return
				end

				DEPTH_THRESH = math.Max(tonumber(dist), 0)
				PLR:ChatPrint("Depth threshold set")
			end
		},
		setSF = {
			desc = "sets the starfall that you're looking at as the display SF",
			func = function()
				local aimEnt = PLR:GetEyeTrace().Entity

				if not aimEnt or not aimEnt:IsValid() then
					PLR:ChatPrint("Entity invalid")
					return
				end

				if not aimEnt.instance or not aimEnt.instance.env or not aimEnt.instance.env.onRender then
					PLR:ChatPrint("Aim entity either not an SF or does not contian onRender function")
					return
				end

				STARFALL = aimEnt
				PLR:ChatPrint("Marked as display SF!")
			end
		}
		
	}

	hook.Add("OnPlayerChat", "chatCommands", function(plr, msg)
		if plr ~= PLR or msg[1] ~= "/" then return end
		
		local args = string.Explode(" ", string.sub(msg, 2))
		
		if args[1] == "help" then
			for k, v in pairs(COMMANDS) do
				PLR:ChatPrint("/"..k.." --> "..v.desc)
			end
		elseif COMMANDS[args[1]] then
			local cmd = args[1]
			table.remove(args, 1)

			if COMMANDS[cmd].numArgs and #args ~= COMMANDS[cmd].numArgs then
				PLR:ChatPrint(
					"Invalid number of arguments, expected "..tostring(COMMANDS[cmd].numArgs..", got "..tostring(#args))
				)
				return
			end
			if IS_RENDERING then
				PLR:ChatPrint("Unable to use commands while tracing")
			else
				COMMANDS[cmd].func(unpack(args))
			end
		else
			PLR:ChatPrint("Unknown command '"..args[1].."'")
		end
	end)
end