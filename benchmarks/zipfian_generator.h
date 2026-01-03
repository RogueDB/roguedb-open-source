/*
 * Copyright (C) Jacob Bartholomew Blankenship - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Written by Jacob Bartholomew Blankenship [jacob.blankenship@roguedb.com] - January 2022
 */

#ifndef ZIPFIAN_GENERATOR_H
#define ZIPFIAN_GENERATOR_H

#include <algorithm>
#include <cstdint>
#include <random>

#include "benchmarks/coroutines.h"

namespace rogue
{
    namespace utilities
    {
        class ZipfianGenerator
        {
        private:
            uint64_t m_numberOfElements; // -> theta
            double m_exponent; // -> zipfianConstant;
            double m_generalizedHarmonicNumber;
            double m_normalizationConstant; // Typically a harmonic number
            std::uniform_real_distribution<double> m_distribution;
            static constexpr double m_epsilon{ 1e-8 };
            std::random_device randomizer{};
            std::mt19937 generator{ randomizer() };

            static double exponentialMinus1OverX(const double x)
            {
                return (std::abs(x) > m_epsilon)
                    ? std::expm1(x) / x
                    : 1.0 + x/2.0 * (1.0 + x/3.0 * (1.0 + x/4.0));
            }

            static double log1PlusXOverX(const double x)
            {
                return (std::abs(x) > m_epsilon)
                    ? std::log1p(x) / x
                    : 1.0 - x * ((1/2.0) - x * ((1/3.0) - x * (1/4.0)));
            }
        
            const double harmonic(const double x)
            {
                const double logX{ std::log(x) };
                return exponentialMinus1OverX((1.0 - m_exponent) * logX) * logX;
            }

            const double inverseHarmonic(const double x)
            {
                const double t{ std::max(-1.0, x * (1.0 - m_exponent)) };
                return std::exp(log1PlusXOverX(t) * x);
            }

            const double hat(const double x)
            {
                return std::exp(-m_exponent * std::log(x));
            }
            
        public:
            ZipfianGenerator(
                const uint64_t numberOfElements,
                const double exponent) :
                m_numberOfElements{ numberOfElements},
                m_exponent{ exponent },
                m_generalizedHarmonicNumber{ harmonic(1.5) - 1.0 },
                m_normalizationConstant{ harmonic(m_numberOfElements + .5) },
                m_distribution{ m_generalizedHarmonicNumber, m_normalizationConstant }
            {}

            uint64_t next()
            {
                while(true)
                {
                    const double u{ m_distribution(generator) };
                    const double x{ inverseHarmonic(u) };
                    const uint64_t k = std::max(
                        uint64_t{1}, std::min(
                            m_numberOfElements, static_cast<uint64_t>(std::round(x))));
                    if(u >= harmonic(k + .5) - hat(k))
                    {
                        return k;
                    }
                }
            }

            concepts::Generator<uint64_t> generate(std::mt19937& rng)
            {
                while(true)
                {
                    const double u{ m_distribution(rng) };
                    const double x{ inverseHarmonic(u) };
                    const uint64_t k = std::max(
                        uint64_t{1}, std::min(
                            m_numberOfElements, static_cast<uint64_t>(std::round(x))));
                    if(u >= harmonic(k + .5) - hat(k))
                    {
                        co_yield k;
                    }
                }
            }
            
        };
    }
}

#endif //ZIPFIAN_GENERATOR_H