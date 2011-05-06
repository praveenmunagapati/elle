//
// ---------- header ----------------------------------------------------------
//
// project       etoile
//
// license       infinit
//
// file          /home/mycure/infinit/etoile/etoile.cc
//
// created       julien quintard   [wed mar  3 22:36:08 2010]
// updated       julien quintard   [fri may  6 14:23:37 2011]
//

//
// ---------- includes --------------------------------------------------------
//

#include <etoile/Etoile.hh>

namespace etoile
{

//
// ---------- functions -------------------------------------------------------
//

  elle::Status		Main(const elle::Natural32		argc,
			     const elle::Character*		argv[])
  {
    nucleus::Address	root;

    enter();

    // initialize the Elle library.
    if (elle::Elle::Initialize() == elle::StatusError)
      escape("unable to initialize the Elle library");

    // set up the program.
    if (elle::Program::Setup() == elle::StatusError)
      escape("unable to set up the program");

    // initialize the nucleus library.
    if (nucleus::Nucleus::Initialize() == elle::StatusError)
      escape("unable to initialize Nucleus");

    // initialize the lune library.
    if (lune::Lune::Initialize() == elle::StatusError)
      escape("unable to initialize the Lune library");

    // XXX
    {
      // XXX[hack for kapoue]
      {
	lune::Descriptor descriptor;

	if (descriptor.Load("kapoue") == elle::StatusError)
	  escape("unable to load the descriptor");

	root = descriptor.root;

	std::cout << "[root] " << root << std::endl;
      }
    }


    // initialize etoile.
    if (Etoile::Initialize(root) == elle::StatusError)
      escape("unable to initialize etoile");

    // launch the program.
    if (elle::Program::Launch() == elle::StatusError)
      escape("an error occured while processing events");

    // clean etoile.
    if (Etoile::Clean() == elle::StatusError)
      escape("unable to clean etoile");

    // clean the Lune library.
    if (lune::Lune::Clean() == elle::StatusError)
      escape("unable to clean the Lune library");

    // clean the nucleus library.
    if (nucleus::Nucleus::Clean() == elle::StatusError)
      escape("unable to clean Nucleus");

    // clean the Elle library.
    if (elle::Elle::Clean() == elle::StatusError)
      escape("unable to clean the Elle library");

    leave();
  }

}

//
// ---------- main ------------------------------------------------------------
//

///
/// XXX
///
int			main(const int			argc,
			     const char*		argv[])
{
  etoile::Main(argc, argv);

  expose();

  return (0);
}
