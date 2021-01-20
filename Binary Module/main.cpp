#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <algorithm>

#include "GarrysMod/Lua/Interface.h"

#include "main.h"
#include "entity.h"
#include "pathtracer.h"
#include "image.h"

using namespace GarrysMod::Lua;

void printLua(ILuaBase* inst, const char text[])
{
	inst->PushSpecial(SPECIAL_GLOB);
	inst->GetField(-1, "print");
	inst->PushString(text);
	inst->Call(1, 0);
	inst->Pop();
}

void dumpStack(ILuaBase* inst)
{
	std::string toPrint = "";

	int max = inst->Top();
	for (int i = 1; i <= max; i++) {
		toPrint += "[" + std::to_string(i) + "] ";
		switch (inst->GetType(i)) {
		case Type::Angle:
			toPrint += "Angle: (" + std::to_string((int)inst->GetAngle(i).x) + ", " + std::to_string((int)inst->GetAngle(i).y) + ", " + std::to_string((int)inst->GetAngle(i).z) + ")";
			break;
		case Type::Bool:
			toPrint += "Bool: " + inst->GetBool(i);
			break;
		case Type::Function:
			toPrint += "Function";
			break;
		case Type::Nil:
			toPrint += "nil";
			break;
		case Type::Number:
			toPrint += "Number: " + std::to_string(inst->GetNumber(i));
			break;
		case Type::String:
			toPrint += "String: " + (std::string)inst->GetString(i);
			break;
		case Type::Table:
			toPrint += "Table";
			break;
		default:
			toPrint += "Unknown";
			break;
		}
		toPrint += "\n";
	}

	printLua(inst, toPrint.c_str());
}

void checkArgCount(ILuaBase* LUA, const int& count)
{
	if (LUA->Top() != count) {
		LUA->ThrowError((std::string("invalid number of arguments, expected ") + std::to_string(count)).c_str());
	}
}

static bool IS_TRACING = false, DETAILS_RECEIVED = true;
static long long renderTime, denoiseTime;

static std::thread MAIN_THREAD;

static std::map<unsigned int, Entity*> ENTS;
static std::map<unsigned int, Light*> LIGHTS;
static Camera CAMERA = Camera(
	Vector3(0.f),
	DEFAULT_RES,
	0.5f,
	80.f
);
static Image::PFM HDRI = Image::PFM("");
static bool validHDRI = false;

static std::vector<std::vector<Vector3>> SCENE;
static std::vector<std::vector<Vector3>> ALBEDO;
static std::vector<std::vector<std::pair<double, unsigned int>>> DEPTH_AND_OBJID;
static std::vector<std::vector<Vector3>> NORMAL;

static std::mutex mut;
static int nextCol;
static int pixelsTraced;
static int pixelsDenoised;

LUA_FUNCTION(SelectHDRI)
{
	if (IS_TRACING) return 0;
	checkArgCount(LUA, 1);
	LUA->CheckType(1, Type::String);

	HDRI = Image::PFM(LUA->GetString());
	validHDRI = HDRI.read();

	LUA->Pop();
	LUA->PushBool(validHDRI);
	return 1;
}
LUA_FUNCTION(DeselectHDRI)
{
	if (IS_TRACING) return 0;
	checkArgCount(LUA, 0);

	HDRI = Image::PFM("");
	validHDRI = false;

	return 0;
}

#pragma region Camera
LUA_FUNCTION(MoveCamera)
{
	if (IS_TRACING) return 0;
	checkArgCount(LUA, 1);
	LUA->CheckType(1, Type::Vector);

	CAMERA.pos = LUA->GetVector(1);

	return 0;
}
LUA_FUNCTION(AngleCamera)
{
	if (IS_TRACING) return 0;
	checkArgCount(LUA, 1);
	LUA->CheckType(1, Type::Table);

	// Read matrix
	if (LUA->ObjLen(1) != 3) {
		LUA->ThrowError("matrix has an invalid number of rows");
		return 0;
	}

	std::vector<std::vector<double>> ang = std::vector<std::vector<double>>(3, std::vector<double>(3, 0));

	LUA->PushNil();
	while (LUA->Next(1) != 0) {
		if (LUA->GetType(-2) != Type::Number) { LUA->ThrowError("Matrix contains non numeric key"); return 0; }
		if (LUA->GetType(-1) != Type::Table) { LUA->ThrowError("Matrix row not a table"); return 0; }

		if (LUA->ObjLen(-1) != 3) {
			LUA->ThrowError("matrix has an invalid number of columns");
			return 0;
		}

		// Iterate over values in row
		LUA->PushNil();
		while (LUA->Next(3) != 0) {
			if (LUA->GetType(-2) != Type::Number) { LUA->ThrowError("Matrix contains non numeric key"); return 0; }
			if (LUA->GetType(-1) != Type::Number) { LUA->ThrowError("Matrix cell not a number"); return 0; }

			ang[(size_t)LUA->GetNumber(-4) - 1][(size_t)LUA->GetNumber(-2) - 1] = (double)LUA->GetNumber(-1);

			LUA->Pop();
		}

		LUA->Pop();
	}
	LUA->Pop();

	CAMERA.ang = ang;

	return 0;
}
LUA_FUNCTION(SetResolution)
{
	if (IS_TRACING) return 0;
	checkArgCount(LUA, 2);
	LUA->CheckType(1, Type::Number);
	LUA->CheckType(2, Type::Number);

	CAMERA.resX = (int)LUA->GetNumber(1);
	CAMERA.resY = (int)LUA->GetNumber(2);

	return 0;
}
LUA_FUNCTION(SetViewplaneDist)
{
	if (IS_TRACING) return 0;
	checkArgCount(LUA, 1);
	LUA->CheckType(1, Type::Number);

	CAMERA.viewplaneDist = LUA->GetNumber(1);

	return 0;
}
LUA_FUNCTION(SetFOV)
{
	if (IS_TRACING) return 0;
	checkArgCount(LUA, 1);
	LUA->CheckType(1, Type::Number);

	CAMERA.fov = LUA->GetNumber(1);

	return 0;
}
LUA_FUNCTION(SetHDRIBrightness)
{
	if (IS_TRACING) return 0;
	checkArgCount(LUA, 1);
	LUA->CheckType(1, Type::Number);

	CAMERA.hdriBrightness = LUA->GetNumber(1);

	return 0;
}
LUA_FUNCTION(SetViewHDRIBrightness)
{
	if (IS_TRACING) return 0;
	checkArgCount(LUA, 1);
	LUA->CheckType(1, Type::Number);

	CAMERA.viewportHDRIBrightness = LUA->GetNumber(1);

	return 0;
}
LUA_FUNCTION(SetSamples)
{
	if (IS_TRACING) return 0;
	checkArgCount(LUA, 1);
	LUA->CheckType(1, Type::Number);

	CAMERA.samples = LUA->GetNumber(1);

	return 0;
}
LUA_FUNCTION(GetSamples)
{
	if (IS_TRACING) return 0;
	LUA->PushNumber(CAMERA.samples);
	return 1;
}
#pragma endregion

// Takes pos, colour, intensity, entIndex
LUA_FUNCTION(CreateLight)
{
	if (IS_TRACING) return 0;
	if (LUA->Top() != 4) {
		LUA->ThrowError("invalid number of arguments, expected 4");
		return 0;
	}

	// Check arg types
	if (LUA->GetType(1) != Type::Vector) { // Pos
		LUA->ArgError(1, "expected Vector");
		return 0;
	}
	if (LUA->GetType(2) != Type::Vector) { // Colour
		LUA->ArgError(2, "expected Vector");
		return 0;
	}
	if (LUA->GetType(3) != Type::Number) { // Intensity
		LUA->ArgError(3, "expected Number");
		return 0;
	}
	if (LUA->GetType(4) != Type::Number) { // Entity Index
		LUA->ArgError(4, "expected Number");
		return 0;
	}

	// Create light and add to lights
	LIGHTS.emplace((unsigned int)LUA->GetNumber(4), new PointLight(
		LUA->GetVector(1),
		LUA->GetVector(2),
		LUA->GetNumber(3)
	));

	return 0;
}

LUA_FUNCTION(CreateEntity)
{
	if (IS_TRACING) return 0;
	checkArgCount(LUA, 11);

	// Check the args are of the correct type
	LUA->CheckType(1, Type::Table);   // Meshes
	LUA->CheckType(2, Type::Table);   // Angle
	LUA->CheckType(3, Type::Table);   // Transposed angle (aka inverse)
	LUA->CheckType(4, Type::Vector);  // Position
	LUA->CheckType(5, Type::Vector);  // Colour
	LUA->CheckType(6, Type::Number);  // Alpha
	LUA->CheckType(7, Type::Number);  // Roughness
	LUA->CheckType(8, Type::Number);  // Emission (what percentage of the entity's colour is emissive. Basically brightness)
	LUA->CheckType(9, Type::Number);  // IoR
	LUA->CheckType(10, Type::Number); // Metalness
	LUA->CheckType(11, Type::Number); // Entity Index

	// Read ent index
	int entIndex = (int)LUA->GetNumber(11);
	LUA->Pop();

	// Read Metalness
	double metalness = LUA->GetNumber(10);
	LUA->Pop();
	// Read IoR
	double IoR = LUA->GetNumber(9);
	LUA->Pop();
	// Read emission
	double emmision = LUA->GetNumber(8);
	LUA->Pop();
	// Read roughness
	double roughness = LUA->GetNumber(7);
	LUA->Pop();
	// Read alpha
	double alpha = LUA->GetNumber(6);
	LUA->Pop();

	// Read the entity's colour
	Vector3 colour = LUA->GetVector(5);
	LUA->Pop();

	// Read the position
	Vector3 pos = LUA->GetVector(4);
	LUA->Pop();

	// Read both matrices
	std::vector<std::vector<double>> ang = std::vector<std::vector<double>>(3, std::vector<double>(3, 0));
	std::vector<std::vector<double>> invAng = std::vector<std::vector<double>>(3, std::vector<double>(3, 0));
	for (int i = 0; i < 2; i++) {
		if (LUA->ObjLen(3 - i) != 3) {
			LUA->ThrowError("matrix has an invalid number of rows");
			return 0;
		}

		LUA->PushNil();
		while (LUA->Next(3 - i) != 0) {
			if (LUA->GetType(-2) != Type::Number) { LUA->ThrowError("Matrix contains non numeric key"); return 0; }
			if (LUA->GetType(-1) != Type::Table) { LUA->ThrowError("Matrix row not a table"); return 0; }

			if (LUA->ObjLen(-1) != 3) {
				LUA->ThrowError("matrix has an invalid number of columns");
				return 0;
			}

			// Iterate over values in row
			LUA->PushNil();
			while (LUA->Next(5 - i) != 0) {
				if (LUA->GetType(-2) != Type::Number) { LUA->ThrowError("Matrix contains non numeric key"); return 0; }
				if (LUA->GetType(-1) != Type::Number) { LUA->ThrowError("Matrix cell not a number"); return 0; }

				if (i == 1) ang[(size_t)LUA->GetNumber(-4) - 1][(size_t)LUA->GetNumber(-2) - 1] = LUA->GetNumber(-1);
				else invAng[(size_t)LUA->GetNumber(-4) - 1][(size_t)LUA->GetNumber(-2) - 1] = LUA->GetNumber(-1);

				LUA->Pop();
			}

			LUA->Pop();
		}

		LUA->Pop();
	}

	// Read the meshes from the stack
	std::vector<Mesh> meshes;

	LUA->PushNil();
	while (LUA->Next(1) != 0) {
		if (LUA->GetType(-2) != Type::Number) { LUA->ThrowError("Mesh array contains non numeric key"); return 0; }
		if (LUA->GetType(-1) != Type::Table) { LUA->ThrowError("Mesh not a table"); return 0; }

		// Read the texture into a material
		LUA->GetField(-1, "material");
		if (!LUA->IsType(-1, Type::Table)) {
			LUA->ThrowError("Mesh material not a table");
			return 0;
		}

		LUA->GetField(-1, "base");
		if (!LUA->IsType(-1, Type::String)) {
			LUA->ThrowError("Base texture not a string");
			return 0;
		}

		LUA->GetField(-2, "normal");
		if (!LUA->IsType(-2, Type::String)) {
			LUA->ThrowError("Normal texture not a string");
			return 0;
		}

		Material mat{
			LUA->GetString(-2),
			LUA->GetString(-1),
			colour,
			alpha,
			roughness,
			IoR,
			colour * emmision,
			metalness
		};
		LUA->Pop(3);

		// Read the mesh verts
		LUA->GetField(-1, "triangles");
		if (!LUA->IsType(-1, Type::Table)) {
			LUA->ThrowError("Mesh verts not a table");
			return 0;
		}

		std::vector<MeshVertex> verts(LUA->ObjLen(4));
		LUA->PushNil();
		while (LUA->Next(4) != 0) {
			if (LUA->GetType(-2) != Type::Number) { LUA->ThrowError("Vertex array contains non numeric key"); return 0; }
			if (LUA->GetType(-1) != Type::Table) { LUA->ThrowError("Vertex not a table"); return 0; }

			LUA->GetField(-1, "pos");
			if (LUA->GetType(-1) != Type::Vector) { LUA->ThrowError("Vertex position not a vector"); return 0; }
			Vector pos = LUA->GetVector(-1);
			LUA->Pop();

			LUA->GetField(-1, "normal");
			if (LUA->GetType(-1) != Type::Vector) { LUA->ThrowError("Vertex normal not a vector"); return 0; }
			Vector normal = LUA->GetVector(-1);
			LUA->Pop();

			LUA->GetField(-1, "tangent");
			if (LUA->GetType(-1) != Type::Vector) { LUA->ThrowError("Vertex tangent not a vector"); return 0; }
			Vector tangent = LUA->GetVector(-1);
			LUA->Pop();

			LUA->GetField(-1, "binormal");
			if (LUA->GetType(-1) != Type::Vector) { LUA->ThrowError("Vertex binormal not a vector"); return 0; }
			Vector binormal = LUA->GetVector(-1);
			LUA->Pop();

			LUA->GetField(-1, "u");
			if (LUA->GetType(-1) != Type::Number) { LUA->ThrowError("Vertex u not a number"); return 0; }
			double u = LUA->GetNumber(-1);
			LUA->Pop();

			LUA->GetField(-1, "v");
			if (LUA->GetType(-1) != Type::Number) { LUA->ThrowError("Vertex v not a number"); return 0; }
			double v = LUA->GetNumber(-1);
			LUA->Pop(2);

			verts[static_cast<size_t>(LUA->GetNumber(-1)) - 1] = MeshVertex{ pos, normal, u, v, tangent, binormal };
		}
		LUA->Pop(2);

		meshes.push_back(Mesh{ mat, verts });
	}
	LUA->Pop();

	Entity* ent = new Entity(pos, ang, invAng, meshes, entIndex);
	ent->calcAABB();

	ENTS.emplace(entIndex, ent);

	return 0;
}

LUA_FUNCTION(ResetTracer)
{
	if (IS_TRACING) return 0;
	ENTS = std::map<unsigned int, Entity*>();
	LIGHTS = std::map<unsigned int, Light*>();
	//HDRI = nullptr;
	return 0;
}

void performRender(const int kernelSize, const double normalThresh, const double albedoThresh, const double depthThresh)
{
	// Compress ent and light maps into vectors
	std::vector<Entity*> ents;
	std::map<unsigned int, Entity*>::iterator entI;
	for (entI = ENTS.begin(); entI != ENTS.end(); entI++) {
		ents.push_back(entI->second);
	}

	std::vector<Light*> lights;
	std::map<unsigned int, Light*>::iterator lightI;
	for (lightI = LIGHTS.begin(); lightI != LIGHTS.end(); lightI++) {
		lights.push_back(lightI->second);
	}

	// Init the scene and shared memory
	SCENE = std::vector<std::vector<Vector3>>(CAMERA.resX, std::vector<Vector3>(CAMERA.resY, Vector3(0, 0, 0)));
	ALBEDO = std::vector<std::vector<Vector3>>(CAMERA.resX, std::vector<Vector3>(CAMERA.resY, Vector3(0, 0, 0)));
	DEPTH_AND_OBJID = std::vector<std::vector<std::pair<double, unsigned int>>>(CAMERA.resX, std::vector<std::pair<double, unsigned int>>(CAMERA.resY));
	NORMAL = std::vector<std::vector<Vector3>>(CAMERA.resX, std::vector<Vector3>(CAMERA.resY, Vector3(0, 0, 0)));

	nextCol = CAMERA.resX;

	// Create the threads
	std::vector<std::thread> threads(std::thread::hardware_concurrency());
	for (int i = 0; i < threads.size(); i++) {
		threads[i] = std::thread(renderScene,
			std::ref(CAMERA),
			std::ref(ents), std::ref(lights),
			std::ref(SCENE),
			std::ref(mut), std::ref(nextCol), std::ref(pixelsTraced),
			validHDRI ? &HDRI : nullptr,
			std::ref(ALBEDO),
			std::ref(DEPTH_AND_OBJID),
			std::ref(NORMAL)
		);
	}

	// Join the threads
	auto begin = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < threads.size(); i++) {
		threads[i].join();
	}
	auto end = std::chrono::high_resolution_clock::now();

	// Denoise
	nextCol = CAMERA.resX;
	const double normalThreshRadians = normalThresh * M_PI / 180;
	threads = std::vector<std::thread>(std::thread::hardware_concurrency());
	std::vector<std::vector<Vector3>> denoised = std::vector<std::vector<Vector3>>(CAMERA.resX, std::vector<Vector3>(CAMERA.resY, Vector3(0, 0, 0)));
	for (int i = 0; i < threads.size(); i++) {
		threads[i] = std::thread(denoise,
			std::ref(denoised), std::ref(SCENE), std::ref(ALBEDO), std::ref(NORMAL), std::ref(DEPTH_AND_OBJID),
			std::ref(mut), std::ref(nextCol), std::ref(pixelsDenoised),
			std::ref(kernelSize),
			std::ref(normalThreshRadians), std::ref(albedoThresh), std::ref(depthThresh)
		);
	}

	auto beginDenoise = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < threads.size(); i++) {
		threads[i].join();
	}
	auto endDenoise = std::chrono::high_resolution_clock::now();

	// Save images
	Image::PPM output = Image::PPM((std::string(OUTPUT_PATH) + std::string("render.ppm")).c_str());
	Image::PPM denoisedImg = Image::PPM((std::string(OUTPUT_PATH) + std::string("denoised.ppm")).c_str());
	Image::PPM denoisedImgSF = Image::PPM((std::string(STARFALL_PATH) + std::string("denoised.ppm")).c_str());

	output.write(SCENE, CAMERA.resX, CAMERA.resY);
	denoisedImg.write(denoised, CAMERA.resX, CAMERA.resY);
	denoisedImgSF.write(denoised, CAMERA.resX, CAMERA.resY);

	// Finalise
	mut.lock();
	renderTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count();
	denoiseTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endDenoise - beginDenoise).count();

	IS_TRACING = false;
	mut.unlock();
}

LUA_FUNCTION(PollCompletion)
{
	if (IS_TRACING || DETAILS_RECEIVED) {
		LUA->PushBool(false);
		return 1;
	}

	DETAILS_RECEIVED = true;
	LUA->PushBool(true);
	LUA->PushNumber(renderTime);
	LUA->PushNumber(denoiseTime);
	return 3;
}

LUA_FUNCTION(StartRender)
{
	if (IS_TRACING) return 0;

	checkArgCount(LUA, 4);

	LUA->CheckType(1, Type::Number); // kernel size
	LUA->CheckType(2, Type::Number); // normal thresh
	LUA->CheckType(3, Type::Number); // albedo thresh
	LUA->CheckType(4, Type::Number); // depth thresh

	IS_TRACING = true;
	DETAILS_RECEIVED = false;
	MAIN_THREAD = std::thread(performRender, LUA->GetNumber(1), LUA->GetNumber(2), LUA->GetNumber(3), LUA->GetNumber(4));
	MAIN_THREAD.detach();

	LUA->PushBool(true);
	return 1;
}

GMOD_MODULE_OPEN()
{
	LUA->PushSpecial(SPECIAL_GLOB);
		LUA->PushCFunction(SelectHDRI);
		LUA->SetField(-2, "SelectHDRI");

		LUA->PushCFunction(DeselectHDRI);
		LUA->SetField(-2, "DeselectHDRI");

		LUA->PushCFunction(MoveCamera);
		LUA->SetField(-2, "MoveCamera");

		LUA->PushCFunction(AngleCamera);
		LUA->SetField(-2, "AngleCamera");

		LUA->PushCFunction(SetResolution);
		LUA->SetField(-2, "SetResolution");

		LUA->PushCFunction(SetViewplaneDist);
		LUA->SetField(-2, "SetViewplaneDist");

		LUA->PushCFunction(SetFOV);
		LUA->SetField(-2, "SetFOV");

		LUA->PushCFunction(SetHDRIBrightness);
		LUA->SetField(-2, "SetHDRIBrightness");

		LUA->PushCFunction(SetViewHDRIBrightness);
		LUA->SetField(-2, "SetViewHDRIBrightness");

		LUA->PushCFunction(SetSamples);
		LUA->SetField(-2, "SetSamples");
		LUA->PushCFunction(GetSamples);
		LUA->SetField(-2, "GetSamples");

		LUA->PushCFunction(CreateLight);
		LUA->SetField(-2, "CreateLight");

		LUA->PushCFunction(CreateEntity);
		LUA->SetField(-2, "CreateEntity");

		LUA->PushCFunction(ResetTracer);
		LUA->SetField(-2, "ResetTracer");

		LUA->PushCFunction(PollCompletion);
		LUA->SetField(-2, "PollCompletion");

		LUA->PushCFunction(StartRender);
		LUA->SetField(-2, "StartRender");
	LUA->Pop();

	printLua(LUA, "Binary loaded");

	return 0;
}

GMOD_MODULE_CLOSE()
{
	while (true) {
		mut.lock();
		if (!IS_TRACING) {
			mut.unlock();
			return 0;
		}
		mut.unlock();
	}
}

// Main entrypoint used to debug from an exe, rather than through GMod (you'll need to change the path to a valid PFM test HDRI if you want to debug this way)
int main()
{
	HDRI = Image::PFM("D:\\Steam\\steamapps\\common\\GarrysMod\\garrysmod\\data\\sf_filedata\\hdri\\urban_courtyard_02_1k.pfm");
	validHDRI = HDRI.read();

	std::vector<std::vector<double>> unitMatrix = { {1, 0, 0}, {0, 1, 0}, {0, 0, 1} };

	Material mat{
		"",
		"",
		Vector3(1)
	};

	Entity ent = Entity(Vector3(0), unitMatrix, unitMatrix, { Mesh{
		mat,
		{
			MeshVertex{
				Vector3(-23.72500038147, 23.72500038147, -23.724994659424),
				Vector3(-0.89442729949951, -3.9096661907934e-08, 0.44721314311028),
				0, 1,
				Vector3(-1.8958832015414e-07, -0.99999988079071, -4.6659994268339e-07),
				Vector3(-0.89442729949951, -3.9096661907934e-08, 0.44721314311028).cross(Vector3(-1.8958832015414e-07, -0.99999988079071, -4.6659994268339e-07))
			},
			MeshVertex{
				Vector3(-3.999999989901e-06, -2.0000002223242e-06, 23.725017547607),
				Vector3(-0.89442729949951, -3.9096661907934e-08, 0.44721314311028),
				0.5, 0,
				Vector3(-1.8958832015414e-07, -0.99999988079071, -4.6659994268339e-07),
				Vector3(-0.89442729949951, -3.9096661907934e-08, 0.44721314311028).cross(Vector3(-1.8958832015414e-07, -0.99999988079071, -4.6659994268339e-07))
			},
			MeshVertex{
				Vector3(-23.724977493286, -23.725008010864, -23.72497177124),
				Vector3(-0.89442729949951, -3.9096661907934e-08, 0.44721314311028),
				1, 0.99999898672104,
				Vector3(-1.8958832015414e-07, -0.99999988079071, -4.6659994268339e-07),
				Vector3(-0.89442729949951, -3.9096661907934e-08, 0.44721314311028).cross(Vector3(-1.8958832015414e-07, -0.99999988079071, -4.6659994268339e-07))
			}
		}
	} }, 1);

	ent.calcAABB();

	std::vector<Entity*> ents;
	ents.push_back(&ent);

	SCENE = std::vector<std::vector<Vector3>>(CAMERA.resX, std::vector<Vector3>(CAMERA.resY, Vector3(0, 0, 0)));
	ALBEDO = std::vector<std::vector<Vector3>>(CAMERA.resX, std::vector<Vector3>(CAMERA.resY, Vector3(0, 0, 0)));
	DEPTH_AND_OBJID = std::vector<std::vector<std::pair<double, unsigned int>>>(CAMERA.resX, std::vector<std::pair<double, unsigned int>>(CAMERA.resY));
	NORMAL = std::vector<std::vector<Vector3>>(CAMERA.resX, std::vector<Vector3>(CAMERA.resY, Vector3(0, 0, 0)));

	nextCol = CAMERA.resX;

	CAMERA.pos = Vector3(-30, 0, 0);

	printf("Starting render\n");
	renderScene(
		CAMERA,
		ents, {},
		SCENE,
		mut, nextCol, pixelsTraced,
		validHDRI ? &HDRI : nullptr,
		ALBEDO,
		DEPTH_AND_OBJID,
		NORMAL
	);

	Image::PPM output = Image::PPM((std::string(OUTPUT_PATH) + std::string("render.ppm")).c_str());
	output.write(SCENE, CAMERA.resX, CAMERA.resY);

	printf("Done\n");
	system("PAUSE");

	return 0;
}