/***************************************************************************
* Copyright (c) 2016, Johan Mabille and Sylvain Corlay                     *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XSIMD_EXPONENTIAL_HPP
#define XSIMD_EXPONENTIAL_HPP

#include "xsimd_exp_reduction.hpp"
#include "xsimd_fp_manipulation.hpp"

namespace xsimd
{
    template <class T, std::size_t N>
    batch<T, N> exp(const batch<T, N>& x);

    template <class T, std::size_t N>
    batch<T, N> exp2(const batch<T, N>& x);

    template <class T, std::size_t N>
    batch<T, N> exp10(const batch<T, N>& x);

    template <class T, std::size_t N>
    batch<T, N> expm1(const batch<T, N>& x);

    /******************************
     * exponential implementation *
     ******************************/

    namespace detail
    {
        template <class B, class Tag, class T = typename B::value_type>
        struct exp_kernel;

        template <class B, class Tag>
        struct exp_kernel<B, Tag, float>
        {
            /* origin: boost/simd/arch/common/detail/simd/expo_base.hpp */
            /*
             * ====================================================
             * copyright 2016 NumScale SAS
             *
             * Distributed under the Boost Software License, Version 1.0.
             * (See copy at http://boost.org/LICENSE_1_0.txt)
             * ====================================================
             */
            static inline B compute(const B& a)
            {
                using reducer_t = exp_reduction<B, Tag>;
                B x;
                B k = reducer_t::reduce(a, x);
                x = reducer_t::approx(x);
                x = select(a <= reducer_t::minlog(), B(0.), ldexp(x, to_int(k)));
                x = select(a >= reducer_t::maxlog(), infinity<B>(), x);
                return x;
            }
        };

        template <class B, class Tag>
        struct exp_kernel<B, Tag, double>
        {
            /* origin: boost/simd/arch/common/detail/simd/expo_base.hpp */
            /*
             * ====================================================
             * copyright 2016 NumScale SAS
             *
             * Distributed under the Boost Software License, Version 1.0.
             * (See copy at http://boost.org/LICENSE_1_0.txt)
             * ====================================================
             */
            static inline B compute(const B& a)
            {
                using reducer_t = exp_reduction<B, Tag>;
                B hi, lo, x;
                B k = reducer_t::reduce(a, hi, lo, x);
                B c = reducer_t::approx(x);
                c = reducer_t::finalize(x, c, hi, lo);
                c = select(a <= reducer_t::minlog(), B(0.), ldexp(c, to_int(k)));
                c = select(a >= reducer_t::maxlog(), infinity<B>(), c);
                return c;
            }
        };
    }

    template <class T, std::size_t N>
    inline batch<T, N> exp(const batch<T, N>& x)
    {
        return detail::exp_kernel<batch<T, N>, exp_tag>::compute(x);
    }

    template <class T, std::size_t N>
    inline batch<T, N> exp2(const batch<T, N>& x)
    {
        return detail::exp_kernel<batch<T, N>, exp2_tag>::compute(x);
    }

    template <class T, std::size_t N>
    inline batch<T, N> exp10(const batch<T, N>& x)
    {
        return detail::exp_kernel<batch<T, N>, exp10_tag>::compute(x);
    }

    /************************
     * expm1 implementation *
     ************************/

    namespace detail
    {
        template <class B, class T = typename B::value_type>
        struct expm1_kernel;

        template <class B>
        struct expm1_kernel<B, float>
        {
            /* origin: boost/simd/arch/common/detail/generic/expm1_kernel.hpp */
            /*
             * ====================================================
             * copyright 2016 NumScale SAS
             *
             * Distributed under the Boost Software License, Version 1.0.
             * (See copy at http://boost.org/LICENSE_1_0.txt)
             * ====================================================
             */
            static inline B compute(const B& a)
            {
                B k = nearbyint(invlog_2<B>() * a);
                B x = fnma(k, log_2hi<B>(), a);
                x = fnma(k, log_2lo<B>(), x);
                B hx = x * B(0.5);
                B hxs = x * hx;
                B r = horner<B,
                    0X3F800000UL, // 1
                    0XBD08887FUL, // -3.3333298E-02
                    0X3ACF6DB4UL  // 1.582554
                >(hxs);
                B t = fnma(r, hx, B(3.));
                B e = hxs * ((r - t) / (B(6.) - x * t));
                e = fms(x, e, hxs);
                using i_type = as_integer_t<B>;
                i_type ik = to_int(k);
                B two2mk = bitwise_cast<B>((maxexponent<B>() - ik) << nmb<B>());
                B y = B(1.) - two2mk - (e - x);
                return ldexp(y, ik);
            }
        };

        template <class B>
        struct expm1_kernel<B, double>
        {
            /* origin: boost/simd/arch/common/detail/generic/expm1_kernel.hpp */
            /*
             * ====================================================
             * copyright 2016 NumScale SAS
             *
             * Distributed under the Boost Software License, Version 1.0.
             * (See copy at http://boost.org/LICENSE_1_0.txt)
             * ====================================================
             */
            static inline B compute(const B& a)
            {
                B k = nearbyint(invlog_2<B>() * a);
                B hi = fnma(k, log_2hi<B>(), a);
                B lo = k * log_2lo<B>();
                B x = hi - lo;
                B hxs = x * x * B(0.5);
                B r = horner<B,
                    0X3FF0000000000000ULL,
                    0XBFA11111111110F4ULL,
                    0X3F5A01A019FE5585ULL,
                    0XBF14CE199EAADBB7ULL,
                    0X3ED0CFCA86E65239ULL,
                    0XBE8AFDB76E09C32DULL
                >(hxs);
                B t = B(3.) - r * B(0.5) * x;
                B e = hxs * ((r - t) / (B(6) - x * t));
                B c = (hi - x) - lo;
                e = (x * (e - c) - c) - hxs;
                using i_type = as_integer_t<B>;
                i_type ik = to_int(k);
                B two2mk = bitwise_cast<B>((maxexponent<B>() - ik) << nmb<B>());
                B ct1 = B(1.) - two2mk - (e - x);
                B ct2 = ++(x - (e + two2mk));
                B y = select(k < B(20.), ct1, ct2);
                return ldexp(y, ik);
            }
        };
    }

    template <class T, std::size_t N>
    inline batch<T, N> expm1(const batch<T, N>& x)
    {
        using b_type = batch<T, N>;
        return select(x < logeps<b_type>(),
                      b_type(-1.),
                      select(x > maxlog<b_type>(),
                             infinity<b_type>(),
                             detail::expm1_kernel<b_type, T>::compute(x)));
    }

}

#endif
