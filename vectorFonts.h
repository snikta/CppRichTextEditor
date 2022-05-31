#pragma once
typedef uint32_t TScreenColor;
#define COLOR32(red, green, blue, alpha)  (((red) & 0xFF)        \
					| ((green) & 0xFF) << 8  \
					| ((blue) & 0xFF) << 16  \
					| ((alpha) & 0xFF) << 24)

int drawGlyph(
	char chr,
	int dx,
	int dy,
	TScreenColor color,
	unsigned char* buffer,
	int bufferWidth,
	int bufferSize
);