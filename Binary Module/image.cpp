#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>

#include "image.h"

namespace Image
{
#pragma region PFM
	PFM::PFM(const char _path[])
	{
		path = _path;
		pixels = { { 1 } };
	}

	bool PFM::read()
	{
		std::ifstream file;
		file.open(path, std::ifstream::binary);
		if (!file.good()) return false;

		char hdr[3] = {0, 0, 0};
		file.read(hdr, 2);

		if (std::string(hdr) == "PF") channels = 3;
		else if (std::string(hdr) == "Pf") channels = 1;
		else { file.close(); return false; }

		std::string _width, _height, ratio;
		int stage = 0;
		while (true) {
			char byte[2] = {0, 0};
			file.read(byte, 1);
			std::string byteStr = std::string(byte);

			if (byteStr == "\n" || byteStr == " " || byteStr == "\r") stage++;
			else if (isdigit(byte[0]) || byteStr == "-" || byteStr == ".") {
				if (stage == 0) stage++;
					
				switch (stage) {
				case 1:
					_width += byte[0];
					break;
				case 2:
					_height += byte[0];
					break;
				case 3:
					ratio += byte[0];
					break;
				default:
					break;
				}
			} else {
				file.close();
				return false;
			}

			if (stage == 4 && byteStr == "\n") break;
		}

		width = std::stoi(_width);
		height = std::stoi(_height);

		bigEndian = std::stof(ratio) >= 0;

		pixels = *(new std::vector<std::vector<Vector3>>(width, std::vector<Vector3>(height)));
		if (channels == 3) {
			for (int y = height - 1; y >= 0; y--) {
				for (int x = 0; x < width; x++) {
					char r[4], g[4], b[4];
					file.read(r, 4);
					file.read(g, 4);
					file.read(b, 4);

					if (bigEndian) {
						std::reverse(r, r + 3);
						std::reverse(g, g + 3);
						std::reverse(b, b + 3);
					}

					pixels[x][y] = Vector3(*(float*)&r, *(float*)&g, *(float*)&b);
				}
			}
		} else {
			for (int y = height - 1; y >= 0; y--) {
				for (int x = 0; x < width; x++) {
					char grey[4];
					file.read(grey, 4);

					if (!bigEndian) std::reverse(grey, grey + 3);

					pixels[x][y] = Vector3(*(float*)&grey);
				}
			}
		}

		file.close();
		return true;
	}

	bool PFM::write(std::vector<std::vector<Vector3>>& imgData, const int& resX, const int& resY)
	{
		std::ofstream file(path, std::ofstream::binary);
		if (!file.is_open()) return false;

		const std::string hdr = "PF\n" + std::to_string(resX) + " " + std::to_string(resY) + "\n" + std::to_string((float)(resX / resY) * -1.f);
		file << hdr << std::endl;

		for (int y = resY - 1; y >= 0; y--) {
			for (int x = 0; x < resX; x++) {
				const float r = static_cast<float>(imgData[x][y].x);
				const float g = static_cast<float>(imgData[x][y].y);
				const float b = static_cast<float>(imgData[x][y].z);

				file.write(reinterpret_cast<const char*>(&r), sizeof(float));
				file.write(reinterpret_cast<const char*>(&g), sizeof(float));
				file.write(reinterpret_cast<const char*>(&b), sizeof(float));
			}
		}

		file.close();
		return true;
	}

	Vector3 PFM::getPixel(const int& x, const int& y) const
	{
		if (x < 0 || x >= width || y < 0 || y >= height) return Vector3(1.f, 0.f, 1.f);
		return Vector3(pixels[x][y]);
	}
#pragma endregion

#pragma region PPM
	PPM::PPM(const char _path[])
	{
		path = _path;
	}

	bool PPM::read()
	{
		std::ifstream file;
		file.open(path, std::ifstream::binary);
		if (!file.good()) return false;

		char hdr[3] = { 0, 0, 0 };
		file.read(hdr, 2);

		if (std::string(hdr) != "P6") {
			file.close();
			return false;
		}

		std::string _width, _height;
		int stage = 0;
		while (true) {
			char byte[2] = { 0, 0 };
			file.read(byte, 1);
			std::string byteStr = std::string(byte);

			if (byteStr == "\n" || byteStr == " " || byteStr == "\r") stage++;
			else if (isdigit(byte[0])) {
				if (stage == 0) stage++;

				switch (stage) {
				case 1:
					_width += byte[0];
					break;
				case 2:
					_height += byte[0];
					break;
				default:
					break;
				}
			} else {
				file.close();
				return false;
			}

			if (stage == 4 && byteStr == "\n") break;
		}

		width = std::stoi(_width);
		height = std::stoi(_height);

		pixels = *(new std::vector<std::vector<Vector3>>(width, std::vector<Vector3>(height)));
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				char rgb[3];
				file.read(rgb, 3);

				pixels[x][y] = Vector3(
					static_cast<double>((unsigned char)rgb[0]) / 255,
					static_cast<double>((unsigned char)rgb[1]) / 255,
					static_cast<double>((unsigned char)rgb[2]) / 255
				);
			}
		}

		file.close();
		return true;
	}

	bool PPM::write(std::vector<std::vector<Vector3>>& imgData, const int& resX, const int& resY)
	{
		std::ofstream file(path, std::ofstream::binary);
		if (!file.is_open()) return false;

		const std::string hdr = "P6\n" + std::to_string(resX) + " " + std::to_string(resY) + "\n255";
		file << hdr.c_str() << std::endl;

		for (int y = 0; y < resY; y++) {
			for (int x = 0; x < resX; x++) {
				char rgb[3] = {
					static_cast<char>(std::clamp(imgData[x][y].x, 0., 1.) * 255),
					static_cast<char>(std::clamp(imgData[x][y].y, 0., 1.) * 255),
					static_cast<char>(std::clamp(imgData[x][y].z, 0., 1.) * 255)
				};
				file.write(rgb, 3);
			}
		}

		file.close();
		return true;
	}

	Vector3 PPM::getPixel(const int& x, const int& y) const
	{
		if (x < 0 || x >= width || y < 0 || y >= height) return Vector3(1.f, 0.f, 1.f);
		return Vector3(pixels[x][y]);
	}
#pragma endregion
}