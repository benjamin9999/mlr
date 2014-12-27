
#include "stdafx.h"
#include <iostream>
#include <vector>

#include <boost/format.hpp>


#include "stats.h"

using namespace PixelToaster;
using namespace std;

const int NICE_BLUE = 0x2266aa;
const int NICE_GREEN = 0x33cc77;
const int NICE_LAVENDER = 0x8877aa;
const int NICE_FOREST = 0x558822;
const int NICE_MAGENTA = 0x9911aa;


Timer teletimer;
vector<int> telecolors = {
	NICE_BLUE, NICE_GREEN, NICE_LAVENDER, NICE_FOREST, NICE_MAGENTA,
	NICE_BLUE, NICE_GREEN, NICE_LAVENDER, NICE_FOREST, NICE_MAGENTA,
	NICE_BLUE, NICE_GREEN, NICE_LAVENDER, NICE_FOREST, NICE_MAGENTA,
	NICE_BLUE, NICE_GREEN, NICE_LAVENDER, NICE_FOREST, NICE_MAGENTA
};


void Telemetry::start()
{
	this->markers.clear();
	this->x = 0;
	teletimer.reset();
}

void Telemetry::mark()
{
	this->markers.push_back({ teletimer.delta() * 1000, this->x++ });
}

void Telemetry::mark2(bool advance)
{
	this->markers.push_back({ teletimer.delta() * 1000, this->x });
	if (advance) this->x++;
}

void Telemetry::end()
{
}

void Telemetry::print() const
{
	cout << "--------------" << endl;
	for (auto& item : markers) {
		cout << "x:" << item.x << ", t:" << boost::format("%.4f") % item.tm << endl;
	}
}

void Telemetry::draw(const unsigned stride, TrueColorPixel * const __restrict dst) const
{
	const int offset = 5; //pixels
	const int graph_width = stride - offset * 2;
	//	const float ms_width = 40.0f;
	const float factor = 20.0f;

	int ci = 0;
	int px = offset;
	for (auto& item : markers) {
		if (ci == 0) {
			ci++; continue;
		}
		int width = static_cast<int>(item.tm * factor + 0.5);
		for (int i = 0; px < stride - offset && i < width; i++, px++) {
			for (int row = 0; row < 5; row++) {
				dst[px + ((row + offset)*stride)].integer = telecolors[item.x];
			}
		}
		if (width)
			px += 1;
		ci++;
		//		cout << "x:" << item.x << ", t:" << boost::format("%.4f") % item.tm << endl;
	}

}