#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <stdint.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <windows_capture_object.hpp>

#include <Audio_controller.hpp>

WCO wco;
POINT cursor_pos, old_cursor_pos;
static const int N = 4096, audio_step = static_cast<int>(N * 0.35);
Audio_controller ac(N, audio_step);

std::string dataset_path, config_path = "config.txt";
int screen_resolution, frame_cut_size;
int im_width, im_height;
unsigned int delta_frames_count = 0, fps = 1;
unsigned long long frames_count = 0;

ma_result result;
ma_device_config micro_device_config, system_device_config;
ma_device mic_device, sys_device;

// Мьютексы для безопасного доступа к данным
std::mutex sys_data_mutex, mic_data_mutex;
std::vector<float> mic_pcm_data, sys_pcm_data;

// std::vector<std::vector<float>> mic_spec, sys_spec;

// Атомарные флаги для управления потоками
std::atomic<bool> keep_running{true};

bool logs = false;

// Функция для безопасного добавления данных
void safe_add_system_data(const float *samples, ma_uint32 frameCount)
{
    std::lock_guard<std::mutex> lock(sys_data_mutex);
    sys_pcm_data.insert(sys_pcm_data.end(), samples, samples + frameCount);
}

void safe_add_mic_data(const float *samples, ma_uint32 frameCount)
{
    std::lock_guard<std::mutex> lock(mic_data_mutex);
    mic_pcm_data.insert(mic_pcm_data.end(), samples, samples + frameCount);
}

// Функции для безопасного получения размера данных
size_t safe_get_system_size()
{
    std::lock_guard<std::mutex> lock(sys_data_mutex);
    return sys_pcm_data.size();
}

size_t safe_get_mic_size()
{
    std::lock_guard<std::mutex> lock(mic_data_mutex);
    return mic_pcm_data.size();
}

void take_screenshot(std::vector<float> &win_frame)
{
    win_frame = wco.screenshot_mono(screen_resolution);
    wco.set_cursor_pos(win_frame, cursor_pos, screen_resolution, 1.5, 8 - screen_resolution);
    wco.cut_img_x(win_frame, frame_cut_size, im_width / screen_resolution, im_height / screen_resolution);
}

// Функция для обработки данных (дополнение нулями и удаление)
bool process_audio_chunks()
{
    bool processed = false;
    std::vector<float> sys_spec_frame, mic_spec_frame, win_frame, win_keys;
    int nsize = N + audio_step;
    if (sys_pcm_data.size() < nsize && mic_pcm_data.size() < nsize)
        return 0;
    // Обрабатываем системный звук
    {
        std::lock_guard<std::mutex> lock(sys_data_mutex);
        // Дополняем нулями с начала если нужно
        if (sys_pcm_data.size() < nsize)
        {
            sys_pcm_data.insert(sys_pcm_data.begin(),
                                nsize - sys_pcm_data.size(), 0.0f);
            // std::cout << sys_pcm_data.size() << std::endl;
        }

        // Здесь ваша обработка данных...
        // ac.process(sys_pcm_data.data(), N); // Пример
        sys_spec_frame = ac.amplitudes_to_spectrogram(sys_pcm_data)[0];
        // if(tmp.size()>1)std::cout<<"warinig: audio frames count: "<<tmp.size()<<std::endl;
        // sys_spec.push_back(tmp[0]);

        // Удаляем обработанные данные
        if (sys_pcm_data.size() > audio_step)
        {
            sys_pcm_data.erase(sys_pcm_data.begin(),
                               sys_pcm_data.begin() + audio_step);
            processed = true;
        }
    }

    // Обрабатываем микрофон
    {
        std::lock_guard<std::mutex> lock(mic_data_mutex);

        if (mic_pcm_data.size() < nsize)
        {
            mic_pcm_data.insert(mic_pcm_data.begin(),
                                nsize - mic_pcm_data.size(), 0.0f);
        }

        // Здесь ваша обработка данных...
        // ac.process(mic_pcm_data.data(), N); // Пример
        mic_spec_frame = ac.amplitudes_to_spectrogram(mic_pcm_data)[0];
        // if(tmp.size()>1)std::cout<<"warinig: audio frames count: "<<tmp.size()<<std::endl;
        // mic_spec.push_back(tmp[0]);

        if (mic_pcm_data.size() > audio_step)
        {
            mic_pcm_data.erase(mic_pcm_data.begin(),
                               mic_pcm_data.begin() + audio_step);
            processed = true;
        }
    }

    GetCursorPos(&cursor_pos);
    take_screenshot(win_frame);
    wco.get_keys(win_keys, cursor_pos, old_cursor_pos, fps);

    if (logs)
    {
        wco.SaveVectorToFile("img.bmp", win_frame, im_width / screen_resolution - 2 * frame_cut_size, im_height / screen_resolution);
        std::cout << "fps: " << fps << "; ";
        for (int i = 0; i < win_keys.size(); i++)
            std::cout << win_keys[i] << " ";
        std::cout << endl;
    }

    std::ofstream f(dataset_path, std::ios::binary | std::ios::app);
    // sys_aud, mic_aud, win_frame, win_keys
    f.write(reinterpret_cast<char *>(sys_spec_frame.data()), sys_spec_frame.size() * sizeof(float));
    f.write(reinterpret_cast<char *>(mic_spec_frame.data()), mic_spec_frame.size() * sizeof(float));
    f.write(reinterpret_cast<char *>(win_frame.data()), win_frame.size() * sizeof(float));
    f.write(reinterpret_cast<char *>(win_keys.data()), win_keys.size() * sizeof(float));

    f.close();

    delta_frames_count++;

    return processed;
}

void init_audio_devices()
{
    // Колбэк для системного звука
    auto system_callback = [](ma_device *pDevice, void *pOutput,
                              const void *pInput, ma_uint32 frameCount)
    {
        if (pInput != nullptr)
        {
            const float *pSamples = static_cast<const float *>(pInput);
            safe_add_system_data(pSamples, frameCount);
        }
    };

    // Колбэк для микрофона
    auto mic_callback = [](ma_device *pDevice, void *pOutput,
                           const void *pInput, ma_uint32 frameCount)
    {
        if (pInput != nullptr)
        {
            const float *pSamples = static_cast<const float *>(pInput);
            safe_add_mic_data(pSamples, frameCount);
        }
    };

    // Настройка устройства для системного звука
    system_device_config = ma_device_config_init(ma_device_type_loopback);
    system_device_config.capture.format = ma_format_f32;
    system_device_config.capture.channels = 1;
    system_device_config.sampleRate = 24000;
    system_device_config.dataCallback = system_callback;
    system_device_config.periodSizeInFrames = 512;

    // Настройка устройства для микрофона
    micro_device_config = ma_device_config_init(ma_device_type_capture);
    micro_device_config.capture.format = ma_format_f32;
    micro_device_config.capture.channels = 1;
    micro_device_config.sampleRate = 24000;
    micro_device_config.dataCallback = mic_callback;
    micro_device_config.periodSizeInFrames = 512;
}

// Отдельный поток для обработки аудио
void audio_processing_thread()
{
    const std::chrono::milliseconds processing_interval(10); // 10 мс
    GetCursorPos(&old_cursor_pos);
    while (keep_running)
    {
        auto start_time = std::chrono::steady_clock::now();

        // Обрабатываем данные
        process_audio_chunks();

        // Ждем до следующей итерации
        auto end_time = start_time + processing_interval;
        std::this_thread::sleep_until(end_time);
    }
}

int main()
{

    int key_out_num;
    {
        bool auto_new;
        std::string foo;
        ifstream f(config_path);
        f >> foo >> screen_resolution;
        f >> foo >> im_width;
        f >> foo >> im_height;
        f >> foo >> frame_cut_size;
        f >> foo >> key_out_num;
        f >> foo >> logs;
        f >> foo >> auto_new;
        f >> foo >> dataset_path;
        f.close();

        if (auto_new)
        {
            f.open(config_path);
            std::stringstream buffer;
            buffer << f.rdbuf();
            std::string content = buffer.str();
            f.close();

            int last_index = content.rfind('_');
            std::ofstream of(config_path);

            of << content.substr(0, last_index) + "_" + std::to_string(std::stoi(content.substr(last_index + 1)) + 1);
            of.close();
        }
    }
    {
        std::vector<float> tmp;
        GetCursorPos(&cursor_pos);

        std::cout << "audio_size(N): " << N << "; audio_step: " << audio_step;
        take_screenshot(tmp);
        std::cout << "; win_frame_size: " << tmp.size();
        wco.get_keys(tmp, cursor_pos, cursor_pos);
        std::cout << "; win_keys_count: " << tmp.size() << ";" << std::endl;
    }

    init_audio_devices();

    // Инициализация устройства системного звука
    result = ma_device_init(NULL, &system_device_config, &sys_device);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Error initializing system device: " << result << std::endl;
        return -1;
    }

    // Initialize microphone device
    result = ma_device_init(NULL, &micro_device_config, &mic_device);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Error initializing microphone: " << result << std::endl;
        ma_device_uninit(&sys_device);
        return -1;
    }

    // Start audio capture
    result = ma_device_start(&sys_device);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Error starting system device: " << result << std::endl;
        ma_device_uninit(&sys_device);
        ma_device_uninit(&mic_device);
        return -1;
    }

    result = ma_device_start(&mic_device);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Error starting microphone: " << result << std::endl;
        ma_device_stop(&sys_device);
        ma_device_uninit(&sys_device);
        ma_device_uninit(&mic_device);
        return -1;
    }

    std::cout << "Capture started" << std::endl;

    // Запускаем поток обработки
    std::thread processing_thread(audio_processing_thread);

    // Ждем 5 секунд
    // std::this_thread::sleep_for(std::chrono::seconds(5));
    while (GetAsyncKeyState(key_out_num) == 0)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        frames_count += delta_frames_count;
        fps = delta_frames_count;
        delta_frames_count = 0;
    }

    // Останавливаем обработку
    keep_running = false;
    processing_thread.join();

    // Останавливаем захват и очищаем ресурсы
    ma_device_stop(&sys_device);
    ma_device_stop(&mic_device);
    ma_device_uninit(&sys_device);
    ma_device_uninit(&mic_device);

    std::cout << frames_count << " frames were taken" << std::endl;

    // std::cout << "Захват завершен. Сохранение в WAV..." << std::endl;

    // Сохраняем данные в файлы (нужен мьютекс при чтении)
    /*{
        for (int i = 0; i < sys_spec.size(); i++)
            sys_spec[i] = ac.recover_modules(sys_spec[i]);
        ac.writeWav("system_audio.wav", ac.griffinLimFromSpectrogram(sys_spec, 15));
    }

    {
        for (int i = 0; i < mic_spec.size(); i++)
            mic_spec[i] = ac.recover_modules(mic_spec[i]);
        ac.writeWav("micro_audio.wav", ac.griffinLimFromSpectrogram(mic_spec, 15));
    }

    std::cout << "Готово. Размеры данных: системный - "
              << sys_pcm_data.size() << ", микрофон - "
              << mic_pcm_data.size() << " сэмплов" << std::endl;*/

    return 0;
}