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

ma_result result;
ma_device_config micro_device_config, system_device_config;
ma_device mic_device, sys_device;

// Мьютексы для безопасного доступа к данным
std::mutex sys_data_mutex, mic_data_mutex;
std::vector<float> mic_pcm_data, sys_pcm_data;
std::vector<std::vector<float>> mic_spec, sys_spec;

// Атомарные флаги для управления потоками
std::atomic<bool> keep_running{true};

static const int N = 4096, audio_step = static_cast<int>(N * 0.35);
Audio_controller ac(N, audio_step);

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

// Функция для обработки данных (дополнение нулями и удаление)
bool process_audio_chunks()
{
    bool processed = false;
    std::vector<std::vector<float>> tmp;
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
            std::cout << sys_pcm_data.size() << std::endl;
        }

        // Здесь ваша обработка данных...
        // ac.process(sys_pcm_data.data(), N); // Пример
        tmp = ac.amplitudes_to_spectrogram(sys_pcm_data);
        sys_spec.insert(sys_spec.end(), tmp.begin(), tmp.end());

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
        tmp = ac.amplitudes_to_spectrogram(mic_pcm_data);
        // std::cout<<tmp.size()<<std::endl;
        mic_spec.insert(mic_spec.end(), tmp.begin(), tmp.end());

        if (mic_pcm_data.size() > audio_step)
        {
            mic_pcm_data.erase(mic_pcm_data.begin(),
                               mic_pcm_data.begin() + audio_step);
            processed = true;
        }
    }

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
    POINT cursor_pos, old_cursor_pos;
    int re = 2;
    auto nya = wco.screenshot_mono(re);
    GetCursorPos(&old_cursor_pos);
    wco.set_cursor_pos(nya, cursor_pos, re, 2);
    wco.SaveVectorToFile("img1.bmp", wco.cut_img_x(nya, 40, wco.get_width() / re, wco.get_height() / re), wco.get_width() / re - 40 * 2, wco.get_height() / re);
    std::vector<float> data;
    while (true)
    {
        GetCursorPos(&cursor_pos);
        wco.get_keys(data, cursor_pos, old_cursor_pos);
        if(data[21]+data[22]+data[23]+data[24]>0)
        {
for (auto &key : data)
            std::cout<<fixed<<setprecision(1) << key << " ";
        std::cout<< endl;
        }
        
        old_cursor_pos = cursor_pos;
    }
        
    return 0;
    init_audio_devices();

    // Инициализация устройства системного звука
    result = ma_device_init(NULL, &system_device_config, &sys_device);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Ошибка инициализации системного устройства: " << result << std::endl;
        return -1;
    }

    // Инициализация устройства микрофона
    result = ma_device_init(NULL, &micro_device_config, &mic_device);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Ошибка инициализации микрофона: " << result << std::endl;
        ma_device_uninit(&sys_device);
        return -1;
    }

    // Запуск захвата аудио
    result = ma_device_start(&sys_device);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Ошибка запуска системного устройства: " << result << std::endl;
        ma_device_uninit(&sys_device);
        ma_device_uninit(&mic_device);
        return -1;
    }

    result = ma_device_start(&mic_device);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Ошибка запуска микрофона: " << result << std::endl;
        ma_device_stop(&sys_device);
        ma_device_uninit(&sys_device);
        ma_device_uninit(&mic_device);
        return -1;
    }

    std::cout << "Захват аудио начат. Обработка в течение 5 секунд..." << std::endl;

    // Запускаем поток обработки
    std::thread processing_thread(audio_processing_thread);

    // Ждем 5 секунд
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Останавливаем обработку
    keep_running = false;
    processing_thread.join();

    // Останавливаем захват и очищаем ресурсы
    ma_device_stop(&sys_device);
    ma_device_stop(&mic_device);
    ma_device_uninit(&sys_device);
    ma_device_uninit(&mic_device);

    std::cout << "Захват завершен. Сохранение в WAV..." << std::endl;

    // Сохраняем данные в файлы (нужен мьютекс при чтении)
    {
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
              << mic_pcm_data.size() << " сэмплов" << std::endl;

    return 0;
}