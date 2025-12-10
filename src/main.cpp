#define MINIAUDIO_IMPLEMENTATION // Это важно: определяет реализацию библиотеки
#include "miniaudio.h"

#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <stdint.h>
#include <vector>

#include <Audio_controller.hpp>
ma_result result;
ma_device_config micro_device_config, system_device_config;
ma_device mic_device, sys_device;
std::ofstream wavFile("system_audio.wav", std::ios::binary);
std::vector<float> mic_pcm_data, sys_pcm_data;
static const int N=4096, audio_step=(int)N*0.35;
Audio_controller ac(N, audio_step);

void init_audio_devices()
{
    auto callback_lambda=[](ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
    {
        // Эта функция вызывается, когда есть новые аудиоданные
        auto *pData = static_cast<std::vector<float> *>(pDevice->pUserData);
        const float *pSamples = static_cast<const float *>(pInput);
        //std::cout<<frameCount<<" "<<pData->size()<<std::endl;
        pData->insert(pData->end(), pSamples, pSamples + frameCount); // Сохраняем данные

        /*if(pData->size()>=N)
        {
            
            pData->erase(pData->begin(),pData->begin()+audio_step);
        }*/
    }; 
    auto callback_lambda2=[](ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
    {
        // Эта функция вызывается, когда есть новые аудиоданные
        auto *pData = static_cast<std::vector<float> *>(pDevice->pUserData);
        const float *pSamples = static_cast<const float *>(pInput);
        //std::cout<<frameCount<<" "<<pData->size()<<std::endl;
        pData->insert(pData->end(), pSamples, pSamples + frameCount); // Сохраняем данные

        /*if(pData->size()>=N)
        {
            
            pData->erase(pData->begin(),pData->begin()+audio_step);
        }*/
    };

    // 1. Настраиваем устройство захвата в режиме loopback (системный звук)
    system_device_config = ma_device_config_init(ma_device_type_loopback);
    system_device_config.capture.format = ma_format_f32; // 16-битный звук
    system_device_config.capture.channels = 1;           // Моно (1 канал)
    system_device_config.sampleRate = 24000;             // Частота дискретизации
    system_device_config.dataCallback = callback_lambda;
    system_device_config.pUserData = &sys_pcm_data; // Передаем вектор для сохранения данных
    system_device_config.periodSizeInFrames = 512;
    


    // 1. Настраиваем устройство захвата в режиме loopback (системный звук)
    micro_device_config = ma_device_config_init(ma_device_type_capture);
    micro_device_config.capture.format = ma_format_f32 ; // 16-битный звук
    micro_device_config.capture.channels = 1;           // Моно (1 канал)
    micro_device_config.sampleRate = 24000;             // Частота дискретизации
    micro_device_config.dataCallback = callback_lambda2;
    micro_device_config.pUserData = &mic_pcm_data; // Передаем вектор для сохранения данных
    micro_device_config.periodSizeInFrames = 512;
}

int main()
{
    init_audio_devices();
    // 2. Инициализируем устройство
    result = ma_device_init(NULL, &system_device_config, &sys_device);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Ошибка инициализации устройства: " << result << std::endl;
        return -1;
    }
    result = ma_device_init(NULL, &micro_device_config, &mic_device);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Ошибка инициализации устройства: " << result << std::endl;
        return -1;
    }

    // 3. Начинаем захват аудио
    result = ma_device_start(&sys_device);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Ошибка запуска устройства: " << result << std::endl;
        ma_device_uninit(&sys_device);
        return -1;
    }
    result = ma_device_start(&mic_device);
    if (result != MA_SUCCESS)
    {
        std::cerr << "Ошибка запуска устройства: " << result << std::endl;
        ma_device_uninit(&mic_device);
        return -1;
    }

    std::cout << "Захват аудио начат. Запись в течение 5 секунд..." << std::endl;

    // 4. Ждем 5 секунд, пока накапливаются данные
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // 5. Останавливаем захват и очищаем ресурсы
    ma_device_stop(&sys_device);
    ma_device_stop(&mic_device);
    ma_device_uninit(&sys_device);
    ma_device_uninit(&mic_device);

    std::cout << "Захват завершен. Сохранение в WAV..." << std::endl;

    
    ac.writeWav("system_audio.wav", sys_pcm_data);
    ac.writeWav("micro_audio.wav", mic_pcm_data);

    return 0;
}