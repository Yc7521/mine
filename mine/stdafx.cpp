#include "stdafx.h"

using boost::format;
using boost::str;

//------var-------
HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE); // 标准输出句柄
HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);  // 标准输入句柄
HWND hwnd = GetConsoleWindow(); // 当前窗口句柄
vector<vector<int>> bg;  // 地图
vector<vector<int>> _bg; // 0: hide 1: open 2: mark
int is_ini;              // 是否初始化 0: 还没 1: 已初始化
int is_dead;             // 游戏 死亡
int open_unit;           // 已打开块 数量
int mark_mine;           // 已标记块 数量
clock_t start_time;      // 开始时间 (clock())
int time__;              // 游戏进行时间 (clock() - start_time)
int old_x, old_y;        // 上次点击坐标
bool need_continue;      // 是否需要继续游戏

//-----thread-----
shared_ptr<thread> mouse_thread; // 鼠标 监听线程
POINT mouse_point;       // 鼠标坐标
int mouse_click;         // 按下的鼠标 1:左键 2:右键
volatile int dead;       // 线程结束 标记
mutex mouse_mutex;       // 锁
condition_variable cv;
bool mouse_listerner_pause;

//----setting-----
int max_x, max_y;       // 地图大小
int mine;               // 地雷数量
int mine_;              // 10-23 (%)
int _d_x = 1, _d_y = 1; // 文字大小
int bg_c, f_c, s_c;     // 颜色
int mode;               // 1: normal, 2: 80% open

#pragma region define
POINT client2window(POINT& p);
POINT window2client(POINT& p);
void change_level();
void change_mode();
void change_size();
void down__(int& y);
void end();
bool check_win();
void game();
void check_pos(int& x, int& y);
void save_x_y(int& x, int& y);
void game_over();
void clear();
void ini(int _x, int _y);
void left__(int& x);
bool m_point2x_y(POINT p, int& x, int& y);
bool m_point2line(POINT p, int& line);
void mark(int& x, int& y);
void menu();
void mouse_listener();
bool get_pause();
void set_pause(bool b);
void set_mousepos(POINT pos);
POINT get_mousepos();
void set_mouse_click(int c);
int get_mouse_click();
void right__(int& x);
void select(int& x, int& y);
void select_color();
void refresh();
void setting();
void show(int x, int y);
void hide_selected();
void show_selected(int& x, int& y);
void show_time();
void show_all();
void show_bg();
void show_help();
void show_info(int& x, int& y);
void show_version();
void switch_action_key(int& x, int& y);
int switch_key(int& x, int& y);
void up__(int& y);
void game_win();
wchar_t switch_unit(int& a);
void gotoxy(short x, short y);
void gotoxy(COORD a);
COORD getxy();
ostream& operator<<(ostream& os, const wchar_t* wstr);
ostream& operator<<(ostream& os, const wchar_t wstr);
void wait(int hz);
void press2continue();
template <typename a> void csout(a str, WORD color);
template<typename a> void csout(a str, WORD Tcolor, WORD BGcolor);
int random(int a, int b);
#pragma endregion

// 程序起点
void start() {
	SetConsoleTitle(_T("mine"));
	system("mode con lines=40");
	//隐藏控制台光标
	CONSOLE_CURSOR_INFO CursorInfo;
	GetConsoleCursorInfo(handle, &CursorInfo);
	CursorInfo.bVisible = false;
	SetConsoleCursorInfo(handle, &CursorInfo);

	// 9 * 9
	max_x = 9;
	max_y = 9;

	mine_ = 11;
	mode = 1;

	// color
	bg_c = 0;
	f_c = 8; // 7
	s_c = 7; // 3

	// 监听线程 初始化
	dead = 0;
	mouse_thread = make_shared<thread>(mouse_listener);

	need_continue = false;

	// 进入菜单
	menu();

	// 结束
	end();
}

// 主菜单
void menu() {
	int key;
	set_mouse_click(0);
	while (true) {
		refresh();
		cout << (need_continue ? "1 : 继    续" : "1 : 开始游戏") << endl;
		cout << "2 : 选    项" << endl;
		cout << "3 : 帮    助" << endl;
		cout << "0 : 退    出" << endl;

		auto temp_mouse_click = get_mouse_click();
		if (temp_mouse_click != 0) {
			int line = 0;
			auto b = m_point2line(get_mousepos(), line);

			if (temp_mouse_click == 2) {
				//----------- 消除右击菜单
				while ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
				CONSOLE_FONT_INFO font;
				GetCurrentConsoleFont(handle, FALSE, &font);
				POINT point_;
				GetCursorPos(&point_);
				POINT point{ font.dwFontSize.X * 6 ,font.dwFontSize.Y / 2 };
				client2window(point);
				SetCursorPos(point.x, point.y);
				mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, point.x, point.y, 0, 0);
				SetCursorPos(point_.x, point_.y);
				//-----------
				set_mouse_click(0);
				continue;
			}

			if (b) {
				switch (line) {
				case 0:
					refresh();
					game();
					break;
				case 1:
					refresh();
					setting();
					break;
				case 2:
					refresh();
					show_help();
					cout << endl;
					show_version();
					(void)_getch();
					break;
				case 3:
					return;
					break;
				}
			}
			set_mouse_click(0);
		}

		if (_kbhit()) {
			key = _getch();
			switch (key) {
			case '1':
				refresh();
				game();
				break;
			case '2':
				refresh();
				setting();
				break;
			case '3':
				refresh();
				show_help();
				cout << endl;
				show_version();
				(void)_getch();
				break;
			case 27:case 3:
			case '0':
				return;
				break;
			}
		}
		wait(30);
	}
}

#pragma region game
// 开始游戏
void game() {
	set_mouse_click(0);
	bool need_set_clock = false;
	if (need_continue == false) {
		//--ini--
		need_continue = true;
		need_set_clock = true;
		clear();
		time__ = 0;
		open_unit = 0;
		mark_mine = 0;
		is_ini = 0;
		is_dead = 0;
		mine = (int)((double)random(mine_ - 1, mine_ + 1) * max_y * max_x / 100);
		//--end--
	}

	// out
	int x = 0, y = 0;
	show_bg();
	show_info(x, y);
	show_selected(x, y);
	// start and set start time
	if (need_set_clock) start_time = clock();

	while (true) {
		// show time
		show_time();

		// key
		if (_kbhit()) {
			save_x_y(x, y);
			switch (switch_key(x, y)) {
			case 1:
				select(x, y);
				break;
			case 2:
				mark(x, y);
				break;
			case -1:
				return;
			}
			check_pos(x, y);
		}

		// mouse
		auto temp_mouse_click = get_mouse_click();
		if (temp_mouse_click != 0) {
			save_x_y(x, y);
			if (m_point2x_y(get_mousepos(), x, y)) {
				if (temp_mouse_click == 1) {
					select(x, y);
				} else if (temp_mouse_click == 2) {
					mark(x, y);
					//----------- 消除右击菜单
					while ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
					CONSOLE_FONT_INFO font;
					GetCurrentConsoleFont(handle, FALSE, &font);
					POINT point_;
					GetCursorPos(&point_);
					POINT point{ font.dwFontSize.X * 6 ,font.dwFontSize.Y / 2 };
					client2window(point);
					SetCursorPos(point.x, point.y);
					mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, point.x, point.y, 0, 0);
					SetCursorPos(point_.x, point_.y);
					//-----------
				}
			}

			set_mouse_click(0);
		}

		// show info and current position.
		show_info(x, y);
		show_selected(x, y);

		// is dead (set in select())
		if (is_dead == 1) {
			refresh();
			game_over();
			return;
		}
		// check win
		if (check_win()) {
			refresh();
			game_win();
			return;
		}
		Sleep(33);
	}
}

// 由主菜单的退出 退出程序
void end() {
	dead = 1;
	set_pause(false);
	refresh();
	show("see ", 70);
	Sleep(120);
	show("you ", 70);
	Sleep(100);
	show("next time", 50);
	mouse_thread->join();
	//TODO: if you want, write some code to save the data
	Sleep(300);
}

// 判断是否胜利
bool check_win() {
	return mode == 1 && (max_x * max_y - open_unit == mine) || mode == 2 && ((int)(0.8 * max_x * max_y) < open_unit);
}

// 胜利 并重设参数
void game_win() {
	need_continue = false;
	show_all();
	cout << endl;
	format a("时间:  %d");
	cout << str(a % time__);
	cout << endl;
	cout << "you win !!";
	cout << endl;
	cout << "press any key to enter the menu";
	press2continue();
}

// 失败 并重设参数
void game_over() {
	need_continue = false;
	show_all();
	cout << endl;
	cout << "you failed !!";
	cout << endl;
	cout << "Press any key to enter the menu";
	press2continue();
}

// 将bg和_bg重新设置为(max_x * max_y) 并 归零
void clear() {
	bg.resize(max_y);
	for (auto& j : bg) {
		j.resize(max_x);
	}
	for (auto& j : bg) {
		fill(j.begin(), j.end(), 0);
	}
	_bg.resize(max_y);
	for (auto& j : _bg) {
		j.resize(max_x);
	}
	for (auto& j : _bg) {
		fill(j.begin(), j.end(), 0);
	}
}

// 初始化 (初次点击坐标以及周围8格 无雷)
void ini(int _x, int _y) {
	int x, y;
	// true: (x,y) in circle(point:(_x,_y), radius: radius)
	static auto circle = [&](int radius) {return x >= _x - radius && x <= _x + radius || y >= _y - radius && y <= _y + radius; };
	for (int i = 0; i < mine; ++i) {
		x = random(0, max_x - 1);
		y = random(0, max_y - 1);
		// 随机在点击区域内或重复 则 再次随机坐标
		if (circle(1) || bg[y][x] == -1) {
			--i;
			continue;
		}
		bg[y][x] = -1;
		// 四周无雷块 增加数值
		for (int j = y - 1; j <= y + 1; ++j) {
			if (j >= 0 && j < max_y) {
				for (int k = x - 1; k <= x + 1; ++k) {
					if (k >= 0 && k < max_x) {
						if (bg[j][k] != -1) {
							++(bg[j][k]);
						}
					}
				}
			}
		}
	}
}

// 点击当前块
void select(int& x, int& y) {
	if (_bg[y][x] == 0) {
		// 首次选择 并初始化
		if (is_ini == 0) {
			ini(x, y);
			is_ini = 1;
		}
		if (bg[y][x] == -1) {
			is_dead = 1;
			return;
		}
		show(x, y);
	}
}

// 标记当前块
void mark(int& x, int& y) {
	if (_bg[y][x] == 0) {
		_bg[y][x] = 2;
		gotoxy((x + _d_x) * 2, y + _d_y);
		cout << L"◆";
		++mark_mine;
	} else if (_bg[y][x] == 2) {
		_bg[y][x] = 0;
		gotoxy((x + _d_x) * 2, y + _d_y);
		cout << L"■";
		--mark_mine;
	}
}

// 坐标边界检查
void check_pos(int& x, int& y) {
	if (y > max_y - 1) {
		--y;
		--old_y;
	}
	if (x < 0) {
		++x;
		++old_x;
	}
	if (y < 0) {
		++y;
		++old_y;
	}
	if (x > max_x - 1) {
		--x;
		--old_x;
	}
}

// 保存至old
void save_x_y(int& x, int& y) {
	old_x = x;
	old_y = y;
}

#pragma endregion

#pragma region show
// 显示地图 (面板)
void show_bg() {
	int index = 0;
	cout << "  ";
	for (auto& j : *bg.begin()) {
		format a("%2d");
		cout << str(a % ++index);
	}
	cout << endl;
	index = 0;
	for (size_t i = 0, isize = _bg.size(); i < isize; i++) {
		format a("%2d");
		cout << str(a % ++index);
		for (size_t j = 0, jsize = _bg[i].size(); j < jsize; j++) {
			auto&& t = _bg[i][j];
			if (t == 0) {
				cout << L"■";
			} else if (t == 1) {
				cout << switch_unit(bg[i][j]);
			} else if (t == 2) {
				cout << L"◆";
			}
		}
		cout << endl;
	}
}

// 显示地图 (底板) (相当于全开)
void show_all() {
	int index = 0;
	cout << "  ";
	for (auto& j : bg) {
		format a("%2d");
		cout << str(a % ++index);
	}
	cout << endl;
	index = 0;
	for (auto& j : bg) {
		format a("%2d");
		cout << str(a % ++index);
		for (auto& i : j) {
			cout << switch_unit(i);
		}
		cout << endl;
	}
}

// 检查坐标 x,y 是否存在于当前地图
bool check_xy(int x, int y) {
	return x >= 0 && x < max_x && y >= 0 && y < max_y;
}

// 点击该块后的显示 (从当前位置向四周解开蒙版) (递归)
void show(int x, int y) {
	if (check_xy(x, y)) {
		if (_bg[y][x] == 0) {
			if (bg[y][x] == 0) {
				gotoxy((x + _d_x) * 2, y + _d_y);
				cout << "  ";
				_bg[y][x] = 1;
				++open_unit;
				show(x - 1, y - 1);
				show(x + 0, y - 1);
				show(x + 1, y - 1);

				show(x - 1, y + 0);
				// 本身
				show(x + 1, y + 0);

				show(x + 1, y + 1);
				show(x + 0, y + 1);
				show(x - 1, y + 1);
			} else {
				gotoxy((x + _d_x) * 2, y + _d_y);
				_bg[y][x] = 1;
				++open_unit;
				cout << switch_unit(bg[y][x]);
			}
		}
	}
}

// 上次选中区域 使用前景色覆盖
void hide_selected() {
	gotoxy((old_x + _d_x) * 2, old_y + _d_y);
	auto&& t = _bg[old_y][old_x];
	if (t == 0) {
		csout(L"■", f_c);
	} else if (t == 1) {
		csout(switch_unit(bg[old_y][old_x]), f_c);
	} else if (t == 2) {
		csout(L"◆", f_c);
	}
}

// 选中区域 使用选中色覆盖
void show_selected(int& x, int& y) {
	hide_selected();
	gotoxy((x + _d_x) * 2, y + _d_y);
	auto&& t = _bg[y][x];
	if (t == 0) {
		csout(L"■", s_c);
	} else if (t == 1) {
		csout(switch_unit(bg[y][x]), s_c);
	} else if (t == 2) {
		csout(L"◆", s_c);
	}
}

// 显示时间 (位于地图下方)
void show_time() {
	gotoxy(0, max_y + 2);
	auto temp = (clock() - start_time);
	if (time__ != temp / 1000) {
		time__ = temp / 1000;
		format a("时间:  %d");
		cout << str(a % time__);
	}
}

// 显示信息 (位于时间下方)
void show_info(int& x, int& y) {
	gotoxy(0, max_y + 3);
	format a("当前坐标: (%d,%d)\r\n剩余方格: %-5d剩余地雷: %-5d\r\n");
	cout << str(a % (1 + x) % (1 + y) % (max_x * max_y - open_unit) % (mine - mark_mine)) << endl;
}

// 返回块数据对应的字符
wchar_t switch_unit(int& a) {
	switch (a) {
	case 0:
		return L'　';
	case 1:
		return L'①';
	case 2:
		return L'②';
	case 3:
		return L'③';
	case 4:
		return L'④';
	case 5:
		return L'⑤';
	case 6:
		return L'⑥';
	case 7:
		return L'⑦';
	case 8:
		return L'⑧';
	case -1:
		return L'●';
	default:
		return NULL;
	}
}

#pragma endregion

#pragma region key
// 响应键盘操作
int switch_key(int& x, int& y) {
	int key;
	key = _getch();
	if (key == 224 || key == 0) {
		switch_action_key(x, y);
		(void)_getch();
	} else {
		switch (key) {
		case 27:case 3:
			return -1;
		case '1':
			//mark(x, y);
			return 2;
		case 10:case 13:case ' ':
			//select(x, y);
			return 1;
		case 'w':case 'W':
			up__(y);
			break;
		case 'a':case 'A':
			left__(x);
			break;
		case 's':case 'S':
			down__(y);
			break;
		case 'd':case 'D':
			right__(x);
			break;
		}
	}
	return 0;
}

// 响应特殊按键操作
void switch_action_key(int& x, int& y) {
	if ((GetAsyncKeyState(VK_UP) & 1) != 0) {
		up__(y);
	} else if ((GetAsyncKeyState(VK_LEFT) & 1) != 0) {
		left__(x);
	} else if ((GetAsyncKeyState(VK_DOWN) & 1) != 0) {
		down__(y);
	} else if ((GetAsyncKeyState(VK_RIGHT) & 1) != 0) {
		right__(x);
	} else if ((GetAsyncKeyState(VK_F12) & 1) != 0) {
		//TODO: for now I don't want to put some code here
	}
}

void up__(int& y) {
	y--;
}

void left__(int& x) {
	--x;
}

void down__(int& y) {
	y++;
}

void right__(int& x) {
	x++;
}
#pragma endregion

#pragma region mouse
// 鼠标监听
void mouse_listener() {
	int is_down = 0;
	POINT p;
	auto set = [&p] {
		GetCursorPos(&p);
		window2client(p);
		set_mousepos(p);
	};
	while (true) {
		if (get_pause()) {
			unique_lock<mutex> l(mouse_mutex);
			cv.wait(l);
		}
		if (GetForegroundWindow() == hwnd) {
			if ((GetKeyState(VK_LBUTTON) & 0x8000) != 0) {
				if (is_down == 0) {
					is_down = 1;
					set_mouse_click(1);
					set();
				}
			} else if ((GetKeyState(VK_RBUTTON) & 0x8000) != 0) {
				if (is_down == 0) {
					is_down = 1;
					set_mouse_click(2);
					set();
				}
			} else {
				is_down = 0;
				//set_mouse_click(0);
			}
		}
		if (dead == 1) {
			return;
		}
		Sleep(33);
	}
}

bool get_pause() {
	unique_lock<mutex> l(mouse_mutex);
	return mouse_listerner_pause;
}

void set_pause(bool b) {
	unique_lock<mutex> l(mouse_mutex);
	mouse_listerner_pause = b;
	if (b == false) {
		cv.notify_all();
	}
}

// 鼠标坐标
void set_mousepos(POINT pos) {
	unique_lock<mutex> l(mouse_mutex);
	mouse_point = pos;
}

// 鼠标坐标
POINT get_mousepos() {
	unique_lock<mutex> l(mouse_mutex);
	return mouse_point;
}

// 按下的鼠标 1:左键 2:右键
void set_mouse_click(int c) {
	unique_lock<mutex> l(mouse_mutex);
	mouse_click = c;
}

// 按下的鼠标 1:左键 2:右键
int get_mouse_click() {
	unique_lock<mutex> l(mouse_mutex);
	return mouse_click;
}

// 窗口坐标 -> 全局坐标
POINT client2window(POINT& p) {
	RECT rect;
	GetWindowRect(hwnd, &rect);
	p.x += rect.left;
	p.y += rect.top;
	return p;
}

// 全局坐标 -> 窗口坐标
POINT window2client(POINT& p) {
	RECT rect;
	GetWindowRect(hwnd, &rect);
	p.x -= rect.left;
	p.y -= rect.top;
	return p;
}

// return: true: 已点击
bool m_point2x_y(POINT p, int& x, int& y) {
	int xx = p.x - 8;
	int yy = p.y - 31;
	CONSOLE_FONT_INFO font;
	GetCurrentConsoleFont(handle, FALSE, &font);
	int dx = font.dwFontSize.X * 2;
	int dy = font.dwFontSize.Y;

	int _x = xx / dx - 1;
	int _y = yy / dy - 1;
	if (_x < 0) {
		return false;
	}
	if (_x > max_x - 1) {
		return false;
	}
	if (_y < 0) {
		return false;
	}
	if (_y > max_y - 1) {
		return false;
	}
	x = _x;
	y = _y;
	return true;
}

// return: true: 已点击
bool m_point2line(POINT p, int& line) {
	CONSOLE_FONT_INFO font;
	GetCurrentConsoleFont(handle, FALSE, &font);
	int dx = font.dwFontSize.X * 2;
	int dy = font.dwFontSize.Y;
	int xx = p.x;
	int yy = p.y - dy;

	int _x = xx / dx - 1;
	int _y = yy / dy - 1;
	if (_x < 0) {
		return false;
	}
	if (_y < 0) {
		return false;
	}
	line = _y;
	return true;
}
#pragma endregion

#pragma region setting
void setting() {
	while (true) {
		refresh();
		need_continue = false;
		cout << "1 : 难  度 (";
		cout << ((mine_ < 17) ?
			(mine_ < 14 ? "Very easy" : "Easy") :
			(mine_ < 20 ? "Normal" : (mine_ < 22 ? "Hard" : "Very hard")));
		cout << ")\n";
		cout << "2 : 大  小 (";
		cout << ((max_x < 30) ?
			(max_x < 16 ? "9 * 9" : "16 * 16") :
			(max_x < 42 ? (max_y < 24 ? "30 * 16" : "30 * 24") : "42 * 24"));
		cout << ")\n";
		cout << "3 : 模  式 (";
		cout << ((mode < 2) ?
			("Normal: open all blocks") :
			("Easy: open 80% blocks"));
		cout << ")\n";
		cout << "4 : 颜  色" << endl;
		cout << (get_pause() ? "5 : 鼠  标 (关)" : "5 : 鼠  标 (开)") << endl;
		cout << "0 : 返  回" << endl;

		switch (_getch()) {
		case '1':
			refresh();
			change_level();
			break;
		case '2':
			refresh();
			change_size();
			break;
		case '3':
			refresh();
			change_mode();
			break;
		case '4':
			refresh();
			select_color();
			break;
		case '5':
			set_pause(!get_pause());
			break;
		case 27:case 3:
		case '0':
			return;
			break;
		}
	}
}

void change_level() {
	while (true) {
		cout << "1 : Very easy" << endl;
		cout << "2 : Easy" << endl;
		cout << "3 : Normal" << endl;
		cout << "4 : Hard" << endl;
		cout << "5 : Very hard" << endl;
		switch (_getch()) {
		case '1':
			mine_ = 11;
			return;
		case '2':
			mine_ = 14;
			return;
		case '3':
			mine_ = 17;
			return;
		case '4':
			mine_ = 20;
			return;
		case '5':
			mine_ = 22;
			return;
		default:
			break;
		}
	}
}

void change_size() {
	while (true) {
		cout << "1 :  9 *  9" << endl;
		cout << "2 : 16 * 16" << endl;
		cout << "3 : 30 * 16" << endl;
		cout << "4 : 30 * 24" << endl;
		cout << "5 : 42 * 24" << endl;
		switch (_getch()) {
		case '1':
			max_x = 9;
			max_y = 9;
			return;
		case '2':
			max_x = 16;
			max_y = 16;
			return;
		case '3':
			max_x = 30;
			max_y = 16;
			return;
		case '4':
			max_x = 30;
			max_y = 24;
			return;
		case '5':
			max_x = 42;
			max_y = 24;
			return;
		default:
			break;
		}
	}
}

void change_mode() {
	while (true) {
		cout << "1 : Easy: open 80% blocks" << endl;
		cout << "2 : Normal: open all blocks" << endl;
		switch (_getch()) {
		case '1':
			mode = 2;
			return;
		case '2':
			mode = 1;
			return;
		default:
			break;
		}
	}
}

void select_color() {
	csout("背  景  颜  色", s_c);
	cout << endl << endl;
	cout << "文  字  颜  色" << endl << endl;
	cout << "被 选 择 颜 色" << endl << endl;
	int temp = 0, old_t = 0;
	int c[3] = { bg_c ,f_c ,s_c };
	int t_c = c[temp];
	const int show_cols = 19;
	gotoxy(show_cols, 0);
	csout(L"■■", c[0]);
	gotoxy(show_cols, 2);
	csout(L"■■", c[1]);
	gotoxy(show_cols, 4);
	csout(L"■■", c[2]);
	while (true) {
		gotoxy(0, 6);
		auto old_temp = temp;
		auto temp_key = switch_key(t_c, temp);
		if (temp_key == -1) {
			break;
		} else if (temp_key == 1) {
			bg_c = c[0];
			f_c = c[1];
			s_c = c[2];
			break;
		}
		if (t_c < 0)t_c = 0xf;
		if (t_c > 0xf)t_c = 0;
		if (temp < 0)temp = 2;
		if (temp > 2)temp = 0;
		if (old_t != temp) {
			old_t = temp;
			t_c = c[temp];
		} else {
			c[temp] = t_c;
		}

		gotoxy(0, old_temp * 2);
		switch (old_temp) {
		case 0:
			cout << "背  景  颜  色";
			break;
		case 1:
			cout << "文  字  颜  色";
			break;
		case 2:
			cout << "被 选 择 颜 色";
			break;
		}
		gotoxy(0, temp * 2);
		switch (temp) {
		case 0:
			csout("背  景  颜  色", s_c);
			break;
		case 1:
			csout("文  字  颜  色", s_c);
			break;
		case 2:
			csout("被 选 择 颜 色", s_c);
			break;
		}
		gotoxy(show_cols, temp * 2);
		csout(L"■■", c[temp]);
	}
}

void refresh() {
	format f("color %x%x");
	system(str(f % bg_c % f_c).c_str());
	system("cls");
}

#pragma endregion

void show_help() {
	cout <<
		R"(no help
)" << endl;
}

// @Author : Xie YC
// @Version : 1.0
// @Date : 2018/4/30
void show_version() {
	//R"(Author : Xie YC
	//Version : 1.0
	//Date : 2018/4/30)"
	cout << R"(
1.41:
  new:
    The main menu supports the mouse.
  modify:
    Change the default color(0/7/3 => 0/8/7).
    
1.4:
  new:
    Support for turning off the mouse listener.
    Add 2 map sizes(30 * 24, 42 * 24).
    Can save game when quit.
  fix:
    force use custom colors(in load time) 

Author : Xie YC
Version : 1.4
Date : 2019/8/11)";
	cout << endl;
}

#pragma region tool
void show(string str, DWORD delay) {
	for (auto i : str) {
		cout << i;
		Sleep(delay);
	}
}

void gotoxy(short x, short y) {
	COORD coord = { x, y };
	SetConsoleCursorPosition(handle, coord);
}

void gotoxy(COORD a) {
	SetConsoleCursorPosition(handle, a);
}

//获取当前坐标
COORD getxy() {
	CONSOLE_SCREEN_BUFFER_INFO gb;
	GetConsoleScreenBufferInfo(handle, &gb);
	return gb.dwCursorPosition;
}

//随机取值
int random(int a, int b) {
	uniform_int_distribution<int> s(a, b);
	static default_random_engine e((UINT)time(NULL));
	return s(e);
}

ostream& operator << (ostream& os, const wchar_t* wstr) {
	WriteConsoleW(handle, wstr, (DWORD)wcslen(wstr), NULL, NULL);
	return os;
}

ostream& operator << (ostream& os, const wchar_t wstr) {
	WriteConsoleW(handle, &wstr, 1, NULL, NULL);
	return os;
}

// hz: sleep(1000/hz)
void wait(int hz) {
	Sleep(1000 / hz);
}

void press2continue() {
	// clear
	while (_kbhit()) {
		(void)_getch();
	}
	set_mouse_click(0);

	while (true) {
		// mouse
		auto temp_mouse_click = get_mouse_click();
		if (temp_mouse_click != 0) {
			if (temp_mouse_click == 1) {
				int line;
				if (m_point2line(get_mousepos(), line) && line < 40) {
					set_mouse_click(0);
					return;
				}
			} else if (temp_mouse_click == 2) {
				//----------- 消除右击菜单
				while ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
				CONSOLE_FONT_INFO font;
				GetCurrentConsoleFont(handle, FALSE, &font);
				POINT point_;
				GetCursorPos(&point_);
				POINT point{ font.dwFontSize.X * 6 ,font.dwFontSize.Y / 2 };
				client2window(point);
				SetCursorPos(point.x, point.y);
				mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, point.x, point.y, 0, 0);
				SetCursorPos(point_.x, point_.y);
				//-----------
			}
			set_mouse_click(0);
		}
		// key
		if (_kbhit()) {
			while (_kbhit()) {
				(void)_getch();
			}
			return;
		}
		wait(30);
	}
}

// 0:黑
// 1:蓝
// 2:绿
// 3:青
// 4:红
// 5:紫
// 6:黄
// 7:白
// +8:高亮
template <typename a>
void csout(a str, WORD color) {
	WORD colorOld;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(handle, &csbi);
	colorOld = csbi.wAttributes;
	SetConsoleTextAttribute(handle, 0x00f0 & colorOld | color);
	cout << str;
	SetConsoleTextAttribute(handle, colorOld);
}

template <typename a>
void csout(a str, WORD Tcolor, WORD BGcolor) {
	WORD colorOld;
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(handle, &csbi);
	colorOld = csbi.wAttributes;
	Tcolor |= BGcolor << 4;
	SetConsoleTextAttribute(handle, Tcolor);
	cout << str;
	SetConsoleTextAttribute(handle, colorOld);
}

#pragma endregion
