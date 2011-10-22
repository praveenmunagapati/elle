//
// ---------- header ----------------------------------------------------------
//
// project       etoile
//
// license       infinit
//
// author        julien quintard   [fri aug 14 23:13:51 2009]
//

#ifndef ETOILE_GEAR_FILE_HH
#define ETOILE_GEAR_FILE_HH

//
// ---------- includes --------------------------------------------------------
//

#include <elle/Elle.hh>
#include <nucleus/Nucleus.hh>

#include <etoile/gear/Object.hh>
#include <etoile/gear/Nature.hh>

#include <etoile/automaton/File.hh>

namespace etoile
{
  namespace gear
  {

//
// ---------- classes ---------------------------------------------------------
//

    ///
    /// this class represents a file-specific context.
    ///
    /// this context derives the Object context and therefore benefits from
    /// all the object-related attributes plus the contents i.e the catalog
    /// in the case of a file.
    ///
    class File:
      public Object
    {
    public:
      //
      // constants
      //
      static const Nature			N = NatureFile;

      //
      // types
      //
      typedef automaton::File			A;

      typedef nucleus::Data			C;

      //
      // constructors & destructors
      //
      File();
      ~File();

      //
      // interfaces
      //

      // dumpable
      elle::Status	Dump(const elle::Natural32 = 0) const;

      // archivable
      elle::Status	Serialize(elle::Archive&) const;
      elle::Status	Extract(elle::Archive&);

      //
      // attributes
      //
      nucleus::Contents<C>*	contents;
    };

  }
}

#endif
