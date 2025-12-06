#pragma once

#define _USE_MATH_DEFINES
#define M_PI 3.1415926

#include <iostream>
#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <cstdint>
#include <random>
#include <iomanip>
typedef std::complex<float> Complex;
class Audio_controller
{
protected:
    int N, sample_rate, step;
    float scaling_coef;
    std::vector<float> window;

    // �������� �� ������� ������
    bool isPowerOfTwo(int n)
    {
        return (n > 0) && ((n & (n - 1)) == 0);
    }

    // ���������� ������ �� ������� ������
    int nextPowerOfTwo(int n)
    {
        if (isPowerOfTwo(n))
            return n;
        int power = 1;
        while (power < n)
            power <<= 1;
        return power;
    }

    // ���-��������� ������������
    void bitReversePermutation(std::vector<Complex> &data);

    // ������ � �������� �������������� ����� (inverse = true ��� IFFT)
    void fft(std::vector<Complex> &data, bool inverse = false);

public:
    void initWindow()
    {
        window.resize(N);

        for (int i = 0; i < N; ++i)
        {
            /// window[i] = (0.5 * (1 - cos(2 * M_PI * i / N)));
            window[i] = (0.54 - 0.46 * cos(2 * M_PI * i / N));
        }
        // for(int i=0;i<N/2;i++)std::cout<<window[i]+window[i+N/2]<<" ";std::cout<<std::endl;
    }
    void apply_window(std::vector<float> &signal)
    {
        for (int i = 0; i < N; ++i)
            signal[i] *= window[i];
    }

    void smooth_audio(std::vector<float> &signal)
    {
        for (float &sample : signal)
        {
            sample = tanh(sample * 0.8f) * 1.2f; // ������ soft clipping
        }
    }

    std::vector<std::vector<float>> amplitudes_to_spectrogram(const std::vector<float> &data);
    std::vector<float> to_amplitudes(const std::vector<float> &modules);

    std::vector<float> griffinLimFromSpectrogram(
        const std::vector<std::vector<float>> &spectrogram,
        int iterations);
    std::vector<float> griffinLimOverlap(const std::vector<float> &magnitudes, int iterations);
    std::vector<float> griffinLim(const std::vector<float> &magnitudeSpectrum, int iterations);

    void normalize(std::vector<float> &vec, float coef = 0)
    {
        if (coef == 0) // coef=*std::max_element(vec.begin(),vec.end());
            for (int i = 0; i < vec.size(); i++)
                if (coef < abs(vec[i]))
                    coef = abs(vec[i]);
        if(coef<=1e-7) coef=1;
        for (int i = 0; i < vec.size(); i++)
            vec[i] /= coef;
    }
    Audio_controller(int a_N = 1024, int a_step = 0, int a_sample_rate = 24000, float a_scaling_coef=1)
    {
        N = a_N;
        step = a_step == 0 ? N : a_step;
        sample_rate = a_sample_rate;
        scaling_coef=a_scaling_coef;
        initWindow();
    }

    void add_frequency_to_block(std::vector<float> &data, float &phase, float frequency, float amplitude);
    void add_frequency_to_block(std::vector<float> &data, int t, float frequency, float amplitude);

    // �������������� �������� � �������
    std::vector<Complex> amplitudeToFrequency(const std::vector<float> &amplitudes);

    // �������������� ������ ������� � ���������
    std::vector<float> frequencyToAmplitude(const std::vector<Complex> &spectrum);

    std::vector<float> complex_to_float(std::vector<Complex> cv);
    std::vector<Complex> float_to_complex(std::vector<float> fv);
    std::vector<float> get_real(std::vector<Complex> cv)
    {
        std::vector<float> res(cv.size());
        for (int i = 0; i < res.size(); i++)
            res[i] = cv[i].real();
        return res;
    }
    std::vector<float> get_imag(std::vector<Complex> cv)
    {
        std::vector<float> res(cv.size());
        for (int i = 0; i < res.size(); i++)
            res[i] = cv[i].imag();
        return res;
    }
    std::vector<float> get_abs(std::vector<Complex> cv)
    {
        std::vector<float> res(cv.size());
        for (int i = 0; i < res.size(); i++)
            res[i] = abs(cv[i]);
        return res;
    }
    std::vector<float> cut_modules(const std::vector<float> &modules)
    {
        std::vector<float> res = modules;
        res.resize(modules.size() / 2 + 1);
        res.erase(res.begin());

        normalize(res,scaling_coef);
        //res=normalize_f(res);

        return res;
    }
    std::vector<float> recover_modules(const std::vector<float> &modules)
    {
        std::vector<float> res(modules.size() * 2, 0);
        for (int i = 0; i < modules.size(); i++)
            res[i + 1] = modules[i];
        for (int i = 0; i < modules.size(); i++)
            res[res.size() - i - 1] = modules[i];

        normalize(res,1.0/scaling_coef);
        //res=denormalize_f(res);
        
        return res;
    }
    /*std::vector<Complex> to_complex(const std::vector<float> &real,const std::vector<float> &imag)
    {
        std::vector<Complex> res(real.size());
        for(int i=0; i<res.size(); i++)res[i]=Complex(real[i],imag[i]);
        return res;
    }*/
    std::vector<Complex> modules_to_complex(const std::vector<float> &modules, const std::vector<float> &phases)
    {
        std::vector<Complex> res(modules.size());
        for (int i = 0; i < res.size(); i++)
            res[i] = Complex(modules[i] * cos(phases[i]), modules[i] * sin(phases[i]));
        return res;
    }
    // files
    std::vector<float> pcmToVector(const char *filename, bool normalize = true);

    void writeWav(const std::string &filename, const std::vector<float> &samples);

    // getters
    int get_n()
    {
        return N;
    }
    int get_sample_rate()
    {
        return sample_rate;
    }
    std::vector<float> normalize_f(std::vector<float> vec)
    {
        for (int i = 0; i < vec.size(); i++)
            vec[i] =sqrt(vec[i]);
        return vec;
    }
    std::vector<float> denormalize_f(std::vector<float> vec)
    {
        for (int i = 0; i < vec.size(); i++)
            vec[i] =std::pow(vec[i],2);
        return vec;
    }
};
