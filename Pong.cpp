/*
	Pong
*/

//  includes
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xaudio2.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <immintrin.h>

//  defines
#define WINDOW_NAME L"Pong"
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480
#define GAME_BPP 32
#define TARGET_MICROSECONDS 16667ULL / 24ULL
#define FONT_SHEET_CHARACTERS_PER_ROW 98
#define DEFAULT_BRIGHTNESS 0
#define SFX_SOURCE_VOICES 4
#define GAME_FRAME_SIZE ((WINDOW_WIDTH * WINDOW_HEIGHT) * (GAME_BPP / 8))
#define KEY_DOWN(VK_CODE) ((GetAsyncKeyState(VK_CODE) & 0x8000) ? 1 : 0)
#define MEMORY_OFFSET(POSX, POSY, X, Y) ((((WINDOW_WIDTH * WINDOW_HEIGHT) - WINDOW_WIDTH) - (WINDOW_WIDTH * POSY) + POSX) + X - (WINDOW_WIDTH * Y))
#define BITMAP_MEMORY_OFFSET(POSX, POSY, X, Y) (((POSX * POSY) - POSX) + X - (POSX * Y))
#define STARTING_FONT_SHEET_BYTE(WIDTH, HEIGHT, CHAR_WIDTH, N) ((WIDTH * HEIGHT) - WIDTH + (CHAR_WIDTH * N))
#define FONT_SHEET_OFFSET(BYTE, WIDTH, X, Y) (BYTE + X - (WIDTH * Y))
#define STRING_BITMAP_OFFSET(N, CHAR_WIDTH, WIDTH, HEIGHT, X, Y) ((N * CHAR_WIDTH) + ((WIDTH * HEIGHT) - WIDTH) + X - (WIDTH * Y))
//  font bitmap file names
#define FONT_BITMAP L"6x7font.bmpx"
//  wav sfx file names
#define MENU_NAVIGATE_SFX L"MenuNavigate.wav"

//  enums
enum GAME_STATE { START_SCREEN, MAIN_GAME, MENU_SCREEN };

//  structs
typedef struct GAME_BITMAP_T {
	BITMAPINFO BitMapInfo;
	void *Memory;
} GAMEBITMAP;

typedef struct GAME_PERFORMANCE_DATA_T {
	LARGE_INTEGER FramesRendered;
	LARGE_INTEGER Frequency;
	LARGE_INTEGER BeginFrame;
	LARGE_INTEGER EndFrame;
	LARGE_INTEGER ElapsedMicroseconds;
	LARGE_INTEGER ElapsedMicrosecondsAcc;
	BOOL DisplayDebug;
	float AverageFPS;
	uint16_t MonitorWidth;
	uint16_t MonitorHeight;
} GAMEPERFORMANCEDATA;

typedef struct PIXEL_32_T {
	uint8_t Blue;
	uint8_t Green;
	uint8_t Red;
	uint8_t Alpha;
} PIXEL32;

typedef struct BALL_T {
	uint16_t X;
	uint16_t Y;
	uint16_t DX;
	uint16_t DY;
} BALL;

typedef struct PLAYER_OBJECT_T {
	uint16_t X;
	uint16_t Y;
	uint16_t Score;
} PLAYER;

typedef struct GAME_SOUND_T {
	WAVEFORMATEX WaveFormat;
	XAUDIO2_BUFFER Buffer;
} GAMESOUND;

typedef struct BACKGROUND_OBJECT_T {
	uint16_t MiddleBarPosX;
	uint16_t MiddleBarPosY;
	uint16_t TopBarPosX;
	uint16_t TopBarPosY;
	uint16_t BottomBarPosX;
	uint16_t BottomBarPosY;
} BACKGROUND;

//  globals
GAMEBITMAP BackBuffer, FontSheetBuffer;
GAMEPERFORMANCEDATA GamePerformanceData;
MONITORINFO MonitorInfo;
BALL Ball;
PLAYER Player1 , Player2;
IXAudio2 *XAudio;
IXAudio2MasteringVoice *XAudioMasteringVoice;
IXAudio2SourceVoice *XAudioSFXSourceVoice[SFX_SOURCE_VOICES];
GAMESOUND Sfx;
BACKGROUND Background;
GAME_STATE GameState;

//  prototypes
void Init(void);
void RenderGraphics(_In_ HWND hGameWnd);
void ClearScreen(void);
void PlayerInput(_In_ HWND hGameWnd);
void DrawGameObjetcs(void);
void CollisionDetection(void);
void DisplayScore(void);
void DrawBackground(void);
void AIMovement(void);
void DrawStartScreen(_In_ HWND hGameWnd);
void DrawMenuScreen(_In_ HWND hGameWnd);
void DrawDebug(void);
void LoadBitmapFromFile(_In_ LPCWSTR Filename, _Inout_ GAMEBITMAP *Bitmap);
void BlitBitmapImage(_In_ GAMEBITMAP *BitMap, _In_ uint16_t XPixel, _In_ uint16_t YPixel, _In_ int16_t Brightness);
void BlitBitmapString(_In_ const char *String, _In_ GAMEBITMAP *BitMap, _In_ PIXEL32 *Color, _In_ uint16_t XPixel, _In_ uint16_t YPixel, _In_ int16_t Brightness);
void InitSoundEngine(void);
void LoadWavFromFile(_In_ LPCWSTR FileName, _Inout_ GAMESOUND *GameSound);
void PlaySFX(_In_ GAMESOUND *GameSound);
LRESULT CALLBACK WndProc(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam);

//  functions
void Init(void)
{
	GameState = START_SCREEN;

	Ball.X = WINDOW_WIDTH / 2;
	Ball.Y = WINDOW_HEIGHT / 2;
	Ball.DX = 2;
	Ball.DY = 1;

	Player1.X = 120;
	Player1.Y = WINDOW_HEIGHT / 2;
	Player1.Score = 0;

	Player2.X = 520;
	Player2.Y = WINDOW_HEIGHT / 2;
	Player2.Score = 0;

	Background.MiddleBarPosX = WINDOW_WIDTH / 2;
	Background.MiddleBarPosY = 0;
	Background.TopBarPosX = 0;
	Background.TopBarPosY = 0;
	Background.BottomBarPosX = 0;
	Background.BottomBarPosY = WINDOW_HEIGHT - 4;

	//  load font bitmap file
	LoadBitmapFromFile(FONT_BITMAP, &FontSheetBuffer);
	//  load wave files
	LoadWavFromFile(MENU_NAVIGATE_SFX, &Sfx);
}

void RenderGraphics(_In_ HWND hGameWnd)
{
	HDC hdc;

	ZeroMemory(&hdc, sizeof(HDC));

	ClearScreen();
	DrawBackground();
	DrawGameObjetcs();
	DisplayScore();

	if (GamePerformanceData.DisplayDebug) {
		DrawDebug();
	}

	hdc = GetDC(hGameWnd);
	StretchDIBits(hdc, 0, 0, GamePerformanceData.MonitorWidth, GamePerformanceData.MonitorHeight, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, BackBuffer.Memory, &BackBuffer.BitMapInfo, DIB_RGB_COLORS, SRCCOPY);
	ReleaseDC(hGameWnd, hdc);
}

void ClearScreen(void)
{
	//  clear the screen 8 pixels at a time
	__m256i OctoPixel = {	//  black background
							0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 
							0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF
						};

	for (uint16_t i = 0; i < (WINDOW_WIDTH * WINDOW_HEIGHT) / 8; i++) {
		_mm256_store_si256((__m256i*) BackBuffer.Memory + i, OctoPixel);
	}
}

void PlayerInput(_In_ HWND hGameWnd)
{
	static uint16_t  DebugKeyWasDown = 0;

	if (KEY_DOWN(VK_ESCAPE)) {
		GameState = MENU_SCREEN;
		PlaySFX(&Sfx);
	}

	if (KEY_DOWN(VK_F1) && !DebugKeyWasDown) {
		GamePerformanceData.DisplayDebug = !GamePerformanceData.DisplayDebug;
		PlaySFX(&Sfx);
	}

	if (KEY_DOWN(VK_UP)) {
		if (Player1.Y > 0) {
			Player1.Y--;
		}
	}

	if (KEY_DOWN(VK_DOWN)) {
		if (Player1.Y < WINDOW_HEIGHT - 64) {
			Player1.Y++;
		}
	}

	DebugKeyWasDown = KEY_DOWN(VK_F1);
}

void DrawGameObjetcs(void)
{
	PIXEL32 White = { 0xFF, 0xFF, 0xFF, 0xFF };

	//  draw the ball to the backbuffer
	for (uint8_t y = 0; y < 16; y++) {
		for (uint8_t x = 0; x < 16; x++) {
			memcpy_s((PIXEL32*) BackBuffer.Memory + MEMORY_OFFSET(Ball.X, Ball.Y, x, y), sizeof(PIXEL32), &White, sizeof(PIXEL32));
		}
	}

	//  draw player1 to the backbuffer
	for (uint8_t y = 0; y < 64; y++) {
		for (uint8_t x = 0; x < 16; x++) {
			memcpy_s((PIXEL32*) BackBuffer.Memory + MEMORY_OFFSET(Player1.X, Player1.Y, x, y), sizeof(PIXEL32), &White, sizeof(PIXEL32));
		}
	}

	//  draw player2 to the backbuffer
	for (uint8_t y = 0; y < 64; y++) {
		for (uint8_t x = 0; x < 16; x++) {
			memcpy_s((PIXEL32*) BackBuffer.Memory + MEMORY_OFFSET(Player2.X, Player2.Y, x, y), sizeof(PIXEL32), &White, sizeof(PIXEL32));
		}
	}
}

void CollisionDetection(void)
{
	//  move the ball every frame
	Ball.X += Ball.DX;
	Ball.Y += Ball.DY;

	//  ball collision detection
	if (Ball.Y == 4) {						//  top of the screen
		Ball.DY = -Ball.DY;
		Ball.Y += Ball.DY;
		PlaySFX(&Sfx);
	}

	if (Ball.Y == WINDOW_HEIGHT - 20) {		//  bottom of the screen
		Ball.DY = -Ball.DY;
		Ball.Y += Ball.DY;
		PlaySFX(&Sfx);
	}

	if (Ball.X == 0) {						//  left side of the screen
		Ball.DX = -Ball.DX;
		Ball.X += Ball.DX;
		Player2.Score++;
		PlaySFX(&Sfx);
	}

	if (Ball.X == WINDOW_WIDTH - 16) {		//  right side of the screen
		Ball.DX = -Ball.DX;
		Ball.X += Ball.DX;
		Player1.Score++;
		PlaySFX(&Sfx);
	}

	//  player collision detection
	if (Ball.X == Player1.X + 16 || Ball.X == Player1.X - 16) {		//  left and right sides of player 1
		for (uint8_t i = 0; i <= 64; i++) {
			if (Ball.Y == Player1.Y + i) {
				Ball.DX = -Ball.DX;
				Ball.X += Ball.DX;
				PlaySFX(&Sfx);
			}
		}
	}
	
	if (Ball.Y == Player1.Y - 16 || Ball.Y == Player1.Y + 80) {		//  top and bottom sides of player 1
		for (uint8_t i = 0; i <= 16; i++) {
			if (Ball.X == Player1.X + i) {
				Ball.DY = -Ball.DY;
				Ball.Y += Ball.DY;
				PlaySFX(&Sfx);
			}
		}
	}

	if (Ball.X == Player2.X - 16 || Ball.X == Player2.X + 16) {		//  left and right sides of player 2
		for (uint8_t i = 0; i <= 64; i++) {
			if (Ball.Y == Player2.Y + i) {
				Ball.DX = -Ball.DX;
				Ball.X += Ball.DX;
				PlaySFX(&Sfx);
			}
		}
	}
	
	if (Ball.Y == Player2.Y - 16 || Ball.Y == Player2.Y + 80) {		//  top and bottom sides of player 2
		for (uint8_t i = 0; i <= 16; i++) {
			if (Ball.X == Player2.X + i) {
				Ball.DY = -Ball.DY;
				Ball.Y += Ball.DY;
				PlaySFX(&Sfx);
			}
		}
	}
}

void DisplayScore(void)
{
	char ScoreString[8];
	PIXEL32 White = { 0xFF, 0xFF, 0xFF, 0xFF };

	memset(ScoreString, 0, sizeof(ScoreString));

	sprintf_s(ScoreString, _countof(ScoreString), "%d", Player1.Score);
	BlitBitmapString(ScoreString, &FontSheetBuffer, &White, 160, 100, DEFAULT_BRIGHTNESS);

	sprintf_s(ScoreString, _countof(ScoreString), "%d", Player2.Score);
	BlitBitmapString(ScoreString, &FontSheetBuffer, &White, 480, 100, DEFAULT_BRIGHTNESS);
}

void DrawBackground(void)
{
	PIXEL32 White = { 0xFF, 0xFF, 0xFF, 0xFF };
	PIXEL32 Black = { 0x00, 0x00, 0x00, 0xFF };

	for (uint16_t y = 0; y < WINDOW_HEIGHT; y++) {		//  middle bar white pixels
		for (uint16_t x = 0; x < 8; x++) {
			memcpy_s((PIXEL32*) BackBuffer.Memory + MEMORY_OFFSET(Background.MiddleBarPosX, Background.MiddleBarPosY, x, y), sizeof(PIXEL32), &White, sizeof(PIXEL32));
		}
	}

	for (uint16_t y = 0; y < WINDOW_HEIGHT; y += 8) {	//  middle bar black pixels
		for (uint16_t x = 0; x < 8; x++) {
			memcpy_s((PIXEL32*) BackBuffer.Memory + MEMORY_OFFSET(Background.MiddleBarPosX, Background.MiddleBarPosY, x, y), sizeof(PIXEL32), &Black, sizeof(PIXEL32));
		}
	}

	for (uint16_t y = 0; y < 4; y++) {					//  top bar white pixels
		for (uint16_t x = 0; x < WINDOW_WIDTH; x++) {
			memcpy_s((PIXEL32*) BackBuffer.Memory + MEMORY_OFFSET(Background.TopBarPosX, Background.TopBarPosY, x, y), sizeof(PIXEL32), &White, sizeof(PIXEL32));
		}
	}

	for (uint16_t y = 0; y < 4; y++) {					//  bottom bar white pixels
		for (uint16_t x = 0; x < WINDOW_WIDTH; x++) {
			memcpy_s((PIXEL32*) BackBuffer.Memory + MEMORY_OFFSET(Background.BottomBarPosX, Background.BottomBarPosY, x, y), sizeof(PIXEL32), &White, sizeof(PIXEL32));
		}
	}
}

void AIMovement(void)
{
	if (Ball.Y < Player2.Y) {
		if (Player2.Y > 0) {
			Player2.Y--;
		}
	}

	if (Ball.Y > Player2.Y) {
		if (Player2.Y < WINDOW_HEIGHT - 64) {
			Player2.Y++;
		}
	}
}

void DrawStartScreen(_In_ HWND hGameWnd)
{
	HDC hdc;
	char StartScreenString[16];
	PIXEL32 White = { 0xFF, 0xFF, 0xFF, 0xFF };
	uint16_t CursorPos = WINDOW_HEIGHT / 2, StartGamePos = WINDOW_HEIGHT / 2, QuitPos = (WINDOW_HEIGHT / 2) + 16;
	static uint16_t DownKeyWasDown = 0, UpKeyWasDown = 0;

	ZeroMemory(&hdc, sizeof(HDC));
	memset(StartScreenString, 0, sizeof(StartScreenString));
	
	while (TRUE) {
		ClearScreen();

		sprintf_s(StartScreenString, _countof(StartScreenString), "PONG");
		BlitBitmapString(StartScreenString, &FontSheetBuffer, &White, WINDOW_WIDTH / 2, 200, DEFAULT_BRIGHTNESS);
		
		sprintf_s(StartScreenString, _countof(StartScreenString), "»");
		BlitBitmapString(StartScreenString, &FontSheetBuffer, &White, (WINDOW_WIDTH / 2) - 16, CursorPos, DEFAULT_BRIGHTNESS);
		
		sprintf_s(StartScreenString, _countof(StartScreenString), "Start Game");
		BlitBitmapString(StartScreenString, &FontSheetBuffer, &White, WINDOW_WIDTH / 2, StartGamePos, DEFAULT_BRIGHTNESS);
		
		sprintf_s(StartScreenString, _countof(StartScreenString), "Quit");
		BlitBitmapString(StartScreenString, &FontSheetBuffer, &White, WINDOW_WIDTH / 2, QuitPos, DEFAULT_BRIGHTNESS);
		
		if (KEY_DOWN(VK_DOWN) && !DownKeyWasDown) {
			CursorPos = QuitPos;
			PlaySFX(&Sfx);
		}
		
		if (KEY_DOWN(VK_UP) && !UpKeyWasDown) {
			CursorPos = StartGamePos;
			PlaySFX(&Sfx);
		}
		
		if (CursorPos == StartGamePos && KEY_DOWN(VK_RETURN)) {
			GameState = MAIN_GAME;
			PlaySFX(&Sfx);
			break;
		}
		
		if (CursorPos == QuitPos && KEY_DOWN(VK_RETURN)) {
			SendMessage(hGameWnd, WM_CLOSE, 0, 0);
			PlaySFX(&Sfx);
			break;
		}
		
		DownKeyWasDown = KEY_DOWN(VK_DOWN);
		UpKeyWasDown = KEY_DOWN(VK_UP);

		hdc = GetDC(hGameWnd);
		StretchDIBits(hdc, 0, 0, GamePerformanceData.MonitorWidth, GamePerformanceData.MonitorHeight, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, BackBuffer.Memory, &BackBuffer.BitMapInfo, DIB_RGB_COLORS, SRCCOPY);
		ReleaseDC(hGameWnd, hdc);
	}
}

void DrawMenuScreen(_In_ HWND hGameWnd)
{
	HDC hdc;
	char MenuString[16];
	PIXEL32 White = { 0xFF, 0xFF, 0xFF, 0xFF };
	uint16_t CursorPos = WINDOW_HEIGHT / 2, ResumePos = WINDOW_HEIGHT / 2, QuitPos = (WINDOW_HEIGHT / 2) + 16;
	static uint16_t DownKeyWasDown = 0, UpKeyWasDown = 0;

	ZeroMemory(&hdc, sizeof(HDC));
	memset(MenuString, 0, sizeof(MenuString));

	while (TRUE) {
		ClearScreen();
		
		sprintf_s(MenuString, _countof(MenuString), "»");
		BlitBitmapString(MenuString, &FontSheetBuffer, &White, (WINDOW_WIDTH / 2) - 16, CursorPos, DEFAULT_BRIGHTNESS);
		
		sprintf_s(MenuString, _countof(MenuString), "Resume");
		BlitBitmapString(MenuString, &FontSheetBuffer, &White, WINDOW_WIDTH / 2, ResumePos, DEFAULT_BRIGHTNESS);
		
		sprintf_s(MenuString, _countof(MenuString), "Quit");
		BlitBitmapString(MenuString, &FontSheetBuffer, &White, WINDOW_WIDTH / 2, QuitPos, DEFAULT_BRIGHTNESS);

		if (KEY_DOWN(VK_DOWN) && !DownKeyWasDown) {
			CursorPos = QuitPos;
			PlaySFX(&Sfx);
		}
		
		if (KEY_DOWN(VK_UP) && !UpKeyWasDown) {
			CursorPos = ResumePos;
			PlaySFX(&Sfx);
		}
		
		if (CursorPos == ResumePos && KEY_DOWN(VK_RETURN)) {
			GameState = MAIN_GAME;
			PlaySFX(&Sfx);
			break;
		}
		
		if (CursorPos == QuitPos && KEY_DOWN(VK_RETURN)) {
			SendMessage(hGameWnd, WM_CLOSE, 0, 0);
			PlaySFX(&Sfx);
			break;
		}
		
		DownKeyWasDown = KEY_DOWN(VK_DOWN);
		UpKeyWasDown = KEY_DOWN(VK_UP);

		hdc = GetDC(hGameWnd);
		StretchDIBits(hdc, 0, 0, GamePerformanceData.MonitorWidth, GamePerformanceData.MonitorHeight, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, BackBuffer.Memory, &BackBuffer.BitMapInfo, DIB_RGB_COLORS, SRCCOPY);
		ReleaseDC(hGameWnd, hdc);
	}
}

void DrawDebug(void)
{
	char DebugString[16];
	PIXEL32 White = { 0xFF, 0xFF, 0xFF, 0xFF };

	memset(DebugString, 0, sizeof(DebugString));

	sprintf_s(DebugString, _countof(DebugString), "FPS: %.01f", GamePerformanceData.AverageFPS);
	BlitBitmapString(DebugString, &FontSheetBuffer, &White, 0, 16, DEFAULT_BRIGHTNESS);
}

void BlitBitmapString(_In_ const char *String, _In_ GAMEBITMAP *BitMap, _In_ PIXEL32 *Color, _In_ uint16_t XPixel, _In_ uint16_t YPixel, _In_ int16_t Brightness)
{
	GAMEBITMAP StringBitmap;
	const char *Characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789`~!@#$%^&*()-=_+\\|[]{};':\",<>./? »«\xf2";
	uint16_t CharWidth = (uint16_t) BitMap->BitMapInfo.bmiHeader.biWidth / FONT_SHEET_CHARACTERS_PER_ROW;
	uint16_t CharHeight = (uint16_t) BitMap->BitMapInfo.bmiHeader.biHeight;
	uint16_t BytesPerChar = (CharWidth * CharHeight * (BitMap->BitMapInfo.bmiHeader.biBitCount / 8));
	uint16_t CharactersLength = (uint16_t) strlen(Characters), StringLength = (uint16_t) strlen(String);
	
	ZeroMemory(&StringBitmap, sizeof(GAMEBITMAP));
	
	StringBitmap.BitMapInfo.bmiHeader.biBitCount = GAME_BPP;
	StringBitmap.BitMapInfo.bmiHeader.biWidth = CharWidth * StringLength;
	StringBitmap.BitMapInfo.bmiHeader.biHeight = CharHeight;
	StringBitmap.BitMapInfo.bmiHeader.biPlanes = 1;
	StringBitmap.BitMapInfo.bmiHeader.biCompression = BI_RGB;
	
	StringBitmap.Memory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, BytesPerChar * StringLength);

	for (uint16_t i = 0; i < StringLength; i++) {
		PIXEL32 FontSheetPixel;
		uint16_t StartingFontSheetByte = 0;

		ZeroMemory(&FontSheetPixel, sizeof(PIXEL32));

		for (uint16_t j = 0; j < CharactersLength; j++) {
			if (String[i] == Characters[j]) {
				StartingFontSheetByte = (uint16_t) STARTING_FONT_SHEET_BYTE(BitMap->BitMapInfo.bmiHeader.biWidth, BitMap->BitMapInfo.bmiHeader.biHeight, CharWidth, j);
				break;
			}
		}
		
		for (uint16_t y = 0; y < CharHeight; y++) {
			for (uint16_t x = 0; x < CharWidth; x++) {
				memcpy_s(&FontSheetPixel, sizeof(PIXEL32), (PIXEL32*) BitMap->Memory + FONT_SHEET_OFFSET(StartingFontSheetByte, BitMap->BitMapInfo.bmiHeader.biWidth, x, y), sizeof(PIXEL32));

				FontSheetPixel.Red = Color->Red;
				FontSheetPixel.Green = Color->Green;
				FontSheetPixel.Blue = Color->Blue;

				memcpy_s((PIXEL32*) StringBitmap.Memory + STRING_BITMAP_OFFSET(i, CharWidth, StringBitmap.BitMapInfo.bmiHeader.biWidth, StringBitmap.BitMapInfo.bmiHeader.biHeight, x, y), sizeof(PIXEL32), &FontSheetPixel, sizeof(PIXEL32));
			}
		}
	}

	BlitBitmapImage(&StringBitmap, XPixel, YPixel, Brightness);

	if (StringBitmap.Memory) {
		HeapFree(GetProcessHeap(), 0, StringBitmap.Memory);
	}
}

void BlitBitmapImage(_In_ GAMEBITMAP *BitMap, _In_ uint16_t XPixel, _In_ uint16_t YPixel, _In_ int16_t Brightness)
{
	PIXEL32 BitmapPixel;
	__m256i BitmapOctoPixel;
	
	ZeroMemory(&BitmapPixel, sizeof(PIXEL32));
	ZeroMemory(&BitmapOctoPixel, sizeof(__m256i));

	for (uint16_t y = 0; y < BitMap->BitMapInfo.bmiHeader.biHeight; y++) {
		uint16_t PixelsRemaining = (uint16_t) BitMap->BitMapInfo.bmiHeader.biWidth;
		uint16_t x = 0;
		
		while (PixelsRemaining >= 8) {
			BitmapOctoPixel = _mm256_load_si256((__m256i*) ((PIXEL32*) BitMap->Memory + BITMAP_MEMORY_OFFSET(BitMap->BitMapInfo.bmiHeader.biWidth, BitMap->BitMapInfo.bmiHeader.biHeight, x, y)));

			__m256i Half1 = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(BitmapOctoPixel, 0));
			Half1 = _mm256_add_epi16(Half1, _mm256_set_epi16(0, Brightness, Brightness, Brightness, 0, Brightness, Brightness, Brightness, 0, Brightness, Brightness, Brightness, 0, Brightness, Brightness, Brightness));
			__m256i Half2 = _mm256_cvtepu8_epi16(_mm256_extracti128_si256(BitmapOctoPixel, 1));
			Half2 = _mm256_add_epi16(Half2, _mm256_set_epi16(0, Brightness, Brightness, Brightness, 0, Brightness, Brightness, Brightness, 0, Brightness, Brightness, Brightness, 0, Brightness, Brightness, Brightness));
			__m256i Recombined = _mm256_packus_epi16(Half1, Half2);
			BitmapOctoPixel = _mm256_permute4x64_epi64(Recombined, _MM_SHUFFLE(3, 1, 2, 0));
			__m256i Mask = _mm256_cmpeq_epi8(BitmapOctoPixel, _mm256_set1_epi8(-1));

			_mm256_maskstore_epi32((int32_t*) ((PIXEL32*) BackBuffer.Memory + MEMORY_OFFSET(XPixel, YPixel, x, y)), Mask, BitmapOctoPixel);
			
			PixelsRemaining -= 8;
			x += 8;
		}
		
		while (PixelsRemaining > 0) {
			memcpy_s(&BitmapPixel, sizeof(PIXEL32), (PIXEL32*) BitMap->Memory + BITMAP_MEMORY_OFFSET(BitMap->BitMapInfo.bmiHeader.biWidth, BitMap->BitMapInfo.bmiHeader.biHeight, x, y), sizeof(PIXEL32));

			if (BitmapPixel.Alpha == 255) {
				
				BitmapPixel.Red = (uint8_t) min(255, max((BitmapPixel.Red + Brightness), 0));
				BitmapPixel.Green = (uint8_t) min(255, max((BitmapPixel.Green + Brightness), 0));
				BitmapPixel.Blue = (uint8_t) min(255, max((BitmapPixel.Blue + Brightness), 0));

				memcpy_s((PIXEL32*) BackBuffer.Memory + MEMORY_OFFSET(XPixel, YPixel, x, y), sizeof(PIXEL32), &BitmapPixel, sizeof(PIXEL32));
			}
				
			PixelsRemaining--;
			x++;
		}
	}
}

void LoadBitmapFromFile(_In_ LPCWSTR Filename, _Inout_ GAMEBITMAP *Bitmap)
{
	HANDLE FileHandle = NULL;
	WORD BitmapHeader = 0;
	DWORD BytesRead = 0, PixelDataOffset = 0;
	BOOL ReadResult = FALSE;

	FileHandle = CreateFile(Filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	ReadResult = ReadFile(FileHandle, &BitmapHeader, 0x2, &BytesRead, NULL);
	SetFilePointer(FileHandle, 0xA, NULL, FILE_BEGIN);  															//  Get bitmap header info
	ReadResult = ReadFile(FileHandle, &PixelDataOffset, sizeof(DWORD), &BytesRead, NULL);
	SetFilePointer(FileHandle, 0xE, NULL, FILE_BEGIN); 										 						//  Get offset of where bitmap pixel data starts
	ReadResult = ReadFile(FileHandle, &Bitmap->BitMapInfo.bmiHeader, sizeof(BITMAPINFOHEADER), &BytesRead, NULL);  	//  Get bitmap header data structure
	Bitmap->Memory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, Bitmap->BitMapInfo.bmiHeader.biSizeImage);
	SetFilePointer(FileHandle, PixelDataOffset, NULL, FILE_BEGIN);  												//  Move to beginning of bitmap pixel data 
	ReadResult = ReadFile(FileHandle, Bitmap->Memory, Bitmap->BitMapInfo.bmiHeader.biSizeImage, &BytesRead, NULL);  //  Read bitmap pixel data into memory

	CloseHandle(FileHandle);
}

void InitSoundEngine(void)
{
	HRESULT hResult = NULL;
	WAVEFORMATEX WaveFormatSFX;

	ZeroMemory(&WaveFormatSFX, sizeof(WAVEFORMATEX));

	WaveFormatSFX.wFormatTag = WAVE_FORMAT_PCM;
	WaveFormatSFX.nChannels = 1;
	WaveFormatSFX.nSamplesPerSec = 44100;
	WaveFormatSFX.nAvgBytesPerSec = WaveFormatSFX.nSamplesPerSec * WaveFormatSFX.nChannels * 2;
	WaveFormatSFX.nBlockAlign = WaveFormatSFX.nChannels * 2;
	WaveFormatSFX.wBitsPerSample = 16;
	WaveFormatSFX.cbSize = 0;

	hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	
	XAudio2Create(&XAudio, 0, XAUDIO2_ANY_PROCESSOR);
	XAudio->CreateMasteringVoice(&XAudioMasteringVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL, AudioCategory_GameEffects);
	
	for (uint8_t i = 0; i < SFX_SOURCE_VOICES; i++) {
		XAudio->CreateSourceVoice(&XAudioSFXSourceVoice[i], &WaveFormatSFX, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);
		XAudioSFXSourceVoice[i]->SetVolume(0.5f, XAUDIO2_COMMIT_NOW);
	}
}

void LoadWavFromFile(_In_ LPCWSTR FileName, _Inout_ GAMESOUND *GameSound)
{
	HANDLE FileHandle = NULL;
	DWORD RIFF = 0, DataChunckSearcher = 0, DataChunckSize = 0, BytesRead = 0;
	BOOL DataChunckFound = FALSE, ReadResult = FALSE;
	uint16_t DataChunckOffset = 0;

	FileHandle = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	ReadResult = ReadFile(FileHandle, &RIFF, sizeof(DWORD), &BytesRead, NULL);
	SetFilePointer(FileHandle, 20, NULL, FILE_BEGIN);
	ReadResult = ReadFile(FileHandle, &GameSound->WaveFormat, sizeof(WAVEFORMATEX), &BytesRead, NULL);
	
	while (!DataChunckFound) {
		SetFilePointer(FileHandle, DataChunckOffset, NULL, FILE_BEGIN);
		ReadResult = ReadFile(FileHandle, &DataChunckSearcher, sizeof(DWORD), &BytesRead, NULL);

		if (DataChunckSearcher == 0x61746164) {  //  'data' backwards
			DataChunckFound = TRUE;
			break;
		}

		DataChunckOffset += 4;

		if (DataChunckOffset > 256) {
			break;
		}
	}

	SetFilePointer(FileHandle, DataChunckOffset + 4, NULL, FILE_BEGIN);
	ReadResult = ReadFile(FileHandle, &DataChunckSize, sizeof(DWORD), &BytesRead, NULL);
	GameSound->Buffer.pAudioData = (BYTE*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, DataChunckSize);

	GameSound->Buffer.Flags = XAUDIO2_END_OF_STREAM;
	GameSound->Buffer.AudioBytes = DataChunckSize;

	SetFilePointer(FileHandle, DataChunckOffset + 8, NULL, FILE_BEGIN);
	ReadResult = ReadFile(FileHandle, (LPVOID) GameSound->Buffer.pAudioData, DataChunckSize, &BytesRead, NULL);

	CloseHandle(FileHandle);
}

void PlaySFX(_In_ GAMESOUND *GameSound)
{
	static uint8_t SourceVoiceSelector = 0;

	XAudioSFXSourceVoice[SourceVoiceSelector]->SubmitSourceBuffer(&GameSound->Buffer, NULL);
	XAudioSFXSourceVoice[SourceVoiceSelector]->Start(0, XAUDIO2_COMMIT_NOW);

	SourceVoiceSelector++;

	if (SourceVoiceSelector > SFX_SOURCE_VOICES - 1) {
		SourceVoiceSelector = 0;
	}
}

LRESULT CALLBACK WndProc(_In_ HWND hWnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	switch (uMsg) {
		case WM_CLOSE : {
			PostQuitMessage(0);
			return 0;
		}

		case WM_ACTIVATE : {
			ShowCursor(FALSE);
			return 0;
		}

		default :
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

//  main function  
int32_t APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PSTR pCmdLine, _In_ int32_t nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(pCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	QueryPerformanceFrequency(&GamePerformanceData.Frequency);
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	
	HWND hGameWnd;
	WNDCLASSEX wc;
	MSG msg;

	ZeroMemory(&hGameWnd, sizeof(HWND));
	ZeroMemory(&wc, sizeof(WNDCLASSEX));
	ZeroMemory(&msg, sizeof(MSG));

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
	wc.lpszMenuName = NULL;
	wc.lpszClassName = WINDOW_NAME;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	BackBuffer.BitMapInfo.bmiHeader.biSize = sizeof(BackBuffer.BitMapInfo.bmiHeader);
	BackBuffer.BitMapInfo.bmiHeader.biWidth = WINDOW_WIDTH;
	BackBuffer.BitMapInfo.bmiHeader.biHeight = WINDOW_HEIGHT;
	BackBuffer.BitMapInfo.bmiHeader.biBitCount = GAME_BPP;
	BackBuffer.BitMapInfo.bmiHeader.biCompression = BI_RGB;
	BackBuffer.BitMapInfo.bmiHeader.biPlanes = 1;

	MonitorInfo.cbSize = sizeof(MONITORINFO);
	
	if (!RegisterClassEx(&wc)) {
		MessageBox(NULL, L"Failed to register window class.", L"Warning", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	
	if (!(hGameWnd = CreateWindowEx(0, WINDOW_NAME, WINDOW_NAME, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, hInstance, NULL))) {
		MessageBox(NULL, L"Failed to create window.", L"Warning", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	if (!(BackBuffer.Memory = VirtualAlloc(NULL, GAME_FRAME_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))) {
		MessageBox(NULL, L"Memory allocation error.", L"Warning", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	if (!GetMonitorInfo(MonitorFromWindow(hGameWnd, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo)) {
		MessageBox(NULL, L"Failed to get monitor info.", L"Warning", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	GamePerformanceData.MonitorWidth = (uint16_t) (MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left);
	GamePerformanceData.MonitorHeight = (uint16_t) (MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top);

	if (!SetWindowLongPtr(hGameWnd, GWL_STYLE, (WS_OVERLAPPEDWINDOW | WS_VISIBLE) & ~WS_OVERLAPPEDWINDOW)) {
		MessageBox(NULL, L"Failed to resize window.", L"Warning", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	if (!SetWindowPos(hGameWnd, HWND_TOP, MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top, GamePerformanceData.MonitorWidth, GamePerformanceData.MonitorHeight, SWP_FRAMECHANGED | SWP_NOOWNERZORDER)) {
		MessageBox(NULL, L"Failed to resize window.", L"Warning", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	Init();
	InitSoundEngine();

	while (TRUE) {
		QueryPerformanceCounter(&GamePerformanceData.BeginFrame);

		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		switch (GameState) {
			case START_SCREEN : {
				DrawStartScreen(hGameWnd);
				break;
			}

			case MENU_SCREEN : {
				DrawMenuScreen(hGameWnd);
				break;
			}

			case MAIN_GAME : {
				PlayerInput(hGameWnd);
				AIMovement();
				CollisionDetection();
				RenderGraphics(hGameWnd);
				break;
			}
		}

		QueryPerformanceCounter(&GamePerformanceData.EndFrame);

		GamePerformanceData.ElapsedMicroseconds.QuadPart = GamePerformanceData.EndFrame.QuadPart - GamePerformanceData.BeginFrame.QuadPart;
		GamePerformanceData.ElapsedMicroseconds.QuadPart *= 1000000;
		GamePerformanceData.ElapsedMicroseconds.QuadPart /= GamePerformanceData.Frequency.QuadPart;
		GamePerformanceData.FramesRendered.QuadPart++;

		while (GamePerformanceData.ElapsedMicroseconds.QuadPart < TARGET_MICROSECONDS) {
			Sleep(1);

			GamePerformanceData.ElapsedMicroseconds.QuadPart = GamePerformanceData.EndFrame.QuadPart - GamePerformanceData.BeginFrame.QuadPart;
			GamePerformanceData.ElapsedMicroseconds.QuadPart *= 1000000;
			GamePerformanceData.ElapsedMicroseconds.QuadPart /= GamePerformanceData.Frequency.QuadPart;
			QueryPerformanceCounter(&GamePerformanceData.EndFrame);
		}

		GamePerformanceData.ElapsedMicrosecondsAcc.QuadPart += GamePerformanceData.ElapsedMicroseconds.QuadPart;

		if (GamePerformanceData.FramesRendered.QuadPart % 120 == 0) {
			GamePerformanceData.AverageFPS = 1.0f / (((float) GamePerformanceData.ElapsedMicrosecondsAcc.QuadPart / 120.0f) * 0.000001f);
			GamePerformanceData.ElapsedMicrosecondsAcc.QuadPart = 0;
		}
	}

	return (int32_t) msg.wParam;
}