#include "windows_capture_object.hpp"

// int w='W', a='A',s='S',d='D', additional_key1='E', additional_key2=27, shift=16, ctrl=17, space=32;// 1 - e  esc=27
// int k1=49, k2=50, k3=51, k4=52, k5=53, k6=54, k7=55, k8=56, k9=57, k0=48;
// int w='E', a='W',s='W',d='W', additional_key='W', shift='W', ctrl='W', space='W';
/// vector<int> keys= {'W','A','S','D', 'E','F', 16,17,32,  49,50,51,52,53,54,55,56,57,48};
///  w a s d  e esc(27) enter(13)  shift(16) ctrl(17) space(32)  1 2 3 4 5 6 7 8 9 0         |      W','A','S','D', 'E',27, 16,17,32,  49,50,51,52,53,54,55,56,57,48

void WCO::clean()
{
    // Sleep(500);
    for (int i = 0; i < keys.size(); i++)
        if (keys[i] != -1)
            keybd_event(keys[i], 0, KEYEVENTF_KEYUP, 0);
    cout << "clear" << endl
         << endl;
    if (logs)
    {
        std::ofstream flogs(logs_path, std::ios::app);
        flogs << std::endl
              << std::endl;
        flogs.close();
    }
}

void WCO::get_keys(vector<float> &output, POINT &cursor_pos, POINT &old_cursor_pos, int fps, const bool track_mouse)
{

    output.resize(25, 0);

    // keyboard
    for (int i = 0; i < keys.size(); i++)
        if (keys[i] != -1)
            output[i] = (GetAsyncKeyState(keys[i]) < 0);

    // mouse
    output[19] = (GetAsyncKeyState(VK_LBUTTON) < 0); // m1
    output[20] = (GetAsyncKeyState(VK_RBUTTON) < 0); // m2

    // mouse pos
    if (track_mouse)
    {
        // output[21]=(abs(cursor_pos.x-old_cursor_pos.x)<=sens)? 0.5: min(max((float)(cursor_pos.x-old_cursor_pos.x)/max_sens, (float)-1), (float)1)/2+0.5;//mx
        // output[22]=(abs(cursor_pos.y-old_cursor_pos.y)<=sens)? 0.5: min(max((float)(cursor_pos.y-old_cursor_pos.y)/max_sens, (float)-1), (float)1)/2+0.5;//my

        output[21] = min(max((float)(cursor_pos.x - old_cursor_pos.x) * fps / max_sens, (float)0), (float)1); // mx
        output[22] = min(max((float)(old_cursor_pos.x - cursor_pos.x) * fps / max_sens, (float)0), (float)1); // mx
        output[23] = min(max((float)(cursor_pos.y - old_cursor_pos.y) * fps / max_sens, (float)0), (float)1); // my
        output[24] = min(max((float)(old_cursor_pos.y - cursor_pos.y) * fps / max_sens, (float)0), (float)1); // my*/

        old_cursor_pos = cursor_pos;
    }

    // output[25] = (GetAsyncKeyState('U') < 0) ? 1 : (GetAsyncKeyState('J') < 0) ? 0 : 0.5; // U - happy, J - sad
}

void WCO::set_keys(vector<float> &output, POINT &cursor_pos, const bool track_mouse)
{

    // keyboard
    for (int i = 0; i < keys.size(); i++)
        if (keys[i] != -1)
            if (output[i] >= 0.5)
                keybd_event(keys[i], 0, 0, 0);
            else
                keybd_event(keys[i], 0, KEYEVENTF_KEYUP, 0);

    //*/
    // mouse
    if (output[19] >= 0.5)
    {
        mouse_event(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTDOWN, cursor_pos.x, cursor_pos.y, 0, 0); //
        mouse_event(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_LEFTUP, cursor_pos.x, cursor_pos.y, 0, 0);   //
    }

    /// else
    /// mouse_event(MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_LEFTUP, cursor_pos.x, cursor_pos.y, 0, 0); //

    if (output[20] >= 0.5)
    {
        mouse_event(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_RIGHTDOWN, cursor_pos.x, cursor_pos.y, 0, 0); //
        mouse_event(MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_RIGHTUP, cursor_pos.x, cursor_pos.y, 0, 0);   //
    }

    /// else
    ///     mouse_event(MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_RIGHTUP, cursor_pos.x, cursor_pos.y, 0, 0); //

    // mouse pos
    if (track_mouse)
    {
        // mouse_event(MOUSEEVENTF_MOVE,speed*(output[21]-0.5), speed*(output[22]-0.5),0,0);
        mouse_event(MOUSEEVENTF_MOVE, speed * (output[21] - output[22]), speed * (output[23] - output[24]), 0, 0);
    }
}

void WCO::add_cursor_to_img(vector<float> &input, POINT &cursor_pos, const int screen_resolution, const int cursor_weight, int x_shift, int y_shift, int shift)
{
    if (cursor_pos.x / screen_resolution + x_shift < im_width / screen_resolution && cursor_pos.x / screen_resolution + x_shift > 0 && cursor_pos.y / screen_resolution + y_shift < im_height / screen_resolution && cursor_pos.y / screen_resolution + y_shift > 0)
        input[cursor_pos.x / screen_resolution + (cursor_pos.y / screen_resolution + y_shift) * im_width / screen_resolution + x_shift + shift] = cursor_weight;
}

void WCO::set_cursor_pos(vector<float> &input, POINT &cursor_pos, const int screen_resolution, const int cursor_weight, const int cursor_size, const int shift)
{
    if (cursor_pos.y >= im_height - cursor_size / 2 || cursor_pos.x >= im_width - cursor_size / 2) //-50
        return;

    for (int i = 0; i <= cursor_size; i++)
        for (int j = 0; j <= cursor_size; j++)
        {
            if ((i == 0) + (j == 0) + (i == cursor_size) + (j == cursor_size) >= 2)
                add_cursor_to_img(input, cursor_pos, screen_resolution, 0, i - cursor_size / 2, j - cursor_size / 2, shift);
            else
                add_cursor_to_img(input, cursor_pos, screen_resolution, cursor_weight, i - cursor_size / 2, j - cursor_size / 2, shift);
        }
}

WCO::WCO(int width, int height)
{
    im_width = width;
    im_height = height;
    screen_linesize = ((im_width * 32 / 8 + 3) & ~3);
    bits = new char[im_height * screen_linesize];

    screenDC = GetDC(0);
    memDC = CreateCompatibleDC(screenDC);
}

WCO::~WCO()
{
    // Освобождаем выделенную память для битов экрана
    if (bits != nullptr)
    {
        delete[] bits;
        bits = nullptr;
    }

    // Освобождаем ресурсы GDI, если они были созданы
    if (memDC != nullptr)
    {
        DeleteDC(memDC);
        memDC = nullptr;
    }

    // Не освобождаем screenDC здесь, так как он получается через GetDC(0)
    // и должен освобождаться через ReleaseDC в том же контексте

    // Убедимся, что все объекты Bitmap удалены
    if (memBm != nullptr)
    {
        DeleteObject(memBm);
        memBm = nullptr;
    }

    // oldBm не нужно удалять отдельно, так как он был выбран в DC
    // и будет удален вместе с удалением memBm
}

void WCO::reset(int width, int height)
{
    // Освобождаем выделенную память для битов экрана
    if (bits != nullptr)
    {
        delete[] bits;
        bits = nullptr;
    }

    // Освобождаем ресурсы GDI, если они были созданы
    if (memDC != nullptr)
    {
        DeleteDC(memDC);
        memDC = nullptr;
    }

    // Не освобождаем screenDC здесь, так как он получается через GetDC(0)
    // и должен освобождаться через ReleaseDC в том же контексте

    // Убедимся, что все объекты Bitmap удалены
    if (memBm != nullptr)
    {
        DeleteObject(memBm);
        memBm = nullptr;
    }

    // oldBm не нужно удалять отдельно, так как он был выбран в DC
    // и будет удален вместе с удалением memBm

    im_width = width;
    im_height = height;
    screen_linesize = ((im_width * 32 / 8 + 3) & ~3);
    bits = new char[im_height * screen_linesize];

    screenDC = GetDC(0);
    memDC = CreateCompatibleDC(screenDC);
}

void WCO::LoadFromScreen(const int x, const int y, const int w, const int h)
{
    screenDC = GetDC(0);
    memDC = CreateCompatibleDC(screenDC);
    memBm = CreateCompatibleBitmap(screenDC, w, h);
    oldBm = (HBITMAP)SelectObject(memDC, memBm);

    BitBlt(memDC, 0, 0, w, h, screenDC, x, y, SRCCOPY);

    memset(&info, 0, sizeof(info));
    info.biSize = sizeof(BITMAPINFOHEADER);
    info.biCompression = BI_RGB;
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biWidth = w;
    info.biHeight = h;
    info.biSizeImage = h * ((w * info.biBitCount / 8 + 3) & ~3);

    im_width = w;
    im_height = h;

    GetDIBits(memDC, memBm, 0, h, bits, (BITMAPINFO *)&info, 0); // int checkMe =

    SelectObject(memDC, oldBm);
    DeleteObject(memBm);
    DeleteDC(memDC);
    ReleaseDC(0, screenDC);
}

void WCO::SaveToFile(const char *fileName)
{
    if (!bits)
    {
        cout << "screen was not saved" << endl;
        return;
    }

    BITMAPINFOHEADER imageHeader;
    memset(&imageHeader, 0, sizeof(imageHeader));
    imageHeader.biSize = sizeof(BITMAPINFOHEADER);
    imageHeader.biCompression = BI_RGB;
    imageHeader.biPlanes = 1;
    imageHeader.biBitCount = 32;
    imageHeader.biWidth = im_width;
    imageHeader.biHeight = im_height;
    imageHeader.biSizeImage = im_height * ((im_width * imageHeader.biBitCount / 8 + 3) & ~3);

    BITMAPFILEHEADER fileHeader;
    memset(&fileHeader, 0, sizeof(fileHeader));
    fileHeader.bfType = 'MB';
    fileHeader.bfOffBits = sizeof(fileHeader) + sizeof(imageHeader);
    fileHeader.bfSize = fileHeader.bfOffBits + imageHeader.biSizeImage;

    FILE *file = fopen(fileName, "wb");
    if (file)
    {
        fwrite(&fileHeader, sizeof(fileHeader), 1, file);
        fwrite(&imageHeader, sizeof(imageHeader), 1, file);
        fwrite(bits, imageHeader.biSizeImage, 1, file);
        fclose(file);
    }
}

unsigned WCO::GetPixel(int x, int y)
{
    if (!bits || (x < 0) || (x >= im_width) || (y < 0) || (y >= im_height))
        return 0;
    return *(unsigned *)(bits + (im_height - y - 1) * screen_linesize + x * 4);
}

unsigned WCO::getRGB(unsigned val, int i)
{
    return (val >> 16 - 8 * i) & 255; //("%02X:%02X:%02X", (val >> 16) & 255);//, (val >> 8) & 255, (val >> 8) & 255);
    //  printf("%02X:%02X:%02X", (val >> 16) & 255, (val >> 8) & 255, (val >> 0) & 255);
}
unsigned WCO::get_sum_RGB(unsigned val)
{
    return ((val >> 16) & 255) * 0.2 + ((val >> 8) & 255) * 0.7 + ((val >> 0) & 255) * 0.1; //("%02X:%02X:%02X", (val >> 16) & 255);//, (val >> 8) & 255, (val >> 8) & 255);
    //  printf("%02X:%02X:%02X", (val >> 16) & 255, (val >> 8) & 255, (val >> 0) & 255);
}

vector<float> WCO::screenshot_mono(int screen_resolution, float coef)
{

    vector<float> res(im_height * im_width / screen_resolution / screen_resolution); //(im_width*im_height/screen_resolution/screen_resolution)
    int k = 0;

    LoadFromScreen(0, 0, im_width, im_height);
    if (logs)
        SaveToFile(img_path);

    unsigned pix;

    for (int y = 0; y < im_height; y += screen_resolution)
    {
        for (int x = 0; x < im_width; x += screen_resolution)
        {
            /// if (y < 1030) ///
            {
                pix = GetPixel(x, y);
                // cout<<pix<<endl;
                res[k] = (float)get_sum_RGB(pix) * coef / 256.0; // ( pow( (((float)image.get_sum_RGB(pix)/256) -0.5*0)*1,1)/1e+0 ); //25    /256.0
                ++k;
            }
            /// else res[k] = 0; ///
        }
    }

    // cout<<res.size();
    return res;
}

vector<float> WCO::screenshot_rgb(int screen_resolution, float coef)
{
    // auto tms=chrono::duration_cast<std::chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
    vector<float> res((im_height * im_width / screen_resolution / screen_resolution) * 3); //(im_width*im_height/screen_resolution/screen_resolution)
    int k = 0;

    LoadFromScreen(0, 0, im_width, im_height);
    if (logs)
        SaveToFile(img_path);
    // cout<<chrono::duration_cast<std::chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count()-tms<<" ms spent"<<endl;
    unsigned pix;

    for (int i = 0; i < 3; i++)
        for (int y = 0; y < im_height; y += screen_resolution)
        {

            for (int x = 0; x < im_width; x += screen_resolution)
            {
                /// if (y < 1030) ///
                {
                    pix = GetPixel(x, y);
                    res[k] = (float)getRGB(pix, i) * coef / 256.0; // /256.0
                    ++k;
                }
                /// else res[k] = 0;///
            }
        }

    return res;
}

// Добавьте в класс WCO (в заголовочный файл):
void SaveVectorToFile(const char *fileName, const std::vector<float> &data, int width, int height);

// Реализация в .cpp файле:
void WCO::SaveVectorToFile(const char *fileName, const std::vector<float> &data, int width, int height)
{
    if (data.empty())
    {
        std::cout << "Data is empty" << std::endl;
        return;
    }

    if (width * height != static_cast<int>(data.size()))
    {
        std::cout << "Data size doesn't match width*height" << std::endl;
        return;
    }

    // Вычисляем размер строки с выравниванием
    size_t linesize = ((width * 32 / 8 + 3) & ~3);
    size_t imageSize = height * linesize;

    // Создаем временный буфер для данных изображения
    std::vector<unsigned char> buffer(imageSize, 0);

    // Заполняем буфер данными из вектора
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // Преобразуем float [0-1] в unsigned char [0-255]
            unsigned char pixelValue = static_cast<unsigned char>(
                (std::min)((std::max)(data[y * width + x] * 255.0f, 0.0f), 255.0f));

            // Вычисляем позицию в буфере (BMP хранится снизу вверх)
            size_t pos = (height - y - 1) * linesize + x * 4;

            // Записываем в формате BGRA (Windows BMP формат)
            buffer[pos] = pixelValue;     // Blue
            buffer[pos + 1] = pixelValue; // Green
            buffer[pos + 2] = pixelValue; // Red
            buffer[pos + 3] = 255;        // Alpha (непрозрачный)
        }
    }

    // Создаем заголовки BMP
    BITMAPINFOHEADER imageHeader;
    memset(&imageHeader, 0, sizeof(imageHeader));
    imageHeader.biSize = sizeof(BITMAPINFOHEADER);
    imageHeader.biCompression = BI_RGB;
    imageHeader.biPlanes = 1;
    imageHeader.biBitCount = 32;
    imageHeader.biWidth = width;
    imageHeader.biHeight = height;
    imageHeader.biSizeImage = static_cast<DWORD>(imageSize);

    BITMAPFILEHEADER fileHeader;
    memset(&fileHeader, 0, sizeof(fileHeader));
    fileHeader.bfType = 'MB';
    fileHeader.bfOffBits = sizeof(fileHeader) + sizeof(imageHeader);
    fileHeader.bfSize = fileHeader.bfOffBits + imageHeader.biSizeImage;

    // Сохраняем в файл
    FILE *file = fopen(fileName, "wb");
    if (file)
    {
        fwrite(&fileHeader, sizeof(fileHeader), 1, file);
        fwrite(&imageHeader, sizeof(imageHeader), 1, file);
        fwrite(buffer.data(), imageHeader.biSizeImage, 1, file);
        fclose(file);
        // std::cout << "Vector saved to " << fileName << std::endl;
    }
    else
    {
        std::cout << "Failed to open file " << fileName << std::endl;
    }
}

// Для RGB данных (3 канала на пиксель)
void WCO::SaveVectorRGBToFile(const char *fileName, const std::vector<float> &data, int width, int height)
{
    if (data.empty())
    {
        std::cout << "Data is empty" << std::endl;
        return;
    }

    if (width * height * 3 != static_cast<int>(data.size()))
    {
        std::cout << "Data size doesn't match width*height*3" << std::endl;
        return;
    }

    size_t linesize = ((width * 32 / 8 + 3) & ~3);
    size_t imageSize = height * linesize;

    std::vector<unsigned char> buffer(imageSize, 0);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            size_t dataIndex = y * width + x;

            // Преобразуем float [0-1] в unsigned char [0-255]
            unsigned char r = static_cast<unsigned char>(
                (std::min)((std::max)(data[dataIndex] * 255.0f, 0.0f), 255.0f));
            unsigned char g = static_cast<unsigned char>(
                (std::min)((std::max)(data[dataIndex + width * height] * 255.0f, 0.0f), 255.0f));
            unsigned char b = static_cast<unsigned char>(
                (std::min)((std::max)(data[dataIndex + width * height * 2] * 255.0f, 0.0f), 255.0f));

            size_t pos = (height - y - 1) * linesize + x * 4;

            // Формат BGRA для Windows BMP
            buffer[pos] = b;       // Blue
            buffer[pos + 1] = g;   // Green
            buffer[pos + 2] = r;   // Red
            buffer[pos + 3] = 255; // Alpha
        }
    }

    BITMAPINFOHEADER imageHeader;
    memset(&imageHeader, 0, sizeof(imageHeader));
    imageHeader.biSize = sizeof(BITMAPINFOHEADER);
    imageHeader.biCompression = BI_RGB;
    imageHeader.biPlanes = 1;
    imageHeader.biBitCount = 32;
    imageHeader.biWidth = width;
    imageHeader.biHeight = height;
    imageHeader.biSizeImage = static_cast<DWORD>(imageSize);

    BITMAPFILEHEADER fileHeader;
    memset(&fileHeader, 0, sizeof(fileHeader));
    fileHeader.bfType = 'MB';
    fileHeader.bfOffBits = sizeof(fileHeader) + sizeof(imageHeader);
    fileHeader.bfSize = fileHeader.bfOffBits + imageHeader.biSizeImage;

    FILE *file = fopen(fileName, "wb");
    if (file)
    {
        fwrite(&fileHeader, sizeof(fileHeader), 1, file);
        fwrite(&imageHeader, sizeof(imageHeader), 1, file);
        fwrite(buffer.data(), imageHeader.biSizeImage, 1, file);
        fclose(file);
    }
}

vector<float> WCO::cut_img_x(const vector<float> &data, int frame, int width, int height)
{
    vector<float> res((width - 2 * frame) * height);
    for (int i = 0; i < height; i++)
        for (int j = frame; j < width - frame; j++)
            res[i * (width - 2 * frame) + (j - frame)] = data[i * width + j];
    return res;
}