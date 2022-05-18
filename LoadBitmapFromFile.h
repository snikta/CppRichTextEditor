#pragma once
#include <wincodec.h>
#include <d2d1.h>
#pragma comment(lib, "d2d1")
#include "Resource.h"

HRESULT LoadBitmapFromFile(
	ID2D1RenderTarget* pRenderTarget,
	IWICImagingFactory* pIWICFactory,
	PCWSTR uri,
	UINT destinationWidth,
	UINT destinationHeight,
	ID2D1Bitmap** ppBitmap
);
