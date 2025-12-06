#define MINIAUDIO_IMPLEMENTATION // Это важно: определяет реализацию библиотеки
#include "miniaudio.h"

#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <stdint.h>
#include <vector>

#include <Audio_controller.hpp>
// Функция для записи заголовка WAV-файла
void writeWavHeader(std::ofstream& file, int sampleRate, int channels, int bitsPerSample, size_t dataSize) {
    // Структура заголовка WAV
    struct WavHeader {
        char riff[4] = { 'R', 'I', 'F', 'F' };
        uint32_t overallSize = 0; // Заполнится позже
        char wave[4] = { 'W', 'A', 'V', 'E' };
        char fmt[4] = { 'f', 'm', 't', ' ' };
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 1; // PCM
        uint16_t numChannels = 0; // Заполнится
        uint32_t sampleRate = 0;  // Заполнится
        uint32_t byteRate = 0;    // Заполнится
        uint16_t blockAlign = 0;  // Заполнится
        uint16_t bitsPerSample = 0; // Заполнится
        char data[4] = { 'd', 'a', 't', 'a' };
        uint32_t dataSize = 0; // Заполнится
    };

    WavHeader header;
    header.numChannels = channels;
    header.sampleRate = sampleRate;
    header.bitsPerSample = bitsPerSample;
    header.byteRate = sampleRate * channels * bitsPerSample / 8;
    header.blockAlign = channels * bitsPerSample / 8;
    header.dataSize = static_cast<uint32_t>(dataSize);
    header.overallSize = header.dataSize + sizeof(WavHeader) - 8;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
}

int main() {
    ma_result result;
    ma_device_config deviceConfig;
    ma_device device;
    std::ofstream wavFile("system_audio.wav", std::ios::binary);
    std::vector<float> pcmData; // Будем использовать 16-битный PCM для простоты

    // 1. Настраиваем устройство захвата в режиме loopback (системный звук)
    deviceConfig = ma_device_config_init(ma_device_type_loopback);
    deviceConfig.capture.format = ma_format_f32 ; // 16-битный звук
    deviceConfig.capture.channels = 1;           // Моно (1 канал)
    deviceConfig.sampleRate = 24000;             // Частота дискретизации
    deviceConfig.dataCallback = [](ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
        // Эта функция вызывается, когда есть новые аудиоданные
        auto* pData = static_cast<std::vector<float>*>(pDevice->pUserData);
        const float* pSamples = static_cast<const float*>(pInput);
        pData->insert(pData->end(), pSamples, pSamples + frameCount); // Сохраняем данные
    };
    deviceConfig.pUserData = &pcmData; // Передаем вектор для сохранения данных

    // 2. Инициализируем устройство
    result = ma_device_init(NULL, &deviceConfig, &device);
    if (result != MA_SUCCESS) {
        std::cerr << "Ошибка инициализации устройства: " << result << std::endl;
        return -1;
    }

    // 3. Начинаем захват аудио
    result = ma_device_start(&device);
    if (result != MA_SUCCESS) {
        std::cerr << "Ошибка запуска устройства: " << result << std::endl;
        ma_device_uninit(&device);
        return -1;
    }

    std::cout << "Захват аудио начат. Запись в течение 5 секунд..." << std::endl;

    // 4. Ждем 5 секунд, пока накапливаются данные
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 5. Останавливаем захват и очищаем ресурсы
    ma_device_stop(&device);
    ma_device_uninit(&device);

    std::cout << "Захват завершен. Сохранение в WAV..." << std::endl;

    // 6. Записываем заголовок WAV
    size_t dataSize = pcmData.size() * sizeof(float);
    writeWavHeader(wavFile, 24000, 1, 32, dataSize);

    // 7. Записываем аудиоданные в файл
    wavFile.write(reinterpret_cast<const char*>(pcmData.data()), dataSize);
    wavFile.close();

    std::cout << "Файл 'system_audio.wav' успешно создан." << std::endl;

    Audio_controller ac;
    ac.writeWav("system_audio.wav",pcmData);

    return 0;
}