// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A 
// PARTICULAR PURPOSE. 
// 
// Copyright (c) Microsoft Corporation. All rights reserved 

#pragma once 

#include <wincodec.h>
#include <d2d1.h>
#pragma comment(lib, "d2d1")
#include "Resource.h" 

const float DEFAULT_DPI = 96.f;   // Default DPI that maps image resolution directly to screen resolution 

								  /******************************************************************
								  *                                                                 *
								  *  DemoApp                                                        *
								  *                                                                 *
								  ******************************************************************/

class DemoApp
{
public:
	static HRESULT CreateDeviceIndependentResources(void);
	HRESULT Initialize(HINSTANCE hInstance);

	DemoApp();
	~DemoApp();

private:
	HRESULT CreateD2DBitmapFromFile(HWND hWnd);
	bool LocateImageFile(HWND hWnd, LPWSTR pszFileName, DWORD cbFileName);
	HRESULT CreateDeviceResources(HWND hWnd);

	LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnPaint(HWND hWnd);

	static LRESULT CALLBACK s_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:

	HINSTANCE               m_hInst;
	IWICImagingFactory* m_pIWICFactory;

	ID2D1Factory* m_pD2DFactory;
	ID2D1HwndRenderTarget* m_pRT;
	ID2D1Bitmap* m_pD2DBitmap;
	IWICFormatConverter* m_pConvertedSourceBitmap;
};
