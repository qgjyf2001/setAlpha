#include <Windows.h>
#include <iostream>
#include <future>
#include <thread>
#include <cstdlib>
#include <conio.h>
#include <cstring>
#include <map>
const int recSize = 10;
char* blank;
int midx, midy;
double dpi = 1.0;
HANDLE ConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
const auto defaultColor = RGB(255, 192, 203);
void gotoxy(int x, int y) {
	COORD coord;
	coord.X = x;
	coord.Y = y;
	SetConsoleCursorPosition(ConsoleHandle, coord);
	printf("%s", blank);
	SetConsoleCursorPosition(ConsoleHandle, coord);
}
std::vector<std::vector<COLORREF>> getcolor(HDC hDc,int x,int y) {

	HDC     MemDC;
	BYTE* Data;
	HBITMAP   hBmp;
	BITMAPINFO   bi;
	memset(&bi, 0, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(BITMAPINFO);
	RECT rect;
	bi.bmiHeader.biWidth = recSize;
	bi.bmiHeader.biHeight = recSize;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 24;

	MemDC = CreateCompatibleDC(hDc);
	hBmp = CreateDIBSection(MemDC, &bi, DIB_RGB_COLORS, (void**)&Data, NULL, 0);
	SelectObject(MemDC, hBmp);

	std::vector<std::vector<COLORREF>>  result;
	BitBlt(MemDC, 0, 0, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight, hDc, x, y, SRCCOPY);
	for (int i = 0; i < recSize; i++) {
		std::vector<COLORREF> vec;
		for (int j = 0; j < recSize; j++)
			vec.emplace_back(GetPixel(MemDC, i, j));
		result.emplace_back(vec);
	}
	DeleteDC(MemDC);
	DeleteObject(hBmp);
	return result;
}
class menu {
public:
	virtual HWND doMenu(HWND arg) = 0;
};
class selectWindow : public menu {
public:
	static void drawText(std::future<bool>& fut, int* x, int* y) {
		int nx = *x;
		int ny = *y;
		auto pos2longlong=[](int x,int y)->long long {
			return ((1LL * x) << 32) + y;
		};
		std::map<long long, COLORREF> mp;
		bool hasSet=false;
		int t = 3;
		while (fut.wait_for(std::chrono::milliseconds(10)) == std::future_status::timeout)
		{
			auto* wnd = ::GetDesktopWindow();
			auto* hdc = ::GetDC(wnd);
			int nowx=*x, nowy=*y;
			auto result = getcolor(hdc, nowx, nowy);
			std::map<long long, COLORREF> colorMp,tmp;
			hasSet = false;
			if (nx != nowx || ny != nowy) {
				hasSet = true;
				nx = nowx, ny = nowy;
				for (auto &[pos,color]:mp) {
					if (color != tmp[pos])
						::SetPixel(hdc, pos>>32,pos&(0xffffffff),mp[pos]);
					}
				::ReleaseDC(wnd, hdc);
			}
			hdc = ::GetDC(wnd);
			for (int i = 0; i < recSize; i++)
				for (int j = 0; j < recSize; j++) {
					if (result[i][j] != defaultColor) {
						colorMp[pos2longlong(i + nowx,j + nowy)] = tmp[pos2longlong(i + nowx, j + nowy)] = result[i][j];
						::SetPixel(hdc, i + nowx, j + nowy, defaultColor);
					}
				}
			if (!hasSet) {
				for (auto& [pos, color] : mp) {
					colorMp[pos] = color;
				}
			}
			std::swap(colorMp, mp);
			::ReleaseDC(wnd, hdc);
		}
	}
	HWND doMenu(HWND arg) override {
		printf("使用方向键选择需要改变的窗口，按w或d键更改调整倍率，按回车键确认");
		char c;
		auto fut = promise.get_future();
		int x = midx, y = midy;
		std::thread thread(selectWindow::drawText, std::ref(fut), &x, &y);
		int time = 1;
		HWND hwnd;
		gotoxy(0, 1);
		printf("倍率:×%d", time);
		while (true) {
			gotoxy(0, 1);
			printf("倍率:×%d", time);
			POINT Point;
			Point.x = 1.0*x/dpi;
			Point.y = 1.0*y/dpi;
			hwnd = ::WindowFromPoint(Point);
			char name[128];
			::GetWindowTextA(hwnd, name , 128);gotoxy(0, 2);
			std::cout << "窗口名称:"<<name;
			c = _getch();
			if (c == 'w') {
				if (time<128)
					time*=2;
			}
			else if (c == 's') {
				if (time >= 2)
					time /=2;
			}
			if (c == 0x48) {
				if (y >= time) {
					y -= time;
				}
			} else if (c == 0x4b) {
				if (x>=time) {
					x-=time;
				}
			} else if (c == 0x4d) {
				if (x+time<=midx*2) {
					x += time;
				}
			} else if (c == 0x50) {
				if (y + time <= midy * 2) {
					y += time;
				}
			}

			if (c == '\r' || c == '\n')
				break;
		}
		promise.set_value(true);
		thread.join();
		return hwnd;
	}
private:
	std::promise<bool> promise;
};
class changeAlpha : public menu
{
public:
	HWND doMenu(HWND wnd) override{
		int time = 1, alpha = 255;
		gotoxy(0, 0);
		printf("按方向键（上下）改变窗口透明度，按w或d键更改调整倍率，按回车键退出");
		gotoxy(0, 1);
		printf("倍率:×%d", time);
		gotoxy(0, 2);
		printf("透明度:%d", alpha);
		auto setAlpha = [&]() {
			auto now = GetWindowLong(wnd, GWL_EXSTYLE);
			now |= WS_EX_LAYERED;
			SetWindowLong(wnd, GWL_EXSTYLE, now);
			::SetLayeredWindowAttributes(wnd, 0, alpha, LWA_ALPHA);    // 设置半透明*/
		};
		while (true) {
			gotoxy(0, 1);
			printf("倍率:×%d", time);
			gotoxy(0, 2);
			printf("透明度:%d", alpha);
			char c = _getch();
			if (c == 'w') {
				if (time < 128)
					time *= 2;
			}
			else if (c == 's') {
				if (time >= 2)
					time /= -2;
			}
			if (c == 0x48) {
				if (alpha+time<=255) {
					alpha += time;
					setAlpha();
				}
			}
			else if (c == 0x50) {
				if (alpha >= time) {
					alpha -= time;
					setAlpha();
				}
			}

			if (c == '\r' || c == '\n')
				break;
		}
		return NULL;
	}

};
int main(int argc, char** argv)
{
	int screenW = ::GetSystemMetrics(SM_CXSCREEN);
	int screenH = ::GetSystemMetrics(SM_CYSCREEN);
	HWND hwd = ::GetDesktopWindow();
	HDC hdc = ::GetDC(hwd);
	int width = ::GetDeviceCaps(hdc, DESKTOPHORZRES);
	int height = ::GetDeviceCaps(hdc, DESKTOPVERTRES);
	midx = width/2;
	midy = height/2;
	dpi = 1.0*width / screenW;
	CONSOLE_SCREEN_BUFFER_INFO info = {};
	if (::GetConsoleScreenBufferInfo(ConsoleHandle, &info) == FALSE)
		return 0;
	int charPerLine = info.dwSize.X;
	blank = new char[charPerLine+1];
	for (int i = 0; i < charPerLine; i++)
		blank[i] = ' ';
	blank[charPerLine] = 0;
	HWND arg = NULL;
	std::vector<std::shared_ptr<menu>> menus = { std::make_shared<selectWindow>() ,std::make_shared<changeAlpha>() };
	for (auto currentMenu : menus) {
		arg = currentMenu->doMenu(arg);
	}
	delete[] blank;
}
