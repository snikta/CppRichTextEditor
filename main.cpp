#include <windows.h>
#include <Windowsx.h>
#include <d2d1.h>
#include <iostream>
#include <string>
#pragma comment(lib, "d2d1")
#include <dwrite.h>
#pragma comment(lib, "Dwrite")
#include "basewin.h"
#include <vector>
#include <map>
#include "LoadBitmapFromFile.h"
#include "SafeRelease.h"
#include "colorConversion.h"
#include "vectorFonts.h"
using namespace std;
using std::string;
using std::vector;
using std::map;

extern int glyphWidths[];

float clamp(float value, float minLimit, float maxLimit) {
	return min(max(value, minLimit), maxLimit);
}

struct formatStop
{
	UINT32 startIndex;
	UINT32 endIndex;
	UINT32 width;
	string value;
};

void splitString(std::string str, char delim, std::vector<std::string>& words)
{
	int i = 0, len = str.length();
	std::string el;
	while (i < len) {
		if (str[i] == delim) {
			words.push_back(el);
			el = "";
		}
		else {
			el += str[i];
		}
		i++;
	}
	words.push_back(el);
}

void RenderBitmap(PCWSTR fileName, double x, double y, ID2D1HwndRenderTarget* pRenderTarget, IWICImagingFactory* m_pWICFactory) {
	ID2D1Bitmap* m_pBitmap;
	HRESULT hr = LoadBitmapFromFile(
		pRenderTarget,
		m_pWICFactory,
		fileName,
		0,
		0,
		&m_pBitmap
	);

	if (SUCCEEDED(hr))
	{
		// Retrieve the size of the bitmap.
		D2D1_SIZE_F size = m_pBitmap->GetSize();

		D2D1_POINT_2F upperLeftCorner = D2D1::Point2F(x, y);

		// Draw a bitmap.
		pRenderTarget->DrawBitmap(
			m_pBitmap,
			D2D1::RectF(
				upperLeftCorner.x,
				upperLeftCorner.y,
				upperLeftCorner.x + size.width,
				upperLeftCorner.y + size.height)
		);

		SafeRelease(&m_pBitmap);
	}
}

typedef struct HitTestMetrics {
	float	left;
	float	top;
	float	width;
	float	height;
	UINT32	textPosition;
} HitTestMetrics;

typedef struct GlyphSize {
	float	width;
	float	height;
} GlyphSize;

ID2D1Factory* g_pD2DFactory = NULL;
IDWriteFontFace* g_pFontFace = NULL;
IDWriteFontFile* g_pFontFile = NULL;
IDWriteTextFormat* g_pTextFormat = NULL;

map<int, GlyphSize> GlyphSizes = {};

class MainWindow : public BaseWindow<MainWindow>
{
    ID2D1Factory            *pFactory;
    ID2D1HwndRenderTarget   *pRenderTarget;
    ID2D1SolidColorBrush    *pBrush;
	IDWriteFactory			*m_pDWriteFactory;
	IDWriteTextFormat		*m_pTextFormat;
	IDWriteTextFormat		*m_pFontNamesTextFormat;
	IDWriteTextFormat		*m_pFontSizesTextFormat;
	ID2D1SolidColorBrush	*pPaleYellowBrush;
	DWRITE_TEXT_METRICS		fNameMetrics;
	DWRITE_TEXT_METRICS		fSizeMetrics;
	vector<string>			fontNames = { "Merriweather", "Arial", "Courier New", "Times New Roman", "Comic Sans MS", "Impact", "Georgia", "Trebuchet MS", "Webdings", "Verdana" };
	vector<string>			fontSizes = { "8", "9", "10", "11", "12", "14", "16", "18", "20", "22", "24", "26", "28", "36", "48", "72" };
	string					userText = "Apollo 11 launched from Kennedy Space Center on July 16, 1969. Astronauts Neil Armstrong and Buzz Aldrin landed on the Moon on July 20 while Command Module Pilot Michael Collins orbited the Moon awaiting their return. All three astronauts splashed down safely on July 24.";
	IDWriteTextLayout		*pOpenTextLayout;
	IDWriteTextLayout		*pFontNameTextLayout;
	IDWriteTextLayout		*pFontSizesTextLayout;
	float					caretX;
	float					caretY;
	float					cursorX;
	float					cursorY;
	HCURSOR					hCursor;
	HitTestMetrics hitTestMetrics;
	BOOL					isTrailingHit;
	UINT32					textPosition;
	UINT32					startTextPosition;
	vector<string>			icons = { "Bold.png", "Italic.png", "Underline.png", "Strikethrough.png", /*"Superscript.png", "Subscript.png",*/ "alignLeft.png", "alignCenter.png", "alignRight.png", "alignJustify.png", "Indent.png", "Outdent.png", "bulletedList.png", "numberedList.png", "forecolor.png", "backcolor.png" };
	D2D1_POINT_2F			toolbarOrigin = D2D1::Point2F(0.0f, 36.0f);
	bool					isSelecting = false;
	string					alignment = "left";
	map<string,map<UINT32, formatStop>> stopsByType;
	ID2D1SolidColorBrush	*pColorBrush;
	D2D1_RECT_F             hueRect;
	D2D1_RECT_F             satValRect1;
	D2D1_RECT_F             satValRect2;
	D2D1_RECT_F				rectColor;
	int						hueX = 0.0;
	string					rgbColor;

    void    CalculateLayout();
	HRESULT CreateDeviceIndependentResources();
    HRESULT CreateGraphicsResources();
    void    DiscardGraphicsResources();
	void	UpdateCaretFromPosition(bool verticalMove = false, bool click = false, bool paint = true);
	void	UpdateCaretFromIndex(bool verticalMove = false);
    void    OnPaint(bool verticalMove = false, bool click = false);
    void    Resize();
	void    OnLButtonDown(int pixelX, int pixelY, DWORD flags);
	void	OnLButtonUp();
	void    OnMouseMove(int pixelX, int pixelY, DWORD flags);
	void	SelectText();
	void	toggleFormatting(string format, string newValue);
	void	HitTestTextPosition(
		UINT32 textPosition,
		bool isTrailing,
		float* pointX,
		float* pointY,
		HitTestMetrics* metrics);

public:

	GlyphSize GetCharWidth(wchar_t c)
	{
		GlyphSize retval = {};

		// Create Direct2D Factory
		HRESULT hr = D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			&g_pD2DFactory
		);
		if (FAILED(hr))
		{
			MessageBox(NULL, L"Create Direct2D factory failed!", L"Error", 0);
			return retval;
		}

		// Create font file reference
		const WCHAR* filePath = L"C:/Windows/Fonts/arial.ttf";
		hr = m_pDWriteFactory->CreateFontFileReference(
			filePath,
			NULL,
			&g_pFontFile
		);
		if (FAILED(hr))
		{
			MessageBox(NULL, L"Create font file reference failed!", L"Error", 0);
			return retval;
		}

		// Create font face
		IDWriteFontFile* fontFileArray[] = { g_pFontFile };
		m_pDWriteFactory->CreateFontFace(
			DWRITE_FONT_FACE_TYPE_TRUETYPE,
			1,
			fontFileArray,
			0,
			DWRITE_FONT_SIMULATIONS_NONE,
			&g_pFontFace
		);
		if (FAILED(hr))
		{
			MessageBox(NULL, L"Create font file face failed!", L"Error", 0);
			return retval;
		}

		wchar_t textString[] = { c, '\0' };

		// Get text length
		UINT32 textLength = (UINT32)wcslen(textString);

		UINT32* pCodePoints = new UINT32[textLength];
		ZeroMemory(pCodePoints, sizeof(UINT32) * textLength);

		UINT16* pGlyphIndices = new UINT16[textLength];
		ZeroMemory(pGlyphIndices, sizeof(UINT16) * textLength);

		for (unsigned int i = 0; i < textLength; ++i)
		{
			pCodePoints[i] = textString[i];
		}

		// Get glyph indices
		hr = g_pFontFace->GetGlyphIndices(
			pCodePoints,
			textLength,
			pGlyphIndices
		);
		if (FAILED(hr))
		{
			MessageBox(NULL, L"Get glyph indices failed!", L"Error", 0);
			return retval;
		}

		DWRITE_GLYPH_METRICS* glyphmetrics = new DWRITE_GLYPH_METRICS[textLength];
		g_pFontFace->GetDesignGlyphMetrics(pGlyphIndices, textLength, glyphmetrics);

		DWRITE_FONT_METRICS* fontmetrics = new DWRITE_FONT_METRICS;

		// do your calculation here
		g_pFontFace->GetMetrics(fontmetrics);
		retval.width = glyphWidths[c - 32];
		retval.height = 21.0;

		delete[]glyphmetrics;
		glyphmetrics = NULL;

		return retval;
	}

    MainWindow() : pFactory(NULL), pRenderTarget(NULL), pBrush(NULL)
    {
		stopsByType["Bold"] = map <UINT32, formatStop>();
		stopsByType["Italic"] = map <UINT32, formatStop>();
		stopsByType["Underline"] = map <UINT32, formatStop>();
		stopsByType["Strikethrough"] = map <UINT32, formatStop>();
		stopsByType["Align"] = map <UINT32, formatStop>();
		stopsByType["ForeColor"] = map <UINT32, formatStop>();
		stopsByType["FontName"] = map <UINT32, formatStop>();
		stopsByType["FontSize"] = map <UINT32, formatStop>();
    }

    PCWSTR  ClassName() const { return L"Circle Window Class"; }
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// START: https://stackoverflow.com/questions/27220/how-to-convert-stdstring-to-lpcwstr-in-c-unicode
wstring s2ws(const string& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	wstring r(buf);
	delete[] buf;
	return r;
}
LPCWSTR stringToLPCWSTR(string str) {
	std::wstring stemp = s2ws(str);
	LPCWSTR result = stemp.c_str();
	return result;
}
// END: https://stackoverflow.com/questions/27220/how-to-convert-stdstring-to-lpcwstr-in-c-unicode*/

void LOut(string str) {
	string s1 = str + "\n";
	std::wstring widestr = std::wstring(s1.begin(), s1.end());
	OutputDebugStringW(widestr.c_str());
}

const wchar_t *LStr(string str) {
	string s1 = str + "\n";
	std::wstring widestr = std::wstring(s1.begin(), s1.end());
	return widestr.c_str();
}

HRESULT MainWindow::CreateDeviceIndependentResources()
{
	static const WCHAR msc_fontName[] = L"Arial";
	static const FLOAT msc_fontSize = 20;
	HRESULT hr;

	// Create a Direct2D factory.
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);

	RECT rc;
	GetClientRect(m_hwnd, &rc);

	D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

	if (SUCCEEDED(hr))
	{

		// Create a DirectWrite factory.
		hr = DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(m_pDWriteFactory),
			reinterpret_cast<IUnknown * *>(&m_pDWriteFactory)
		);
	}
	if (SUCCEEDED(hr))
	{
		// Create a DirectWrite text format object.
		hr = m_pDWriteFactory->CreateTextFormat(
			msc_fontName,
			NULL,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			msc_fontSize,
			L"", //locale
			&m_pTextFormat
		);
	}
	if (SUCCEEDED(hr))
	{
		static const WCHAR msc_fontName2[] = L"Arial";
		static const FLOAT msc_fontSize2 = 16.0;
		// Create a DirectWrite text format object.
		hr = m_pDWriteFactory->CreateTextFormat(
			msc_fontName2,
			NULL,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			msc_fontSize2,
			L"", //locale
			&m_pFontSizesTextFormat
		);
	}
	if (SUCCEEDED(hr))
	{
		string allFontSizes = "";
		for (string fSize : fontSizes)
		{
			allFontSizes += fSize + "\n";
		}
		std::wstring w_allFontSizes = std::wstring(allFontSizes.begin(), allFontSizes.end());
		hr = m_pDWriteFactory->CreateTextLayout(
			w_allFontSizes.c_str(),      // The string to be laid out and formatted.
			w_allFontSizes.size(),  // The length of the string.
			m_pFontSizesTextFormat,  // The text format to apply to the string (contains font information, etc).
			500,         // The width of the layout box.
			size.height,        // The height of the layout box.
			&pFontSizesTextLayout  // The IDWriteTextLayout interface pointer.
		);
		pFontSizesTextLayout->GetMetrics(&fSizeMetrics);
	}
	if (SUCCEEDED(hr))
	{
		static const WCHAR msc_fontName2[] = L"Arial";
		static const FLOAT msc_fontSize2 = 16.0;
		// Create a DirectWrite text format object.
		hr = m_pDWriteFactory->CreateTextFormat(
			msc_fontName2,
			NULL,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			msc_fontSize2,
			L"", //locale
			&m_pFontNamesTextFormat
		);
	}
	if (SUCCEEDED(hr))
	{
		string allFontNames = "";
		for (string fName : fontNames)
		{
			allFontNames += fName + "\n";
		}
		std::wstring w_allFontNames = std::wstring(allFontNames.begin(), allFontNames.end());
		hr = m_pDWriteFactory->CreateTextLayout(
			w_allFontNames.c_str(),      // The string to be laid out and formatted.
			w_allFontNames.size(),  // The length of the string.
			m_pFontNamesTextFormat,  // The text format to apply to the string (contains font information, etc).
			500,         // The width of the layout box.
			size.height,        // The height of the layout box.
			&pFontNameTextLayout  // The IDWriteTextLayout interface pointer.
		);
		pFontNameTextLayout->GetMetrics(&fNameMetrics);
		UINT32 pos = 0;
		for (string fName : fontNames)
		{
			DWRITE_TEXT_RANGE fNameRange = { pos, pos + fName.length() };
			std::wstring w_fName = std::wstring(fName.begin(), fName.end());
			pFontNameTextLayout->SetFontFamilyName(w_fName.c_str(), fNameRange);
			pos += fName.length() + 1; // +1 for \n
		}
	}
	if (SUCCEEDED(hr))
	{
		// Center the text horizontally and vertically.
		m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		m_pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
	}

	for (int i = 32; i <= 126; i++) {
		wchar_t chr = i;
		GlyphSizes[i] = MainWindow::GetCharWidth(chr);
	}

	return hr;
}

class DPIScale
{
	static float scaleX;
	static float scaleY;

public:
	static void Initialize(ID2D1Factory* pFactory)
	{
		FLOAT dpiX, dpiY;
		dpiX = (FLOAT)GetDpiForWindow(GetDesktopWindow());
		dpiY = dpiX;
		scaleX = dpiX / 96.0f;
		scaleY = dpiY / 96.0f;
	}

	template <typename T>
	static D2D1_POINT_2F PixelsToDips(T x, T y)
	{
		return D2D1::Point2F(static_cast<float>(x) / scaleX, static_cast<float>(y) / scaleY);
	}
};

float DPIScale::scaleX = 1.0f;
float DPIScale::scaleY = 1.0f;

// Recalculate drawing layout when the size of the window changes.

void MainWindow::CalculateLayout()
{
    if (pRenderTarget != NULL)
    {
        D2D1_SIZE_F size = pRenderTarget->GetSize();
		hueRect = D2D1::RectF(size.width - 200.0, size.height - 250.0, size.width, size.height - 250.0 - 25.0);
		satValRect1 = D2D1::RectF(size.width - 200.0, size.height - 250.0, size.width, size.height - 50.0);
		satValRect2 = D2D1::RectF(size.width - 200.0, size.height - 250.0, size.width, size.height - 50.0);
		rectColor = D2D1::RectF(size.width - 200.0 + 10.0, size.height - 50.0 + 10.0, size.width - 10.0, size.height - 10.0);
    }
}

HRESULT MainWindow::CreateGraphicsResources()
{
    HRESULT hr = S_OK;
    if (pRenderTarget == NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &pRenderTarget);

        if (SUCCEEDED(hr))
        {
            const D2D1_COLOR_F color = D2D1::ColorF(0.0f, 0.0f, 0.0f);
            hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);

			const D2D1_COLOR_F color2 = D2D1::ColorF(1.0f, 1.0f, 0.8f, 0.5f);
			hr = pRenderTarget->CreateSolidColorBrush(color2, &pPaleYellowBrush);

            if (SUCCEEDED(hr))
            {
                CalculateLayout();
            }
        }
    }
    return hr;
}

void MainWindow::DiscardGraphicsResources()
{
    SafeRelease(&pRenderTarget);
    SafeRelease(&pBrush);
	SafeRelease(&pPaleYellowBrush);
	SafeRelease(&m_pDWriteFactory);
	SafeRelease(&m_pTextFormat);
	SafeRelease(&m_pFontNamesTextFormat);
	SafeRelease(&pFontNameTextLayout);
}

void MainWindow::toggleFormatting(string format, string newValue)
{
	UINT32 startCharIndex = min(startTextPosition, textPosition);
	UINT32 endCharIndex = max(startTextPosition, textPosition);
	vector<UINT32> stopIndices;
	map<UINT32, formatStop>& stops = stopsByType[format];

	for (auto it = stops.begin(); it != stops.end(); ++it)
	{
		stopIndices.push_back(it->first);
	}

	formatStop *startsEnd = new formatStop();
	formatStop *endsStart = new formatStop();
	startsEnd->value = "undefined";
	endsStart->value = "undefined";

	for (auto it = stopIndices.begin(); it != stopIndices.end(); ++it)
	{
		formatStop curStop = stops[*it];
		if (curStop.endIndex < startCharIndex || curStop.startIndex > endCharIndex)
		{
			continue;
		}
		if (curStop.startIndex >= startCharIndex && curStop.endIndex <= endCharIndex) // equal/inside
		{
			stops.erase(*it); // delete stop
		}
		else if (curStop.startIndex <= startCharIndex && curStop.endIndex >= endCharIndex)
		{ // greater on both sides/left/right
			if (curStop.startIndex == startCharIndex)
			{
				stops.erase(*it);
			}
			if (curStop.endIndex > endCharIndex)
			{
				stops[endCharIndex] = formatStop();
				stops[endCharIndex].startIndex = endCharIndex;
				stops[endCharIndex].endIndex = curStop.endIndex;
				stops[endCharIndex].value = curStop.value;
				startsEnd = &stops[endCharIndex];
			}
			if (curStop.startIndex < startCharIndex)
			{
				curStop.endIndex = startCharIndex;
				endsStart = &curStop;
			}
		}
		else if (curStop.startIndex < startCharIndex)
		{ // hugs left
			curStop.endIndex = startCharIndex;
			endsStart = &curStop;
		}
		else if (curStop.endIndex > endCharIndex)
		{
			stops[endCharIndex] = formatStop();
			stops[endCharIndex].startIndex = endCharIndex;
			stops[endCharIndex].endIndex = curStop.endIndex;
			stops[endCharIndex].value = curStop.value;
			startsEnd = &stops[endCharIndex];
			stops.erase(curStop.startIndex);
		}
	}

	//if (!completelyBold)
	{
		formatStop oldStartsEnd = *startsEnd;
		formatStop oldEndsStart = *endsStart;

		if (startsEnd->value != "undefined" && endsStart->value != "undefined")
		{
			if (startsEnd->value == newValue)
			{
				stops.erase(startsEnd->startIndex);
			}
			if (endsStart->value == newValue)
			{
				stops.erase(endsStart->startIndex);
			}

			UINT32 newStartIndex = oldEndsStart.value == newValue ? oldEndsStart.startIndex : startCharIndex;
			stops[newStartIndex] = formatStop();
			stops[newStartIndex].startIndex = newStartIndex;
			stops[newStartIndex].endIndex = oldStartsEnd.value == newValue ? oldStartsEnd.endIndex : endCharIndex;
			stops[newStartIndex].value = newValue;
		}
		else if (startsEnd->value != "undefined")
		{
			if (startsEnd->value == newValue)
			{
				stops.erase(startsEnd->startIndex);
			}
			stops[startCharIndex] = formatStop();
			stops[startCharIndex].startIndex = startCharIndex;
			stops[startCharIndex].endIndex = oldStartsEnd.value == newValue ? oldStartsEnd.endIndex : endCharIndex;
			stops[startCharIndex].value = newValue;
		}
		else if (endsStart->value != "undefined")
		{
			if (endsStart->value == newValue)
			{
				stops[oldEndsStart.startIndex].endIndex = endCharIndex;
			}
			else
			{
				stops[startCharIndex] = formatStop();
				stops[startCharIndex].startIndex = startCharIndex;
				stops[startCharIndex].endIndex = endCharIndex;
				stops[startCharIndex].value = newValue;
			}
		}
		else
		{
			stops[startCharIndex] = formatStop();
			stops[startCharIndex].startIndex = startCharIndex;
			stops[startCharIndex].endIndex = endCharIndex;
			stops[startCharIndex].value = newValue;
		}
	}

	OnPaint(false, true);
}

void MainWindow::OnPaint(bool verticalMove, bool click)
{
	hCursor = LoadCursor(NULL, IDC_IBEAM);
	SetCursor(hCursor);
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);
     
        pRenderTarget->BeginDraw();
        pRenderTarget->Clear( D2D1::ColorF(D2D1::ColorF::White) );
		
		// Create a text layout using the text format.
		D2D1_SIZE_F size = pRenderTarget->GetSize();

		ID2D1GradientStopCollection* pGradientStops = NULL;
		D2D1_GRADIENT_STOP* gradStops = new D2D1_GRADIENT_STOP[7]; // delete later
		gradStops[0].color = D2D1::ColorF(D2D1::ColorF::Red);
		gradStops[0].position = (1.0 - 1.0) / 6.0;
		gradStops[1].color = D2D1::ColorF(D2D1::ColorF::Yellow);
		gradStops[1].position = (2.0 - 1.0) / 6.0;
		gradStops[2].color = D2D1::ColorF(D2D1::ColorF::Lime);
		gradStops[2].position = (3.0 - 1.0) / 6.0;
		gradStops[3].color = D2D1::ColorF(D2D1::ColorF::Cyan);
		gradStops[3].position = (4.0 - 1.0) / 6.0;
		gradStops[4].color = D2D1::ColorF(D2D1::ColorF::Blue);
		gradStops[4].position = (5.0 - 1.0) / 6.0;
		gradStops[5].color = D2D1::ColorF(D2D1::ColorF::Magenta);
		gradStops[5].position = (6.0 - 1.0) / 6.0;
		gradStops[6].color = D2D1::ColorF(D2D1::ColorF::Red);
		gradStops[6].position = (7.0 - 1.0) / 6.0;
		HRESULT hr = pRenderTarget->CreateGradientStopCollection(
			gradStops,
			7,
			D2D1_GAMMA_2_2,
			D2D1_EXTEND_MODE_CLAMP,
			&pGradientStops
		);
		ID2D1LinearGradientBrush* m_pLinearGradientBrush = nullptr;
		if (SUCCEEDED(hr))
		{
			hr = pRenderTarget->CreateLinearGradientBrush(
				D2D1::LinearGradientBrushProperties(
					D2D1::Point2F(size.width - 200.0, 0.0),
					D2D1::Point2F(size.width, 0.0)
				),
				pGradientStops,
				&m_pLinearGradientBrush
			);
			pRenderTarget->FillRectangle(hueRect, m_pLinearGradientBrush);
			m_pLinearGradientBrush->Release();
		}
		SafeRelease(&pGradientStops);
		//SafeRelease(&m_pLinearGradientBrush);
		delete[] gradStops;

		pGradientStops = NULL;
		gradStops = new D2D1_GRADIENT_STOP[2]; // delete later
		gradStops[0].color = D2D1::ColorF(D2D1::ColorF::White, 1.0);
		gradStops[0].position = 0.0;
		gradStops[1].color = hsvToRgb((hueX - (size.width - 200.0)) / 200.0 * 360, 1, 1);
		gradStops[1].position = 1.0;
		hr = pRenderTarget->CreateGradientStopCollection(
			gradStops,
			2,
			D2D1_GAMMA_2_2,
			D2D1_EXTEND_MODE_CLAMP,
			&pGradientStops
		);
		m_pLinearGradientBrush = nullptr;
		if (SUCCEEDED(hr))
		{
			hr = pRenderTarget->CreateLinearGradientBrush(
				D2D1::LinearGradientBrushProperties(
					D2D1::Point2F(size.width - 200.0, 0.0),
					D2D1::Point2F(size.width, 0.0)
				),
				pGradientStops,
				&m_pLinearGradientBrush
			);
			pRenderTarget->FillRectangle(satValRect1, m_pLinearGradientBrush);
			m_pLinearGradientBrush->Release();
		}
		SafeRelease(&pGradientStops);
		//SafeRelease(&m_pLinearGradientBrush);
		delete[] gradStops;

		pGradientStops = NULL;
		gradStops = new D2D1_GRADIENT_STOP[2]; // delete later
		gradStops[0].color = D2D1::ColorF(D2D1::ColorF::Black, 0.0);
		gradStops[0].position = 0.0;
		gradStops[1].color = D2D1::ColorF(D2D1::ColorF::Black, 1.0);
		gradStops[1].position = 1.0;
		hr = pRenderTarget->CreateGradientStopCollection(
			gradStops,
			2,
			D2D1_GAMMA_2_2,
			D2D1_EXTEND_MODE_CLAMP,
			&pGradientStops
		);
		m_pLinearGradientBrush = nullptr;
		if (SUCCEEDED(hr))
		{
			D2D1_SIZE_F size = pRenderTarget->GetSize();
			hr = pRenderTarget->CreateLinearGradientBrush(
				D2D1::LinearGradientBrushProperties(
					D2D1::Point2F(0.0, size.height - 250.0),
					D2D1::Point2F(0.0, size.height - 50.0)
				),
				pGradientStops,
				&m_pLinearGradientBrush
			);
			pRenderTarget->FillRectangle(satValRect2, m_pLinearGradientBrush);
			m_pLinearGradientBrush->Release();
			const D2D1_COLOR_F brushColor = hsvToRgb((hueX - (size.width - 200.0)) / 200.0 * 360, clamp((cursorX - (size.width - 200.0)) / 200.0, 0, 1), clamp(1 - (cursorY - (size.height - 250.0)) / 200.0, 0, 1));
			rgbColor = std::to_string(UINT8(brushColor.r * 255) << 16 | UINT8(brushColor.g * 255) << 8 | UINT8(brushColor.b * 255) << 0);
			hr = pRenderTarget->CreateSolidColorBrush(brushColor, &pColorBrush);
			pRenderTarget->FillRectangle(rectColor, pColorBrush);
		}
		SafeRelease(&pGradientStops);
		//SafeRelease(&m_pLinearGradientBrush);
		delete[] gradStops;

		D2D1_RECT_F rectangle = D2D1::RectF(0.0, 0.0, size.width, 36.0);

		pGradientStops = NULL;
		gradStops = new D2D1_GRADIENT_STOP[2]; // delete later
		gradStops[0].color = D2D1::ColorF(0.62745098f, 0.62745098f, 0.62745098f, 1.0f);
		gradStops[0].position = (1.0 - 1.0) / 1.0;
		gradStops[1].color = D2D1::ColorF(0.5f, 0.5f, 0.5f, 1.0f);
		gradStops[1].position = (2.0 - 1.0) / 1.0;
		hr = pRenderTarget->CreateGradientStopCollection(
			gradStops,
			2,
			D2D1_GAMMA_2_2,
			D2D1_EXTEND_MODE_CLAMP,
			&pGradientStops
		);
		m_pLinearGradientBrush = nullptr;
		if (SUCCEEDED(hr))
		{
			hr = pRenderTarget->CreateLinearGradientBrush(
				D2D1::LinearGradientBrushProperties(
					D2D1::Point2F(0.0, 0.0),
					D2D1::Point2F(0.0, 36.0)
				),
				pGradientStops,
				&m_pLinearGradientBrush
			);
			pRenderTarget->FillRectangle(rectangle, m_pLinearGradientBrush);
			m_pLinearGradientBrush->Release();
		}
		SafeRelease(&pGradientStops);
		//SafeRelease(&m_pLinearGradientBrush);
		delete[] gradStops;

		// Create a bitmap from a file.
		IWICImagingFactory* m_pWICFactory;

		hr = CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&m_pWICFactory)
		);

		if (SUCCEEDED(hr)) {
			for (int i = 0, len = icons.size(); i < len; ++i)
			{
				D2D1_RECT_F bbox = D2D1::RectF(
					i * 36.0,
					0.0,
					(i + 1) * 36.0,
					36.0
				);
				/*if (myAppState.selectedIcon == it->first) {
					pRenderTarget->FillRectangle(&bbox, pBrush);
				}
				else {
					pRenderTarget->FillRectangle(&bbox, pPaleYellowBrush);
				//}*/
				string sFName = "icons/" + icons[i];
				std::wstring wFName(sFName.begin(), sFName.end());
				RenderBitmap(wFName.c_str(), i * 36.0 + 10.0, 0.0 + 10.0, pRenderTarget, m_pWICFactory);
			}
		}

		SafeRelease(&m_pWICFactory);
		
		if (pOpenTextLayout) {
			SafeRelease(&pOpenTextLayout);
		}

		if (alignment == "left")
		{
			m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
		}
		else if (alignment == "center")
		{
			m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
		}
		else if (alignment == "right")
		{
			m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
		}
		else if (alignment == "justify")
		{
			m_pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_JUSTIFIED);
		}

		std::wstring w_userText = std::wstring(userText.begin(), userText.end());

		hr = m_pDWriteFactory->CreateTextLayout(
			w_userText.c_str(),      // The string to be laid out and formatted.
			w_userText.size(),  // The length of the string.
			m_pTextFormat,  // The text format to apply to the string (contains font information, etc).
			500,         // The width of the layout box.
			size.height,        // The height of the layout box.
			&pOpenTextLayout  // The IDWriteTextLayout interface pointer.
		);

		map<UINT32, formatStop>	&boldStops = stopsByType["Bold"];
		for (auto it = boldStops.begin(); it != boldStops.end(); ++it)
		{
			DWRITE_TEXT_RANGE boldRange = { it->second.startIndex, it->second.endIndex - it->second.startIndex };
			pOpenTextLayout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, boldRange);
		}
		map<UINT32, formatStop>& italicStops = stopsByType["Italic"];
		for (auto it = italicStops.begin(); it != italicStops.end(); ++it)
		{
			DWRITE_TEXT_RANGE italicRange = { it->second.startIndex, it->second.endIndex - it->second.startIndex };
			pOpenTextLayout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, italicRange);
		}
		map<UINT32, formatStop>& underlineStops = stopsByType["Underline"];
		for (auto it = underlineStops.begin(); it != underlineStops.end(); ++it)
		{
			DWRITE_TEXT_RANGE underlineRange = { it->second.startIndex, it->second.endIndex - it->second.startIndex };
			pOpenTextLayout->SetUnderline(true, underlineRange);
		}
		map<UINT32, formatStop>& strikethroughStops = stopsByType["Strikethrough"];
		for (auto it = strikethroughStops.begin(); it != strikethroughStops.end(); ++it)
		{
			DWRITE_TEXT_RANGE strikethroughRange = { it->second.startIndex, it->second.endIndex - it->second.startIndex };
			pOpenTextLayout->SetStrikethrough(true, strikethroughRange);
		}
		map<UINT32, formatStop>& foreColorStops = stopsByType["ForeColor"];
		for (auto it = foreColorStops.begin(); it != foreColorStops.end(); ++it)
		{
			DWRITE_TEXT_RANGE foreColorRange = { it->second.startIndex, it->second.endIndex - it->second.startIndex };
			const D2D1_COLOR_F color = D2D1::ColorF(stoi(it->second.value));
			ID2D1SolidColorBrush *pColorBrush;
			hr = pRenderTarget->CreateSolidColorBrush(color, &pColorBrush);
			if (SUCCEEDED(hr))
			{
				pOpenTextLayout->SetDrawingEffect(pColorBrush, foreColorRange);
				SafeRelease(&pColorBrush);
			}
		}
		map<UINT32, formatStop>& fontNameStops = stopsByType["FontName"];
		for (auto it = fontNameStops.begin(); it != fontNameStops.end(); ++it)
		{
			DWRITE_TEXT_RANGE fontNameRange = { it->second.startIndex, it->second.endIndex - it->second.startIndex };
			std::wstring w_value = std::wstring(it->second.value.begin(), it->second.value.end());
			pOpenTextLayout->SetFontFamilyName(w_value.c_str(), fontNameRange);
		}
		map<UINT32, formatStop>& fontSizeStops = stopsByType["FontSize"];
		for (auto it = fontSizeStops.begin(); it != fontSizeStops.end(); ++it)
		{
			DWRITE_TEXT_RANGE fontSizeRange = { it->second.startIndex, it->second.endIndex - it->second.startIndex };
			std::wstring w_value = std::wstring(it->second.value.begin(), it->second.value.end());
			pOpenTextLayout->SetFontSize(std::stof(w_value.c_str()), fontSizeRange);
		}
		/*map<UINT32, formatStop>& scriptStops = stopsByType["Script"];
		for (auto it = scriptStops.begin(); it != scriptStops.end(); ++it)
		{
			DWRITE_TEXT_RANGE scriptRange = { it->second.startIndex, it->second.endIndex - it->second.startIndex };
			if (it->second.value == "super")
			{
				pOpenTextLayout->SetFontSize(10.0, scriptRange);
				pOpenTextLayout->SetDrawingEffect(pPaleYellowBrush, scriptRange);
			}
			else if (it->second.value == "sub")
			{
				pOpenTextLayout->SetFontSize(10.0, scriptRange);
			}
		}*/

		UpdateCaretFromIndex(verticalMove);
		if (click && isTrailingHit == 1 && hitTestMetrics.left == 0.0) {
			--textPosition;
			UpdateCaretFromIndex(verticalMove);
		}

		/*pRenderTarget->DrawTextLayout(
			toolbarOrigin,
			pOpenTextLayout,
			pBrush
		);*/

		pRenderTarget->DrawTextLayout(
			D2D1::Point2F(toolbarOrigin.x + 14.0 * 36.0, toolbarOrigin.y),
			pFontNameTextLayout,
			pBrush
		);
		pRenderTarget->DrawTextLayout(
			D2D1::Point2F(toolbarOrigin.x + 14.0 * 36.0 + fNameMetrics.width, toolbarOrigin.y),
			pFontSizesTextLayout,
			pBrush
		);

		pRenderTarget->DrawLine(
			D2D1::Point2F(
				toolbarOrigin.x + hitTestMetrics.left,
				toolbarOrigin.y + hitTestMetrics.top
			),
			D2D1::Point2F(
				toolbarOrigin.x + hitTestMetrics.left,
				toolbarOrigin.y + hitTestMetrics.top + hitTestMetrics.height
			),
			pBrush
		);

		int bitmapWidth = 500;
		int bitmapHeight = size.height - toolbarOrigin.y;
		unsigned char* bitmap = new unsigned char[bitmapWidth * bitmapHeight * 4];
		for (int i = 0, len = bitmapWidth * bitmapHeight * 4; i < len; i+=4) {
			bitmap[i] = 255;
			bitmap[i + 1] = 255;
			bitmap[i + 2] = 255;
			bitmap[i + 3] = 255;
		}

		const char* str = userText.c_str();
		int x = 0;
		int y = 0;

		vector<string> words;
		int absCharIndex = 0;
		splitString(userText, ' ', words);
		float lineHeight = 0.0;
		for (
			size_t wordIndex = 0, wordCount = words.size();
			wordIndex < wordCount;
			wordIndex++
			) {
			float wordWidth = 0.0;
			string word = words[wordIndex] + " ";
			for (
				size_t charIndex = 0, charCount = word.size();
				charIndex < charCount;
				charIndex++, absCharIndex++
				) {
				wordWidth += glyphWidths[word[charIndex]-32];
				lineHeight = 21.0;
				if (x >= 500) {
					x = wordWidth - glyphWidths[word[charIndex] - 32];
					y += lineHeight;
				}
				x += glyphWidths[word[charIndex] - 32];
			}
			x -= wordWidth;
			const char* str = word.c_str();
			do {
				x += drawGlyph(*str, x, y, COLOR32(0, 0, 0, 255), bitmap, bitmapWidth, bitmapWidth * bitmapHeight * 4);
			} while (*str++);
		}
		
		ID2D1Bitmap* pBitmap = NULL;
		hr = pRenderTarget->CreateBitmap(D2D1::SizeU(bitmapWidth, bitmapHeight), bitmap, bitmapWidth * 4, D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &pBitmap);

		// Draw a bitmap.
		pRenderTarget->DrawBitmap(
			pBitmap,
			D2D1::RectF(
				toolbarOrigin.x,
				toolbarOrigin.y,
				bitmapWidth,
				bitmapHeight
			),
			1.0
		);

		SafeRelease(&pBitmap);
		delete[] bitmap;

		//if (isSelecting)
		//{
		SelectText();
		//}

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::Resize()
{
    if (pRenderTarget != NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        pRenderTarget->Resize(size);
        CalculateLayout();
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
	MainWindow win;

    if (!win.Create(L"RichTextEditor", WS_OVERLAPPEDWINDOW))
    {
        return 0;
    }

    ShowWindow(win.Window(), nCmdShow);

    // Run the message loop.

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

void MainWindow::UpdateCaretFromPosition(bool verticalMove, bool click, bool paint)
{
	if (pOpenTextLayout) {
		BOOL isInside;
		vector<string> words;
		float x = 0;
		float y = 0.0;
		int absCharIndex = 0;
		splitString(userText, ' ', words);
		float lineHeight = 0.0;
		for (
			size_t wordIndex = 0, wordCount = words.size();
			wordIndex < wordCount;
			wordIndex++
			) {
			float wordWidth = 0.0;
			string word = words[wordIndex] + " ";
			for (
				size_t charIndex = 0, charCount = word.size();
				charIndex < charCount;
				charIndex++, absCharIndex++
				) {
				GlyphSize glyphSize;
				glyphSize.width = glyphWidths[word[charIndex] - 32];
				glyphSize.height = 21.0;
				wordWidth += glyphSize.width;
				lineHeight = 21.0;
				if (x >= 500) {
					x = wordWidth - glyphWidths[' ' - 32];
					y += lineHeight;
				}
				if (
					x >= -toolbarOrigin.x + cursorX &&
					y + 14.0 >= -toolbarOrigin.y + cursorY
					) {
					hitTestMetrics.left = x;
					hitTestMetrics.top = y;
					hitTestMetrics.height = 21.0;
					hitTestMetrics.textPosition = absCharIndex;
					goto breakInnerLoop;
				}
				x += glyphSize.width;
			}
		}
	breakInnerLoop: {}
	
		textPosition = hitTestMetrics.textPosition + (isTrailingHit ? 1 : 0);
		if (!verticalMove)
		{
			caretX = toolbarOrigin.x + cursorX;
		}
		cursorY = toolbarOrigin.y + hitTestMetrics.top;
		if (paint)
		{
			OnPaint(verticalMove, click);
		}
	}
}

void MainWindow::UpdateCaretFromIndex(bool verticalMove)
{
	if (pOpenTextLayout) {
		/*MainWindow::HitTestTextPosition(
			textPosition,
			isTrailingHit,
			&cursorX,
			&cursorY,
			&hitTestMetrics
		);*/
		cursorX += toolbarOrigin.x;
		cursorY += toolbarOrigin.y;
		if (!verticalMove)
		{
			caretX = cursorX;
		}
		cursorY = hitTestMetrics.top + toolbarOrigin.y;
	}
}

void MainWindow::HitTestTextPosition(
	UINT32 textPosition,
	bool isTrailing,
	float* pointX,
	float* pointY,
	HitTestMetrics* metrics) {
	vector<string> words;
	float x = 0;
	float y = 0.0;
	UINT32 absCharIndex = 0;
	splitString(userText, ' ', words);
	float lineHeight = 0.0;
	for (
		size_t wordIndex = 0, wordCount = words.size();
		wordIndex < wordCount;
		wordIndex++
		) {
		float wordWidth = 0.0;
		string word = words[wordIndex] + " ";
		for (
			size_t charIndex = 0, charCount = word.size();
			charIndex < charCount;
			charIndex++, absCharIndex++
			) {
			GlyphSize glyphSize = GlyphSizes[word[charIndex]];
			wordWidth += glyphSize.width;
			lineHeight = 21.0;
			if (x >= 500) {
				x = wordWidth - GlyphSizes[' '].width;
				y += lineHeight;
			}
			if (
				absCharIndex >= textPosition
				) {
				metrics->left = x;
				metrics->top = y;
				metrics->width = glyphSize.width;
				metrics->height = 21.0;
				metrics->textPosition = absCharIndex;
				goto breakInnerLoop;
			}
			x += glyphSize.width;
		}
	}
breakInnerLoop: {};
}

void MainWindow::SelectText()
{
	if (pRenderTarget && pOpenTextLayout)
	{
		UINT32 startCharIndex = min(startTextPosition, textPosition);
		UINT32 endCharIndex = max(startTextPosition, textPosition);
		DWRITE_TEXT_METRICS textMetrics;
		UINT32 actualLineCount;
		pOpenTextLayout->GetMetrics(&textMetrics);
		UINT32 lineCount = textMetrics.lineCount;
		vector<DWRITE_LINE_METRICS> lineMetrics(lineCount);
		pOpenTextLayout->GetLineMetrics(lineMetrics.data(), lineMetrics.size(), &actualLineCount);

		UINT32 curIndex = 0;
		for (auto& metric : lineMetrics)
		{
			HitTestMetrics startMetrics;
			HitTestMetrics endMetrics;
			float sx;
			float sy;
			float ex;
			float ey;
			if (startCharIndex >= curIndex && startCharIndex < (curIndex + metric.length) && endCharIndex >= curIndex && endCharIndex < (curIndex + metric.length))
			{
				MainWindow::HitTestTextPosition(
					startCharIndex,
					false,
					&sx,
					&sy,
					&startMetrics
				);
				MainWindow::HitTestTextPosition(
					endCharIndex,
					false,
					&ex,
					&ey,
					&endMetrics
				);
				D2D1_RECT_F startRect = D2D1::RectF(startMetrics.left, toolbarOrigin.y + startMetrics.top, toolbarOrigin.x + endMetrics.left, toolbarOrigin.y + startMetrics.top + startMetrics.height);
				pRenderTarget->FillRectangle(&startRect, pPaleYellowBrush);
			}
			else if (startCharIndex >= curIndex && startCharIndex < (curIndex + metric.length))
			{ // on the start line
				MainWindow::HitTestTextPosition(
					startCharIndex,
					false,
					&sx,
					&sy,
					&startMetrics
				);
				MainWindow::HitTestTextPosition(
					curIndex + metric.length - 1,
					true,
					&ex,
					&ey,
					&endMetrics
				);
				D2D1_RECT_F startRect = D2D1::RectF(startMetrics.left, toolbarOrigin.y + startMetrics.top, toolbarOrigin.x + endMetrics.left + endMetrics.width, toolbarOrigin.y + startMetrics.top + startMetrics.height);
				pRenderTarget->FillRectangle(&startRect, pPaleYellowBrush);
			}
			else if (curIndex > startCharIndex)
			{ // after the start line
				if (endCharIndex >= curIndex && endCharIndex < (curIndex + metric.length))
				{ // on the end line
					MainWindow::HitTestTextPosition(
						curIndex,
						false,
						&sx,
						&sy,
						&startMetrics
					);
					MainWindow::HitTestTextPosition(
						endCharIndex,
						false,
						&ex,
						&ey,
						&endMetrics
					);
					D2D1_RECT_F endRect = D2D1::RectF(toolbarOrigin.x + startMetrics.left, toolbarOrigin.y + startMetrics.top, toolbarOrigin.x + endMetrics.left, toolbarOrigin.y + endMetrics.top + endMetrics.height);
					pRenderTarget->FillRectangle(&endRect, pPaleYellowBrush);
				}
				else if ((curIndex + metric.length) <= endCharIndex)
				{ // between start and end lines
					MainWindow::HitTestTextPosition(
						curIndex,
						false,
						&sx,
						&sy,
						&startMetrics
					);
					MainWindow::HitTestTextPosition(
						curIndex + metric.length - 1,
						true,
						&ex,
						&ey,
						&endMetrics
					);
					D2D1_RECT_F endRect = D2D1::RectF(toolbarOrigin.x + startMetrics.left, toolbarOrigin.y + startMetrics.top, toolbarOrigin.x + endMetrics.left + endMetrics.width, toolbarOrigin.y + endMetrics.top + endMetrics.height);
					pRenderTarget->FillRectangle(&endRect, pPaleYellowBrush);
				}
			}
			curIndex += metric.length;
		}
	}
}

void MainWindow::OnLButtonDown(int pixelX, int pixelY, DWORD flags)
{
	const D2D1_POINT_2F dips = DPIScale::PixelsToDips(pixelX, pixelY);
	cursorX = dips.x;
	cursorY = dips.y;

	RECT rc;
	GetClientRect(m_hwnd, &rc);

	D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);
	
	if (cursorX >= (toolbarOrigin.x + 14.0 * 36.0 + fNameMetrics.left) && cursorX <= (toolbarOrigin.x + 14.0 * 36.0 + fNameMetrics.left + fNameMetrics.width) && cursorY >= (toolbarOrigin.y + fNameMetrics.top) && cursorY <= (toolbarOrigin.y + fNameMetrics.top + fNameMetrics.height))
	{
		toggleFormatting("FontName", fontNames[(cursorY - (fNameMetrics.top + toolbarOrigin.y)) / (fNameMetrics.height / fontNames.size())]);
		return;
	}

	if (cursorX >= (toolbarOrigin.x + 14.0 * 36.0 + fNameMetrics.width + fSizeMetrics.left) && cursorX <= (toolbarOrigin.x + 14.0 * 36.0 + fNameMetrics.width + fSizeMetrics.left + fSizeMetrics.width) && cursorY >= (toolbarOrigin.y + fSizeMetrics.top) && cursorY <= (toolbarOrigin.y + fSizeMetrics.top + fSizeMetrics.height))
	{
		toggleFormatting("FontSize", fontSizes[(cursorY - (fSizeMetrics.top + toolbarOrigin.y)) / (fSizeMetrics.height / fontSizes.size())]);
		return;
	}

	if (cursorX >= (size.width - 200.0) && cursorX <= size.width && cursorY >= (size.height - 250.0 - 25.0) && cursorY <= (size.height - 250.0))
	{
		hueX = dips.x;
		OnPaint();
		return;
	}

	if (cursorX >= (size.width - 200.0) && cursorX <= size.width && cursorY >= (size.height - 250.0) && cursorY <= (size.height))
	{
		toggleFormatting("ForeColor", rgbColor);
		OnPaint();
		return;
	}

	if (cursorY >= 0.0 && cursorY <= 36.0)
	{
		int iconIndex = cursorX / 36;
		if (iconIndex == 0)
		{
			toggleFormatting("Bold", "true");
		}
		else if (iconIndex == 1)
		{
			toggleFormatting("Italic", "true");
		}
		else if (iconIndex == 2)
		{
			toggleFormatting("Underline", "true");
		}
		else if (iconIndex == 3)
		{
			toggleFormatting("Strikethrough", "true");
		}
		/*else if (iconIndex == 4)
		{
			toggleFormatting("Script", "super");
		}
		else if (iconIndex == 5)
		{
			toggleFormatting("Script", "sub");
		}*/
		else if (iconIndex == 4)
		{
			alignment = "left";
			OnPaint();
		}
		else if (iconIndex == 5)
		{
			alignment = "center";
			OnPaint();
		}
		else if (iconIndex == 6)
		{
			alignment = "right";
			OnPaint();
		}
		else if (iconIndex == 7)
		{
			alignment = "justify";
			OnPaint();
		}
		return;
	}

	UpdateCaretFromPosition(false, true, false);
	startTextPosition = textPosition;
	OnPaint();
	isSelecting = true;
}

void MainWindow::OnLButtonUp()
{
	isSelecting = false;
}

void MainWindow::OnMouseMove(int pixelX, int pixelY, DWORD flags)
{
	const D2D1_POINT_2F dips = DPIScale::PixelsToDips(pixelX, pixelY);
	cursorX = dips.x;
	cursorY = dips.y;

	RECT rc;
	GetClientRect(m_hwnd, &rc);

	D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

	if (cursorX >= (size.width - 200.0) && cursorX <= size.width && cursorY >= (size.height - 250.0) && cursorY <= (size.height)) {
		OnPaint();
		return;
	}

	if (isSelecting)
	{
		UpdateCaretFromPosition(false, true);
	}
}

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        if (FAILED(D2D1CreateFactory(
                D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
        {
            return -1;  // Fail CreateWindowEx.
        }
		CreateDeviceIndependentResources();
        return 0;

    case WM_DESTROY:
        DiscardGraphicsResources();
        SafeRelease(&pFactory);
		if (pOpenTextLayout) {
			SafeRelease(&pOpenTextLayout);
		}
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        OnPaint();
        return 0;

	case WM_MOUSEMOVE:
		OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
		return 0;

    case WM_SIZE:
        Resize();
        return 0;

	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_LEFT:
			if (textPosition == 0)
			{
				return 0;
			}
			--textPosition;
			OnPaint();
			break;
		case VK_UP:
			cursorX = caretX;
			cursorY = max(0.0, cursorY - hitTestMetrics.height);
			UpdateCaretFromPosition(true);
			break;
		case VK_RIGHT:
			if (textPosition == userText.size())
			{
				return 0;
			}
			++textPosition;
			OnPaint();
			break;
		case VK_DOWN:
			cursorX = caretX;
			cursorY = cursorY + hitTestMetrics.height;
			UpdateCaretFromPosition(true);
			break;
		case VK_DELETE:
			if (textPosition == userText.size())
			{
				return 0;
			}
			userText = userText.substr(0, textPosition) + userText.substr(textPosition + 1);

			for (auto i = stopsByType.begin(); i != stopsByType.end(); ++i)
			{
				vector<UINT32> stopIndices;
				map<UINT32, formatStop>& stops = i->second;
				for (auto j = stops.begin(); j != stops.end(); ++j)
				{
					stopIndices.push_back(j->first);
				}
				for (auto stopIndex : stopIndices)
				{
					formatStop& curStop = stops[stopIndex];
					if (curStop.endIndex > textPosition)
					{
						--curStop.endIndex;
					}
					if (stopIndex > textPosition)
					{
						--curStop.startIndex;
						stops[stopIndex - 1] = curStop;
						stops.erase(stopIndex);
					}
					if (curStop.startIndex == curStop.endIndex)
					{
						stops.erase(curStop.startIndex);
					}
				}
			}

			OnPaint();
			break;
		case VK_BACK:
			if (textPosition == 0)
			{
				return 0;
			}
			userText = userText.substr(0, textPosition - 1) + userText.substr(textPosition);
			for (auto i = stopsByType.begin(); i != stopsByType.end(); ++i)
			{
				vector<UINT32> stopIndices;
				map<UINT32, formatStop>& stops = i->second;
				for (auto j = stops.begin(); j != stops.end(); ++j)
				{
					stopIndices.push_back(j->first);
				}
				for (auto stopIndex : stopIndices)
				{
					formatStop& curStop = stops[stopIndex];
					if (curStop.endIndex >= textPosition)
					{
						--curStop.endIndex;
					}
					if (stopIndex >= textPosition)
					{
						--curStop.startIndex;
						stops[stopIndex - 1] = curStop;
						stops.erase(stopIndex);
					}
					if (curStop.startIndex == curStop.endIndex)
					{
						stops.erase(curStop.startIndex);
					}
				}
			}
			--textPosition;
			OnPaint();
			break;
		}
		return 0;

	case WM_CHAR:
		switch (wParam)
		{
		/*case VK_LEFT: case VK_UP: case VK_RIGHT: */case VK_BACK:
			return 0; break;
		default:
			break;
		}
		userText = userText.substr(0, textPosition) + (char)wParam + userText.substr(textPosition);

		for (auto i = stopsByType.begin(); i != stopsByType.end(); ++i)
		{
			vector<UINT32> stopIndices;
			map<UINT32, formatStop>& stops = i->second;
			for (auto j = stops.begin(); j != stops.end(); ++j)
			{
				stopIndices.push_back(j->first);
			}
			for (auto stopIndex : stopIndices)
			{
				formatStop& curStop = stops[stopIndex];
				if (curStop.endIndex > textPosition)
				{
					++curStop.endIndex;
				}
				if (stopIndex > textPosition)
				{
					++curStop.startIndex;
					stops[stopIndex + 1] = curStop;
					stops.erase(stopIndex);
				}
				if (curStop.startIndex == curStop.endIndex)
				{
					stops.erase(curStop.startIndex);
				}
			}
		}

		++textPosition;
		OnPaint();
		return 0;

	case WM_LBUTTONDOWN:
		OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
		return 0;

	case WM_LBUTTONUP:
		OnLButtonUp();
		return 0;

	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT)
		{
			SetCursor(hCursor);
			return TRUE;
		}
		break;
    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}