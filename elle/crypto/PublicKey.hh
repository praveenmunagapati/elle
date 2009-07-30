//
// ---------- header ----------------------------------------------------------
//
// project       elle
//
// license       infinit (c)
//
// file          /home/mycure/infinit/elle/crypto/PublicKey.hh
//
// created       julien quintard   [tue oct 30 01:08:16 2007]
// updated       julien quintard   [thu jul 30 18:42:57 2009]
//

#ifndef ELLE_CRYPTO_PUBLICKEY_HH
#define ELLE_CRYPTO_PUBLICKEY_HH

//
// ---------- includes --------------------------------------------------------
//

#include <elle/io/IO.hh>

#include <elle/core/Core.hh>

#include <elle/misc/Misc.hh>

#include <elle/archive/Archive.hh>

#include <elle/crypto/Plain.hh>
#include <elle/crypto/Code.hh>
#include <elle/crypto/Signature.hh>
#include <elle/crypto/Digest.hh>
#include <elle/crypto/OneWay.hh>
#include <elle/crypto/SecretKey.hh>

#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>

namespace elle
{
  using namespace io;
  using namespace core;
  using namespace misc;
  using namespace archive;

  namespace crypto
  {

//
// ---------- classes ---------------------------------------------------------
//

    ///
    /// this class represents a public key.
    ///
    class PublicKey:
      public Entity,
      public Dumpable, public Archivable
    {
    public:
      //
      // constructors & destructors
      //
      PublicKey();
      ~PublicKey();

      //
      // methods
      //
      Status		Create(const ::EVP_PKEY*);
      Status		Create(Large*,
			       Large*);

      Status		Encrypt(const Plain&,
				Code&) const;
      Status		Verify(const Signature&,
			       const Plain&) const;

      //
      // interfaces
      //

      // entity
      PublicKey&	operator=(const PublicKey&);
      Boolean		operator==(const PublicKey&);
      Boolean		operator!=(const PublicKey&);

      // dumpable
      Status		Dump(const Natural32 = 0);

      // archivable
      Status		Serialize(Archive&) const;
      Status		Extract(Archive&);

      //
      // attributes
      //
      ::EVP_PKEY*	key;

      struct
      {
	::EVP_PKEY_CTX*	encrypt;
	::EVP_PKEY_CTX*	verify;
      }			contexts;

      //
      // variadic methods
      //

      // encrypt
      Status		Encrypt(const Archivable&,
				Code&) const;
      Status		Encrypt(const Archivable&,
				const Archivable&,
				Code&) const;
      Status		Encrypt(const Archivable&,
				const Archivable&,
				const Archivable&,
				Code&) const;
      Status		Encrypt(const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				Code&) const;
      Status		Encrypt(const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				Code&) const;
      Status		Encrypt(const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				Code&) const;
      Status		Encrypt(const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				Code&) const;
      Status		Encrypt(const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				Code&) const;
      Status		Encrypt(const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				const Archivable&,
				Code&) const;

      // verify
      Status		Verify(const Signature&,
			       const Archivable&) const;
      Status		Verify(const Signature&,
			       const Archivable&,
			       const Archivable&) const;
      Status		Verify(const Signature&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&) const;
      Status		Verify(const Signature&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&) const;
      Status		Verify(const Signature&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&) const;
      Status		Verify(const Signature&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&) const;
      Status		Verify(const Signature&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&) const;
      Status		Verify(const Signature&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&) const;
      Status		Verify(const Signature&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&,
			       const Archivable&) const;

    };

  }
}

#endif
