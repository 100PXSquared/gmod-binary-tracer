#pragma once

#include "vector.h"

namespace Image
{
	class PFM
	{
	private:
		std::string path;
		std::vector<std::vector<Vector3>> pixels;
		bool bigEndian;
	public:
		int width;
		int height;
		int channels;

		PFM(const char[]);

		bool read();
		bool write(std::vector<std::vector<Vector3>>& imgData, const int& resX, const int& resY);
		Vector3 getPixel(const int&, const int&) const;
	};

	class PPM
	{
	private:
		std::string path;
		std::vector<std::vector<Vector3>> pixels;

	public:
		int width;
		int height;

		PPM(const char[]);

		bool read();
		bool write(std::vector<std::vector<Vector3>>& imgData, const int& resX, const int& resY);
		Vector3 getPixel(const int&, const int&) const;
	};
}