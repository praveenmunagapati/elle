//
// ---------- header ----------------------------------------------------------
//
// project       elle
//
// license       infinit
//
// file          /home/mycure/infinit/elle/radix/Parameters.hxx
//
// created       julien quintard   [sun feb 21 15:29:32 2010]
// updated       julien quintard   [fri jun 10 00:08:13 2011]
//

#ifndef ELLE_RADIX_PARAMETERS_HXX
#define ELLE_RADIX_PARAMETERS_HXX

//
// ---------- includes --------------------------------------------------------
//

#include <elle/core/Natural.hh>

#include <elle/standalone/Maid.hh>
#include <elle/standalone/Report.hh>

#include <elle/radix/Status.hh>
#include <elle/radix/Base.hh>

#include <elle/idiom/Open.hh>

namespace elle
{
  using namespace core;
  using namespace standalone;
  using namespace radix;

  namespace radix
  {

//
// ---------- structures ------------------------------------------------------
//

    ///
    /// zero parameter.
    ///
    template <>
    struct Parameters<>:
      public Base<Parameters>
    {
      //
      // constants
      //
      static const Natural32	Quantum = 0;
    };

    ///
    /// one parameter.
    ///
    template <typename T1>
    struct Parameters<T1>:
      public Base<Parameters>
    {
      //
      // types
      //
      typedef T1		P1;

      //
      // constants
      //
      static const Natural32	Quantum = 1;
    };

    ///
    /// two parameter.
    ///
    template <typename T1,
	      typename T2>
    struct Parameters<T1,
		      T2>:
      public Base<Parameters>
    {
      //
      // types
      //
      typedef T1		P1;
      typedef T2		P2;

      //
      // constants
      //
      static const Natural32	Quantum = 2;
    };

    ///
    /// three parameter.
    ///
    template <typename T1,
	      typename T2,
	      typename T3>
    struct Parameters<T1,
		      T2,
		      T3>:
      public Base<Parameters>
    {
      //
      // types
      //
      typedef T1		P1;
      typedef T2		P2;
      typedef T3		P3;

      //
      // constants
      //
      static const Natural32	Quantum = 3;
    };

    ///
    /// four parameter.
    ///
    template <typename T1,
	      typename T2,
	      typename T3,
	      typename T4>
    struct Parameters<T1,
		      T2,
		      T3,
		      T4>:
      public Base<Parameters>
    {
      //
      // types
      //
      typedef T1		P1;
      typedef T2		P2;
      typedef T3		P3;
      typedef T4		P4;

      //
      // constants
      //
      static const Natural32	Quantum = 4;
    };

    ///
    /// five parameter.
    ///
    template <typename T1,
	      typename T2,
	      typename T3,
	      typename T4,
	      typename T5>
    struct Parameters<T1,
		      T2,
		      T3,
		      T4,
		      T5>:
      public Base<Parameters>
    {
      //
      // types
      //
      typedef T1		P1;
      typedef T2		P2;
      typedef T3		P3;
      typedef T4		P4;
      typedef T5		P5;

      //
      // constants
      //
      static const Natural32	Quantum = 5;
    };

    ///
    /// six parameter.
    ///
    template <typename T1,
	      typename T2,
	      typename T3,
	      typename T4,
	      typename T5,
	      typename T6>
    struct Parameters<T1,
		      T2,
		      T3,
		      T4,
		      T5,
		      T6>:
      public Base<Parameters>
    {
      //
      // types
      //
      typedef T1		P1;
      typedef T2		P2;
      typedef T3		P3;
      typedef T4		P4;
      typedef T5		P5;
      typedef T6		P6;

      //
      // constants
      //
      static const Natural32	Quantum = 6;
    };

    ///
    /// seven parameter.
    ///
    template <typename T1,
	      typename T2,
	      typename T3,
	      typename T4,
	      typename T5,
	      typename T6,
	      typename T7>
    struct Parameters<T1,
		      T2,
		      T3,
		      T4,
		      T5,
		      T6,
		      T7>:
      public Base<Parameters>
    {
      //
      // types
      //
      typedef T1		P1;
      typedef T2		P2;
      typedef T3		P3;
      typedef T4		P4;
      typedef T5		P5;
      typedef T6		P6;
      typedef T7		P7;

      //
      // constants
      //
      static const Natural32	Quantum = 7;
    };

    ///
    /// eight parameter.
    ///
    template <typename T1,
	      typename T2,
	      typename T3,
	      typename T4,
	      typename T5,
	      typename T6,
	      typename T7,
	      typename T8>
    struct Parameters<T1,
		      T2,
		      T3,
		      T4,
		      T5,
		      T6,
		      T7,
		      T8>:
      public Base<Parameters>
    {
      //
      // types
      //
      typedef T1		P1;
      typedef T2		P2;
      typedef T3		P3;
      typedef T4		P4;
      typedef T5		P5;
      typedef T6		P6;
      typedef T7		P7;
      typedef T8		P8;

      //
      // constants
      //
      static const Natural32	Quantum = 8;
    };

    ///
    /// nine parameter.
    ///
    template <typename T1,
	      typename T2,
	      typename T3,
	      typename T4,
	      typename T5,
	      typename T6,
	      typename T7,
	      typename T8,
	      typename T9>
    struct Parameters<T1,
		      T2,
		      T3,
		      T4,
		      T5,
		      T6,
		      T7,
		      T8,
		      T9>:
      public Base<Parameters>
    {
      //
      // types
      //
      typedef T1		P1;
      typedef T2		P2;
      typedef T3		P3;
      typedef T4		P4;
      typedef T5		P5;
      typedef T6		P6;
      typedef T7		P7;
      typedef T8		P8;
      typedef T9		P9;

      //
      // constants
      //
      static const Natural32	Quantum = 9;
    };

  }
}

#endif
