#include "audio_controller.hpp"



void Audio_controller::bitReversePermutation(std::vector<Complex> &data)
{
    int n = data.size();
    for (int i = 1, j = 0; i < n; i++)
    {
        int bit = n >> 1;
        while (j & bit)
        {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j)
        {
            std::swap(data[i], data[j]);
        }
    }
}
void Audio_controller::fft(std::vector<Complex> &data, bool inverse)
{
    int n = data.size();
    if (!isPowerOfTwo(n))
    {
        throw std::invalid_argument("Data size must be power of two");
    }

    bitReversePermutation(data);

    for (int len = 2; len <= n; len <<= 1)
    {
        float angle = 2 * M_PI / len * (inverse ? 1 : -1);
        Complex wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < n; i += len)
        {
            Complex w(1);
            for (int j = 0; j < len / 2; j++)
            {
                int idx1 = i + j;
                int idx2 = i + j + len / 2;
                Complex u = data[idx1];
                Complex v = data[idx2] * w;
                data[idx1] = u + v;
                data[idx2] = u - v;
                w *= wlen;
            }
        }
    }

    if (inverse)
    {
        for (Complex& x : data)
        {
            x /= n;
        }
    }
}
std::vector<Complex> Audio_controller::amplitudeToFrequency(const std::vector<float>& amplitudes)
{
    int n = amplitudes.size();
    if (!isPowerOfTwo(n))
    {
        throw std::invalid_argument("Input size must be power of two");
    }

    std::vector<Complex> spectrum(n);
    for (int i = 0; i < n; i++)
    {
        spectrum[i] = Complex(amplitudes[i], 0);
    }

    fft(spectrum, false);
    return spectrum;
}

std::vector<float> Audio_controller::frequencyToAmplitude(const std::vector<Complex>& spectrum)
{
    int n = spectrum.size();
    std::vector<Complex> data = spectrum; // Создаем копию для работы

    fft(data, true); // Обратное преобразование (уже нормировано)

    std::vector<float> amplitudes;
    amplitudes.reserve(n);

    for (int i=0; i<N; i++) //(const Complex& x : data)
    {
        // Для вещественных сигналов imag ≈ 0, но лучше проверить
        if (std::abs(data[i].imag()) > 1e-5)
        {
            //std::cerr << "Non-zero imaginary part: " << x.imag() << std::endl;
        }
        amplitudes.push_back(data[i].real());
    }
    return amplitudes;
}

std::vector<std::vector<float>> Audio_controller::amplitudes_to_spectrogram(const std::vector<float> &data)
{
    std::vector<std::vector<float>>spectrogram;
    std::vector<float> signal(N);

    for(int i=0; i<(data.size()-N)/step; i++)
    {
        std::copy(data.begin()+i*step,data.begin()+i*step+N, signal.data());
        apply_window(signal);
        spectrogram.push_back(cut_modules(get_abs(amplitudeToFrequency(signal))));
        //spectrogram[i]=normalize_sum(spectrogram[i]);
        //for(int j=0;j<spectrogram[i].size();j++) if(isnan(spectrogram[i][j]))std::cout<<j<<" "<<spectrogram[i][j]<<std::endl;
        //normalize(spectrogram[i], norm_coef);
        //for(int j=0;j<spectrogram[i].size();j++) std::cout<<spectrogram[i][j]<<" ";
        //std::cout<<std::endl;//*std::max_element(spectrogram[i].begin(),spectrogram[i].end())
        spectrogram[i][0]=1e-5;
    }
    return spectrogram;
}

std::vector<float> Audio_controller::to_amplitudes(const std::vector<float> &modules)
{
    std::vector<float> res((modules.size()-step)/N*step+N,0);//(modules.size()/N*N,0);


    std::vector<float>  modules_tmp,  reconstructed(N,0);
    for(int i=0; i<modules.size()/N ; i++)
    {
        modules_tmp.resize(N/2+1);
        std::copy(modules.begin()+i*N,modules.begin()+i*N+N, modules_tmp.data());
        modules_tmp=recover_modules(modules_tmp);

        reconstructed=griffinLim(modules_tmp,15);

        apply_window(reconstructed);


        for (int j = 0;  j< N; j++)
        {
            res[i*step+j]+=reconstructed[j];
        }

        //std::cout << i*step+N<<" "<<res.size()<< std::endl;
    }

    return res;
}
std::vector<float> Audio_controller::griffinLimFromSpectrogram(
    const std::vector<std::vector<float>>& spectrogram,
    int iterations)
{
    int frameSize = window.size();
    int hopSize = step;
    int numFrames = spectrogram.size();

    // Рассчитываем длину выходного сигнала
    int outputLength = (numFrames - 1) * hopSize + frameSize;
    std::vector<float> signal(outputLength, 0.0f);
    std::vector<float> windowSum(outputLength, 0.0f);

    // Инициализация случайным сигналом
    std::default_random_engine generator;
    std::normal_distribution<float> distribution(0.0f, 1.0f);
    for(float& sample : signal)
    {
        sample = distribution(generator);
    }

    for(int iter = 0; iter < iterations; iter++)
    {
        std::vector<float> newSignal(outputLength, 0.0f);

        for(int i = 0; i < numFrames; i++)
        {
            int start = i * hopSize;
            std::vector<float> frame(frameSize);

            // Копируем фрейм из текущего сигнала
            for(int j = 0; j < frameSize; j++)
            {
                if(start + j < signal.size())
                {
                    frame[j] = signal[start + j] * window[j];
                }
                else
                {
                    frame[j] = 0.0f;
                }
            }

            // Переходим в частотную область
            std::vector<Complex> spectrum = amplitudeToFrequency(frame);

            // Корректируем амплитуды
            for(int j = 0; j < frameSize; j++)
            {
                float currentMag = std::abs(spectrum[j]);
                float targetMag = spectrogram[i][j];

                if(currentMag > 1e-6f)
                {
                    spectrum[j] = spectrum[j] * (targetMag / currentMag);
                }
                else
                {
                    spectrum[j] = Complex(targetMag, 0.0f);
                }
            }

            // Обратное преобразование
            std::vector<float> newFrame = frequencyToAmplitude(spectrum);

            // Накладываем фрейм с перекрытием
            for(int j = 0; j < frameSize; j++)
            {
                if(start + j < newSignal.size())
                {
                    newSignal[start + j] += newFrame[j] * window[j];
                    windowSum[start + j] += window[j] * window[j];
                }
            }
        }

        // Нормализация
        for(int i = 0; i < signal.size(); i++)
        {
            if(windowSum[i] > 1e-6f)
            {
                signal[i] = newSignal[i] / windowSum[i];
            }
        }

        // Сброс суммы окон
        std::fill(windowSum.begin(), windowSum.end(), 0.0f);
    }

    return signal;
}
std::vector<float> Audio_controller:: griffinLimOverlap(const std::vector<float>& magnitudes, int iterations)
{
    if (!isPowerOfTwo(N))
    {
        throw std::invalid_argument("Frame size must be power of two");
    }

    int numFrames = (magnitudes.size() - N) / step + 1;
    initWindow();
    std::vector<float> outputSignal(magnitudes.size(), 0.0f);
    std::vector<float> windowSum(magnitudes.size(), 0.0f);

    // Инициализация случайным сигналом
    std::vector<float> signal(magnitudes.size());
    std::default_random_engine generator;
    std::normal_distribution<float> distribution(0.0f, 1.0f);
    for (float& sample : signal)
    {
        sample = distribution(generator);
    }

    for (int iter = 0; iter < iterations; ++iter)
    {
        std::vector<float> newSignal(magnitudes.size(), 0.0f);

        for (int i = 0; i < numFrames; ++i)
        {
            int pos = i * step;

            // Вырезаем фрейм с оконной функцией
            std::vector<float> frame(N);
            for (int j = 0; j < N; ++j)
            {
                if (pos + j < signal.size())
                {
                    frame[j] = signal[pos + j] * window[j];
                }
            }

            // Преобразуем в частотную область
            std::vector<Complex> spectrum = amplitudeToFrequency(frame);

            // Корректируем амплитуды
            for (int k = 0; k < N; ++k)
            {
                float targetMag = magnitudes[std::min(pos + k, (int)magnitudes.size() - 1)];
                float currentMag = std::abs(spectrum[k]);
                if (currentMag > 1e-6f)
                {
                    spectrum[k] = spectrum[k] * (targetMag / currentMag);
                }
                else
                {
                    spectrum[k] = Complex(targetMag, 0.0f);
                }
            }

            // Обратное преобразование
            std::vector<float> newFrame = frequencyToAmplitude(spectrum);

            // Накладываем фрейм с перекрытием
            for (int j = 0; j < N; ++j)
            {
                if (pos + j < newSignal.size())
                {
                    newSignal[pos + j] += newFrame[j] * window[j];
                    windowSum[pos + j] += window[j] * window[j];
                }
            }
        }

        // Нормализация перекрытия
        for (int i = 0; i < signal.size(); ++i)
        {
            if (windowSum[i] > 1e-6f)
            {
                signal[i] = newSignal[i] / windowSum[i];
            }
        }

        // Сброс суммы окон для следующей итерации
        std::fill(windowSum.begin(), windowSum.end(), 0.0f);
    }

    return signal;
}

std::vector<float> Audio_controller::griffinLim(const std::vector<float>& magnitudeSpectrum, int iterations)
{
    int n = magnitudeSpectrum.size();
    if (!isPowerOfTwo(n))
    {
        throw std::invalid_argument("Magnitude spectrum size must be power of two");
    }

    // 1. Инициализация случайным сигналом
    std::vector<float> signal(n);
    std::default_random_engine generator;
    std::normal_distribution<float> distribution(0.0f, 1.0f);

    for (int i = 0; i < n; ++i)
    {
        signal[i] = distribution(generator);
    }

    // 2. Итеративная оптимизация
    for (int iter = 0; iter < iterations; ++iter)
    {
        // Прямое преобразование Фурье
        std::vector<Complex> spectrum = amplitudeToFrequency(signal);

        // Замена амплитуд на целевые с сохранением фаз
        for (int i = 0; i < n; ++i)
        {
            float current_mag = std::abs(spectrum[i]);
            float target_mag = magnitudeSpectrum[i];

            if (current_mag > 1e-6f)
            {
                float scale = target_mag / current_mag;
                spectrum[i] = spectrum[i] * scale;
            }
            else
            {
                spectrum[i] = Complex(target_mag, 0.0f);
            }
            //if(i==n-1)std::cout<<target_mag<<" "<<current_mag<<std::endl;
        }

        // Обратное преобразование Фурье
        signal = frequencyToAmplitude(spectrum);
    }

    return signal;
}

//files
std::vector<float> Audio_controller::pcmToVector(const char* filename, bool normalize)
{
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Предполагаем 16-битные PCM данные
    std::vector<int16_t> intBuffer(size / sizeof(int16_t));
    file.read(reinterpret_cast<char*>(intBuffer.data()), size);
    file.close();

    std::vector<float> floatBuffer(intBuffer.size());
    for (size_t i = 0; i < intBuffer.size(); ++i)
    {
        floatBuffer[i] = normalize ? intBuffer[i] / 32768.0f : intBuffer[i];
    }

    return floatBuffer;
}
void Audio_controller::writeWav(const std::string& filename, const std::vector<float>& samples)
{
    std::ofstream file(filename, std::ios::binary);
    if (!file)
    {
        std::cerr << "Ошибка открытия файла!" << std::endl;
        return;
    }

    // Параметры WAV
    const int numChannels = 1;  // Моно
    const int bitsPerSample = 16;
    const int dataSize = samples.size() * numChannels * bitsPerSample / 8;
    const int byteRate = sample_rate * numChannels * bitsPerSample / 8;
    const int blockAlign = numChannels * bitsPerSample / 8;

    // RIFF-заголовок
    file.write("RIFF", 4);
    int fileSize = 36 + dataSize;  // 36 = размер заголовка без данных
    file.write(reinterpret_cast<const char*>(&fileSize), 4);
    file.write("WAVE", 4);

    // Форматный чанк (fmt)
    file.write("fmt ", 4);
    int fmtChunkSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtChunkSize), 4);
    short audioFormat = 1;  // PCM
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&numChannels), 2);
    file.write(reinterpret_cast<const char*>(&sample_rate), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // Чанк данных (data)
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);

    // Запись аудиоданных с сохранением знака
    for (float sample : samples)
    {
        // 1. Ограничение диапазона (-1.0 до 1.0)
        sample = std::clamp(sample, -1.0f, 1.0f);

        // 2. Правильное масштабирование в 16-битное целое
        short intSample;
        if (sample >= 0)
        {
            intSample = static_cast<short>(sample * 32767.0f);
        }
        else
        {
            intSample = static_cast<short>(sample * 32768.0f);
        }

        file.write(reinterpret_cast<const char*>(&intSample), 2);
    }
    file.close();
    std::cout << "saved: " << filename << std::endl;
}
void Audio_controller::add_frequency_to_block(std::vector<float> &data, float &phase, float frequency, float amplitude)
{
    for (int i = 0; i < data.size(); ++i)
    {
        phase += 2.0f * M_PI * frequency / sample_rate;
        if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;

        data[i] = amplitude * sin(phase);
    }
}
void Audio_controller::add_frequency_to_block(std::vector<float> &data, int t, float frequency, float amplitude)
{
    for (int i = 0; i < data.size(); ++i)
    {
        data[i] += amplitude * sin(2 * M_PI *frequency * (float)(t*data.size()+i)/sample_rate);
    }
}

std::vector<float> Audio_controller::complex_to_float(std::vector<Complex> cv)
{
    std::vector<float> res(cv.size()*2);
    for(int i=0; i<cv.size()*2; i+=2)
    {
        res[i]=cv[i/2].real();
    }
    for(int i=0; i<cv.size()*2; i+=2)
    {
        res[i+1]=cv[i/2].imag();
        //res[i+1]=std::remainder(res[i+1], 2 * M_PI);
        //res[i+1]=atan2(sin(res[i+1]), cos(res[i+1]));
        ///while(res[i+1]>2*M_PI)res[i+1]-=2*M_PI;
        ///while(res[i+1]<-2*M_PI) res[i+1]+=2*M_PI;
    }
    return res;
}
std::vector<Complex> Audio_controller::float_to_complex(std::vector<float> fv)
{
    std::vector<Complex> res(fv.size()/2);
    for(int i=0; i<fv.size(); i+=2)
    {
        res[i/2]= {fv[i],fv[i+1]};
    }
    return res;
}



