#pragma once
#include <vector>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <windows.h>

#define img_path "img.bmp"
#define logs_path "logs.txt"
//#define logs 1

using namespace std;

class WCO
{
    vector<int> keys = {'W', 'A', 'S', 'D', 'E', 32, 16, 17, 27, 49, 50, 51, 52, 53, 54, 55, 56, 57, 48};

    bool logs =0;

    const float max_sens = 4096;
    const float sens = 1;
    const int speed = 100 / 2; // 25
    const float cursor_weight = 1.5;
    int screenX;
    int screenY;

    int im_width, im_height;
    size_t screen_linesize;

    char *bits;

    HDC screenDC;
    HDC memDC;
    HBITMAP memBm;
    HBITMAP oldBm;
    BITMAPINFOHEADER info;

public:
    WCO(int width = 1920, int height = 1080);
    ~WCO();
    int get_width()
    {
        return im_width;
    }
    int get_height()
    {
        return im_height;
    }

    void clean();

    void get_keys(vector<float> &output, POINT &cursor_pos, POINT &old_cursor_pos, int fps=1, const bool track_mouse = true);

    void set_keys(vector<float> &output, POINT &cursor_pos, const bool track_mouse = true);

    void set_cursor_pos(vector<float> &input, POINT &cursor_pos, const int screen_resolution, const int cursor_weight, const int cursor_size=8, const int shift=0);

    inline void add_cursor_to_img(vector<float> &input, POINT &cursor_pos, const int screen_resolution, const int cursor_weight, int x_shift = 0, int y_shift = 0, int shift = 0);

    void LoadFromScreen(const int x, const int y, const int w, const int h);

    void SaveToFile(const char *fileName);

    unsigned GetPixel(int x, int y);
    unsigned getRGB(unsigned val, int i);
    unsigned get_sum_RGB(unsigned val);

    vector<float> screenshot_mono(int screen_resolution = 1, float coef = 1);
    vector<float> screenshot_rgb(int screen_resolution = 1, float coef = 1);

    void SaveVectorRGBToFile(const char *fileName, const std::vector<float> &data, int width, int height);
    void SaveVectorToFile(const char *fileName, const std::vector<float> &data, int width, int height);

    vector<float> cut_img_x(const vector<float> &data, int frame, int width, int height);
};
