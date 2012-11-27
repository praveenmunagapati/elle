#ifndef NUCLEUS_PROTON_CONTENTS_HH
# define NUCLEUS_PROTON_CONTENTS_HH

# include <nucleus/proton/ContentHashBlock.hh>
# include <nucleus/proton/Node.hh>
# include <nucleus/proton/Address.hh>

# include <cryptography/fwd.hh>
// XXX[temporary: for cryptography]
using namespace infinit;

# include <elle/attribute.hh>
# include <elle/idiom/Open.hh>

// XXX
# include <cryptography/KeyPair.hh>

namespace nucleus
{
  namespace proton
  {

    ///
    /// XXX[boxes]
    ///
    /// this class abstracts the data, catalog or reference by embedding
    /// it since the contents is always encrypted. XXX[will soon be overwritten
    /// by the porcupine code]
    ///
    /// therefore, once a contents is retrieved from the storage, the Extract()
    /// method is called which basically extracts an archive i.e the encrypted
    /// version of the embedded block. Then the Decrypt() method can be called
    /// in order to (i) decrypt the embedded archive (ii) extract it (iii)
    /// build the embedded unencrypted object.
    ///
    /// the _cipher_ attribute contains the data once encrypted, often
    /// just before being stored in the storage layer since there is no
    /// benefit in encrypting the data for fun.
    ///
    /// XXX expliquer que contents ca represente un block de donnee chiffree
    ///     qui peut potentiellement depasser la taille (extent) et qui sera
    ///     donc split ou merge si trop petit.
    ///     pour cette raison cette classe contient des methodes communes
    ///     a beaucoup de sous-classes: Seam, Quill, Catalog etc.
    ///
    class Contents:
      public proton::ContentHashBlock,
      public elle::serialize::SerializableMixin<Contents>,
      public elle::concept::UniquableMixin<Contents>
    {
      /*-------------.
      | Enumerations |
      `-------------*/
    public:
      /// Represent all the nodes which can be embedded in a Contents i.e
      /// which need to be encrypted.
      enum class Type
        {
          data_seam,
          data_quill,
          data_value,
          catalog_seam,
          catalog_quill,
          catalog_value,
          reference_seam,
          reference_quill,
          reference_value
        };

      /// Define the mode of the contents i.e the state of its embedded node
      /// being either encrypted or decrypted.
      enum class Mode
        {
          encrypted,
          decrypted
        };

      /*---------------.
      | Static Methods |
      `---------------*/
    public:
      /// Return the factory capable of building inner-contents classes.
      static
      elle::utility::Factory<Type> const&
      factory();

      /*----------.
      | Constants |
      `----------*/
    public:
      struct Constants
      {
        static neutron::Component const component;
      };

      /*-------------.
      | Construction |
      `-------------*/
    public:
      Contents(); // XXX[to deserialize]
      /// Construct a contents based on the given node.
      ///
      /// Note that the ownership of the given node is transferred to the
      /// contents.
      template <typename T>
      Contents(Network const& network,
               cryptography::PublicKey const& creator_K,
               T* node);
      ~Contents();

      // XXX
      template <typename T>
      Contents(T* node): // XXX[TEMPORARY!!! TO ANNIHILATE]
        proton::ContentHashBlock(Network("local"),
                                 neutron::ComponentContents,
                                 cryptography::KeyPair::generate(1024).K()),

        _type(T::Constants::type),
        _node(node),
        _cipher(nullptr)
      {
      }
      // XXX

      /*--------.
      | Methods |
      `--------*/
    public:
      /// Encrypt the contents with the given key.
      void
      encrypt(cryptography::SecretKey const& key);
      /// Decrypt the contents with the given key.
      void
      decrypt(cryptography::SecretKey const& key);
      /// Return the mode of the contents i.e either encrypted or decrypted.
      Mode
      mode() const;
      /// Return the node (in its decrypted form) referenced by the contents.
      Node const&
      node() const;
      /// Return the node (in its decrypted form) referenced by the contents.
      Node&
      node();

      /*-----------.
      | Interfaces |
      `-----------*/
    public:
      // dumpable
      elle::Status
      Dump(const elle::Natural32 = 0) const;
      // serializable
      ELLE_SERIALIZE_FRIEND_FOR(Contents);
      ELLE_SERIALIZE_SERIALIZABLE_METHODS(Contents);
      // printable
      virtual
      void
      print(std::ostream& stream) const;

      /*-----------.
      | Attributes |
      `-----------*/
    private:
      ELLE_ATTRIBUTE_R(Type, type);
      ELLE_ATTRIBUTE(Node*, node);
      ELLE_ATTRIBUTE(cryptography::Cipher*, cipher);
    };

    /*----------.
    | Operators |
    `----------*/

    std::ostream&
    operator <<(std::ostream& stream,
                Contents::Type const type);
    std::ostream&
    operator <<(std::ostream& stream,
                Contents::Mode const mode);
  }
}

# include <nucleus/proton/Contents.hxx>

#endif
