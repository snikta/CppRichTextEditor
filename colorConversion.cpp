#pragma once
#include "colorConversion.h"
#include <d2d1.h>
#include <iostream>
#include <string>
#pragma comment(lib, "d2d1")

const D2D1_COLOR_F hsvToRgb(float h, float s, float v) {
	float c = v * s;
	float x = c * (1 - abs(fmod(h / 60, 2) - 1));
	float m = v - c;
	float r_ = 0.0;
	float g_ = 0.0;
	float b_ = 0.0;

	if (h == 360) { h = 0; }

	if (h >= 0 && h < 60) {
		r_ = c; g_ = x; b_ = 0;
	}
	else if (h >= 60 && h < 120) {
		r_ = x; g_ = c; b_ = 0;
	}
	else if (h >= 120 && h < 180) {
		r_ = 0; g_ = c; b_ = x;
	}
	else if (h >= 180 && h < 240) {
		r_ = 0; g_ = x; b_ = c;
	}
	else if (h >= 240 && h < 300) {
		r_ = x; g_ = 0; b_ = c;
	}
	else if (h >= 300 && h < 360) {
		r_ = c; g_ = 0; b_ = x;
	}

	return D2D1_COLOR_F(D2D1::ColorF(r_ + m, g_ + m, b_ + m, 1.0f));
}
