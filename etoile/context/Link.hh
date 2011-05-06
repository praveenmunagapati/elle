//
// ---------- header ----------------------------------------------------------
//
// project       etoile
//
// license       infinit
//
// file          /home/mycure/infinit/etoile/context/Link.hh
//
// created       julien quintard   [fri aug 14 23:13:51 2009]
// updated       julien quintard   [thu may  5 16:25:37 2011]
//

#ifndef ETOILE_CONTEXT_LINK_HH
#define ETOILE_CONTEXT_LINK_HH

//
// ---------- includes --------------------------------------------------------
//

#include <elle/Elle.hh>
#include <nucleus/Nucleus.hh>

#include <etoile/context/Object.hh>

namespace etoile
{
  namespace context
  {

//
// ---------- classes ---------------------------------------------------------
//

    ///
    /// this context represents a link object as it embeds
    /// a reference along with inherited object-related stuff.
    ///
    class Link:
      public Object
    {
    public:
      //
      // types
      //
      typedef nucleus::Reference	Content;

      //
      // constructors & destructors
      //
      Link();
      ~Link();

      //
      // interfaces
      //

      // dumpable
      elle::Status	Dump(const elle::Natural32 = 0) const;

      //
      // attributes
      //
      nucleus::Contents<Content>*	contents;
    };

  }
}

#endif
