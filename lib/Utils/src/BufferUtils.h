#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace jalari::utils
{

    class BufferUtils
    {
    public:
        template <std::size_t N>
        static void clear(std::array<std::uint8_t, N> &buffer)
        {
            buffer.fill(0);
        }

        template <std::size_t N>
        static std::size_t copy(const std::uint8_t *src, std::size_t srcSize, std::array<std::uint8_t, N> &dst)
        {
            const std::size_t copySize = (srcSize < N) ? srcSize : N;
            for (std::size_t i = 0; i < copySize; ++i)
            {
                dst[i] = src[i];
            }
            return copySize;
        }
    };

} // namespace jalari::utils
