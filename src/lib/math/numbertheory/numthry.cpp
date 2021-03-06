/*
* Number Theory Functions
* (C) 1999-2011,2016 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <botan/numthry.h>
#include <botan/pow_mod.h>
#include <botan/reducer.h>
#include <botan/monty.h>
#include <botan/internal/bit_ops.h>
#include <botan/internal/mp_core.h>
#include <botan/internal/ct_utils.h>
#include <botan/internal/monty_exp.h>
#include <algorithm>

namespace Botan {

/*
* Return the number of 0 bits at the end of n
*/
size_t low_zero_bits(const BigInt& n)
   {
   size_t low_zero = 0;

   if(n.is_positive() && n.is_nonzero())
      {
      for(size_t i = 0; i != n.size(); ++i)
         {
         const word x = n.word_at(i);

         if(x)
            {
            low_zero += ctz(x);
            break;
            }
         else
            low_zero += BOTAN_MP_WORD_BITS;
         }
      }

   return low_zero;
   }

/*
* Calculate the GCD
*/
BigInt gcd(const BigInt& a, const BigInt& b)
   {
   if(a.is_zero() || b.is_zero())
      return 0;
   if(a == 1 || b == 1)
      return 1;

   BigInt X[2] = { a, b };
   X[0].set_sign(BigInt::Positive);
   X[1].set_sign(BigInt::Positive);

   const size_t shift = std::min(low_zero_bits(X[0]), low_zero_bits(X[1]));

   X[0] >>= shift;
   X[1] >>= shift;

   while(X[0].is_nonzero())
      {
      X[0] >>= low_zero_bits(X[0]);
      X[1] >>= low_zero_bits(X[1]);

      const uint8_t sel = static_cast<uint8_t>(X[0] >= X[1]);

      X[sel^1] -= X[sel];
      X[sel^1] >>= 1;
      }

   return (X[1] << shift);
   }

/*
* Calculate the LCM
*/
BigInt lcm(const BigInt& a, const BigInt& b)
   {
   return ((a * b) / gcd(a, b));
   }

/*
Sets result to a^-1 * 2^k mod a
with n <= k <= 2n
Returns k

"The Montgomery Modular Inverse - Revisited" Çetin Koç, E. Savas
https://citeseerx.ist.psu.edu/viewdoc/citations?doi=10.1.1.75.8377

A const time implementation of this algorithm is described in
"Constant Time Modular Inversion" Joppe W. Bos
http://www.joppebos.com/files/CTInversion.pdf
*/
size_t almost_montgomery_inverse(BigInt& result,
                                 const BigInt& a,
                                 const BigInt& p)
   {
   size_t k = 0;

   BigInt u = p, v = a, r = 0, s = 1;

   while(v > 0)
      {
      if(u.is_even())
         {
         u >>= 1;
         s <<= 1;
         }
      else if(v.is_even())
         {
         v >>= 1;
         r <<= 1;
         }
      else if(u > v)
         {
         u -= v;
         u >>= 1;
         r += s;
         s <<= 1;
         }
      else
         {
         v -= u;
         v >>= 1;
         s += r;
         r <<= 1;
         }

      ++k;
      }

   if(r >= p)
      {
      r -= p;
      }

   result = p - r;

   return k;
   }

BigInt normalized_montgomery_inverse(const BigInt& a, const BigInt& p)
   {
   BigInt r;
   size_t k = almost_montgomery_inverse(r, a, p);

   for(size_t i = 0; i != k; ++i)
      {
      if(r.is_odd())
         r += p;
      r >>= 1;
      }

   return r;
   }

BigInt ct_inverse_mod_odd_modulus(const BigInt& n, const BigInt& mod)
   {
   if(n.is_negative() || mod.is_negative())
      throw Invalid_Argument("ct_inverse_mod_odd_modulus: arguments must be non-negative");
   if(mod < 3 || mod.is_even())
      throw Invalid_Argument("Bad modulus to ct_inverse_mod_odd_modulus");
   if(n >= mod)
      throw Invalid_Argument("ct_inverse_mod_odd_modulus n >= mod not supported");

   /*
   This uses a modular inversion algorithm designed by Niels Möller
   and implemented in Nettle. The same algorithm was later also
   adapted to GMP in mpn_sec_invert.

   It can be easily implemented in a way that does not depend on
   secret branches or memory lookups, providing resistance against
   some forms of side channel attack.

   There is also a description of the algorithm in Appendix 5 of "Fast
   Software Polynomial Multiplication on ARM Processors using the NEON Engine"
   by Danilo Câmara, Conrado P. L. Gouvêa, Julio López, and Ricardo
   Dahab in LNCS 8182
      https://conradoplg.cryptoland.net/files/2010/12/mocrysen13.pdf

   Thanks to Niels for creating the algorithm, explaining some things
   about it, and the reference to the paper.
   */

   // todo allow this to be pre-calculated and passed in as arg
   BigInt mp1o2 = (mod + 1) >> 1;

   const size_t mod_words = mod.sig_words();
   BOTAN_ASSERT(mod_words > 0, "Not empty");

   BigInt a = n;
   BigInt b = mod;
   BigInt u = 1, v = 0;

   a.grow_to(mod_words);
   u.grow_to(mod_words);
   v.grow_to(mod_words);
   mp1o2.grow_to(mod_words);

   secure_vector<word>& a_w = a.get_word_vector();
   secure_vector<word>& b_w = b.get_word_vector();
   secure_vector<word>& u_w = u.get_word_vector();
   secure_vector<word>& v_w = v.get_word_vector();

   CT::poison(a_w.data(), a_w.size());
   CT::poison(b_w.data(), b_w.size());
   CT::poison(u_w.data(), u_w.size());
   CT::poison(v_w.data(), v_w.size());

   // Only n.bits() + mod.bits() iterations are required, but avoid leaking the size of n
   size_t bits = 2 * mod.bits();

   while(bits--)
      {
      /*
      const word odd = a.is_odd();
      a -= odd * b;
      const word underflow = a.is_negative();
      b += a * underflow;
      a.set_sign(BigInt::Positive);

      a >>= 1;

      if(underflow)
         {
         std::swap(u, v);
         }

      u -= odd * v;
      u += u.is_negative() * mod;

      const word odd_u = u.is_odd();

      u >>= 1;
      u += mp1o2 * odd_u;
      */

      const word odd_a = a_w[0] & 1;

      //if(odd_a) a -= b
      word underflow = bigint_cnd_sub(odd_a, a_w.data(), b_w.data(), mod_words);

      //if(underflow) { b -= a; a = abs(a); swap(u, v); }
      bigint_cnd_add(underflow, b_w.data(), a_w.data(), mod_words);
      bigint_cnd_abs(underflow, a_w.data(), mod_words);
      bigint_cnd_swap(underflow, u_w.data(), v_w.data(), mod_words);

      // a >>= 1
      bigint_shr1(a_w.data(), mod_words, 0, 1);

      //if(odd_a) u -= v;
      word borrow = bigint_cnd_sub(odd_a, u_w.data(), v_w.data(), mod_words);

      // if(borrow) u += p
      bigint_cnd_add(borrow, u_w.data(), mod.data(), mod_words);

      const word odd_u = u_w[0] & 1;

      // u >>= 1
      bigint_shr1(u_w.data(), mod_words, 0, 1);

      //if(odd_u) u += mp1o2;
      bigint_cnd_add(odd_u, u_w.data(), mp1o2.data(), mod_words);
      }

   CT::unpoison(a_w.data(), a_w.size());
   CT::unpoison(b_w.data(), b_w.size());
   CT::unpoison(u_w.data(), u_w.size());
   CT::unpoison(v_w.data(), v_w.size());

   BOTAN_ASSERT(a.is_zero(), "A is zero");

   if(b != 1)
      return 0;

   return v;
   }

/*
* Find the Modular Inverse
*/
BigInt inverse_mod(const BigInt& n, const BigInt& mod)
   {
   if(mod.is_zero())
      throw BigInt::DivideByZero();
   if(mod.is_negative() || n.is_negative())
      throw Invalid_Argument("inverse_mod: arguments must be non-negative");

   if(n.is_zero() || (n.is_even() && mod.is_even()))
      return 0; // fast fail checks

   if(mod.is_odd() && n < mod)
      return ct_inverse_mod_odd_modulus(n, mod);

   return inverse_euclid(n, mod);
   }

BigInt inverse_euclid(const BigInt& n, const BigInt& mod)
   {
   if(mod.is_zero())
      throw BigInt::DivideByZero();
   if(mod.is_negative() || n.is_negative())
      throw Invalid_Argument("inverse_mod: arguments must be non-negative");

   if(n.is_zero() || (n.is_even() && mod.is_even()))
      return 0; // fast fail checks

   BigInt u = mod, v = n;
   BigInt A = 1, B = 0, C = 0, D = 1;

   while(u.is_nonzero())
      {
      const size_t u_zero_bits = low_zero_bits(u);
      u >>= u_zero_bits;
      for(size_t i = 0; i != u_zero_bits; ++i)
         {
         if(A.is_odd() || B.is_odd())
            { A += n; B -= mod; }
         A >>= 1; B >>= 1;
         }

      const size_t v_zero_bits = low_zero_bits(v);
      v >>= v_zero_bits;
      for(size_t i = 0; i != v_zero_bits; ++i)
         {
         if(C.is_odd() || D.is_odd())
            { C += n; D -= mod; }
         C >>= 1; D >>= 1;
         }

      if(u >= v) { u -= v; A -= C; B -= D; }
      else       { v -= u; C -= A; D -= B; }
      }

   if(v != 1)
      return 0; // no modular inverse

   while(D.is_negative()) D += mod;
   while(D >= mod) D -= mod;

   return D;
   }

word monty_inverse(word input)
   {
   if(input == 0)
      throw Exception("monty_inverse: divide by zero");

   word b = input;
   word x2 = 1, x1 = 0, y2 = 0, y1 = 1;

   // First iteration, a = n+1
   word q = bigint_divop(1, 0, b);
   word r = (MP_WORD_MAX - q*b) + 1;
   word x = x2 - q*x1;
   word y = y2 - q*y1;

   word a = b;
   b = r;
   x2 = x1;
   x1 = x;
   y2 = y1;
   y1 = y;

   while(b > 0)
      {
      q = a / b;
      r = a - q*b;
      x = x2 - q*x1;
      y = y2 - q*y1;

      a = b;
      b = r;
      x2 = x1;
      x1 = x;
      y2 = y1;
      y1 = y;
      }

   const word check = y2 * input;
   BOTAN_ASSERT_EQUAL(check, 1, "monty_inverse result is inverse of input");

   // Now invert in addition space
   y2 = (MP_WORD_MAX - y2) + 1;

   return y2;
   }

/*
* Modular Exponentiation
*/
BigInt power_mod(const BigInt& base, const BigInt& exp, const BigInt& mod)
   {
   if(mod.is_negative() || mod == 1)
      {
      return 0;
      }

   if(base.is_zero() || mod.is_zero())
      {
      if(exp.is_zero())
         return 1;
      return 0;
      }

   Power_Mod pow_mod(mod);

   /*
   * Calling set_base before set_exponent means we end up using a
   * minimal window. This makes sense given that here we know that any
   * precomputation is wasted.
   */

   if(base.is_negative())
      {
      pow_mod.set_base(-base);
      pow_mod.set_exponent(exp);
      if(exp.is_even())
         return pow_mod.execute();
      else
         return (mod - pow_mod.execute());
      }
   else
      {
      pow_mod.set_base(base);
      pow_mod.set_exponent(exp);
      return pow_mod.execute();
      }
   }

namespace {

bool mr_witness(BigInt&& y,
                const Modular_Reducer& reducer_n,
                const BigInt& n_minus_1, size_t s)
   {
   if(y == 1 || y == n_minus_1)
      return false;

   for(size_t i = 1; i != s; ++i)
      {
      y = reducer_n.square(y);

      if(y == 1) // found a non-trivial square root
         return true;

      /*
      -1 is the trivial square root of unity, so ``a`` is not a
      witness for this number - give up
      */
      if(y == n_minus_1)
         return false;
      }

   return true; // is a witness
   }

size_t mr_test_iterations(size_t n_bits, size_t prob, bool random)
   {
   const size_t base = (prob + 2) / 2; // worst case 4^-t error rate

   /*
   * If the candidate prime was maliciously constructed, we can't rely
   * on arguments based on p being random.
   */
   if(random == false)
      return base;

   /*
   * For randomly chosen numbers we can use the estimates from
   * http://www.math.dartmouth.edu/~carlp/PDF/paper88.pdf
   *
   * These values are derived from the inequality for p(k,t) given on
   * the second page.
   */
   if(prob <= 128)
      {
      if(n_bits >= 1536)
         return 4; // < 2^-133
      if(n_bits >= 1024)
         return 6; // < 2^-133
      if(n_bits >= 512)
         return 12; // < 2^-129
      if(n_bits >= 256)
         return 29; // < 2^-128
      }

   /*
   If the user desires a smaller error probability than we have
   precomputed error estimates for, just fall back to using the worst
   case error rate.
   */
   return base;
   }

}

/*
* Test for primality using Miller-Rabin
*/
bool is_prime(const BigInt& n, RandomNumberGenerator& rng,
              size_t prob, bool is_random)
   {
   if(n == 2)
      return true;
   if(n <= 1 || n.is_even())
      return false;

   // Fast path testing for small numbers (<= 65521)
   if(n <= PRIMES[PRIME_TABLE_SIZE-1])
      {
      const uint16_t num = static_cast<uint16_t>(n.word_at(0));

      return std::binary_search(PRIMES, PRIMES + PRIME_TABLE_SIZE, num);
      }

   const size_t test_iterations = mr_test_iterations(n.bits(), prob, is_random);

   const BigInt n_minus_1 = n - 1;
   const size_t s = low_zero_bits(n_minus_1);
   const BigInt nm1_s = n_minus_1 >> s;

   const Modular_Reducer mod_n(n);
   auto monty_n = std::make_shared<Montgomery_Params>(n, mod_n);

   const size_t powm_window = 4;

   for(size_t i = 0; i != test_iterations; ++i)
      {
      const BigInt a = BigInt::random_integer(rng, 2, n_minus_1);

      auto powm_a_n = monty_precompute(monty_n, a, powm_window);

      BigInt y = monty_execute(*powm_a_n, nm1_s);

      if(mr_witness(std::move(y), mod_n, n_minus_1, s))
         return false;
      }

   return true;
   }

}
