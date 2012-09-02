#ifndef ELLE_SERIALIZE_ARCHIVESERIALIZER_HXX
# define ELLE_SERIALIZE_ARCHIVESERIALIZER_HXX

///
/// This module provides tools to specialize the class ArchiveSerializer with
/// your types, or in other words, to define your own serializers.
///
/// To define a serializer the hard way, you'll have to specialize the class ArchiveSerializer
/// with your type in the namespace elle::serialize.
/// ----------------------------------
/// XXX example here
/// ----------------------------------
///
/// If you need to do something different to save and load your type, you can
/// simply derive from the class SplitSerializer<T>, where T is your type. It
/// will call Save() or Load() depending on the archive mode.
/// ----------------------------------
/// XXX example here
/// ----------------------------------
///
/// This module also provides some macros to help you defining common
/// serializers with far less verbosity.
/// These macros uses this kind of arguments:
///   * T: the type to specialize
///   * archive: the variable name of the archive (its template type is Archive)
///   * value: the variable name of the value (its type is T)
///   * version: the variable name of the version (its type is unsigned long)
///
/// ELLE_SERIALIZE_SIMPLE(T, archive, value, version) : both way serialization method
/// ----------------------------------
/// struct A
/// {
///   int i;
///   short k;
/// };
///
/// ELLE_SERIAZE_SIMPLE(A, archive, value, version)
/// {
///    // 0 is the current and only version of A serializer.
///   assert(version == 0);
///
///   // both way serialization operator
///   archive & i;
///   archive & k;
/// }
/// -----------------------------------
///

# include <cassert>
# include <type_traits>

# include <elle/print.hh>

# include "ArchivableClass.hh"
# include "ArchiveMode.hh"
# include "Concrete.hh"
# include "StoreClassVersion.hh"

namespace elle
{
  namespace serialize
  {

    ///
    /// The BaseArchiveSerializer define common standard function involved in
    /// the serialization process. You might want to derive from this struct for
    /// each specialization of the template class ArchiveSerializer.
    ///
    template <typename T>
    struct BaseArchiveSerializer
    {
      template <typename Super>
      static inline
      Concrete<Super>
      base_class(T& obj)
      {
        return Concrete<Super>(static_cast<Super&>(obj));
      }

      template <typename Super>
      static inline
      Concrete<Super const>
      base_class(T const& obj)
      {
        return Concrete<Super const>(static_cast<Super const&>(obj));
      }
      ///
      /// Having a chance to construct yourself the object (with a placement new)
      /// can be achieved by overriding this function.
      /// The default implementation just construct the object without parameters
      /// and forward to standard Load() method.
      ///
      template <typename Archive>
      static inline void LoadConstruct(Archive& archive, T*& ptr)
      {
        assert(ptr == nullptr);
        static_assert(!std::is_pointer<T>::value, "You cannot construct pointers...");
        ptr = new T;
        archive >> *ptr;
      }

    };

    ///
    /// This type is instanciated for any user defined type. You will have to
    /// make a specialization for each user defined type.
    ///
    /// @see ELLE_SERIALIZE_SIMPLE() for an helper
    ///
    template<typename T> struct ArchiveSerializer
      : public BaseArchiveSerializer<T>
    {
      ///
      /// Generic method used by either serialization and deserialization
      /// process. You may want to use the '&' operator that do the right
      /// operation depending on the archive mode.
      ///
      template<typename Archive>
      static void Serialize(Archive&, T&, unsigned int)
      {
        static_assert(
            ArchivableClass<T>::version != 0,
            "You have to specialize ArchiveSerializer<T> for your type."
        );
      }
    };


      namespace detail {

          // Call the Save() or Load() depending on the archive mode.
          template<typename T, ArchiveMode mode>
            struct _SplitSerializerSelectMethod;

          template<typename T>
            struct _SplitSerializerSelectMethod<T, ArchiveMode::Input>
            {
              template<typename Archive>
                static inline void Serialize(Archive& ar,
                                             T& val,
                                             unsigned int version)
                { ArchiveSerializer<T>::Load(ar, val, version); }
            };

          template<typename T>
            struct _SplitSerializerSelectMethod<T, ArchiveMode::Output>
            {
              template<typename Archive>
              static inline void Serialize(Archive& ar,
                                           T const& val,
                                           unsigned int version)
              { ArchiveSerializer<T>::Save(ar, val, version); }
            };

      } // !namespace detail

      /// Home made serializer specialization can inherit this class to inherit
      /// a split serializer out of the box.
      template<typename T>
        struct SplitSerializer:
          public BaseArchiveSerializer<T>
        {
          template<typename Archive>
          static inline void Serialize(Archive& ar,
                                       T& val,
                                       unsigned int version)
          {
            typedef detail::_SplitSerializerSelectMethod<
                T
              , Archive::mode
            > Method;
            Method::Serialize(ar, val, version);
          }
        };

  }
} // !namespace elle::serialize

# define _ELLE_SERIALIZE_LOG_ACTION(T, version, mode, _value)                   \
  ELLE_LOG_COMPONENT("elle.serialize");                                         \
  ELLE_TRACE("%s " #T " (%s): %p",                                              \
                 mode == ArchiveMode::Input ? "Loading" : "Saving",             \
                 (elle::serialize::StoreClassVersion<T>::value                  \
                    ? elle::sprint("version", version)                          \
                    : "no version"),                                            \
                 &_value)                                                       \
  /**/

/// Define a simple serializer for the type T
# define ELLE_SERIALIZE_SIMPLE(T, archive, value, version)                    \
  ELLE_SERIALIZE_SIMPLE_TN(T, archive, value, version, 0)                     \
/**/

/// Define a simple serializer for the type T<T1>
# define ELLE_SERIALIZE_SIMPLE_T1(T, archive, value, version)                 \
  ELLE_SERIALIZE_SIMPLE_TN(T, archive, value, version, 1)                     \
/**/

/// Define a simple serializer for the type T<T1, T2>
# define ELLE_SERIALIZE_SIMPLE_T2(T, archive, value, version)                 \
  ELLE_SERIALIZE_SIMPLE_TN(T, archive, value, version, 2)                     \
/**/

/// Define a simple serializer for the type T<T1, ..., TN>
# define ELLE_SERIALIZE_SIMPLE_TN(T, archive, value, version, n)              \
namespace elle { namespace serialize {                                        \
    __ESA_TEMPLATE_DECL_N(n)                                                  \
    struct ArchiveSerializer<__ESA_TEMPLATE_TYPE(T, n)>:                      \
        public BaseArchiveSerializer<__ESA_TEMPLATE_TYPE(T, n)>               \
    {                                                                         \
      ELLE_SERIALIZE_BASE_CLASS_MIXIN_TN(T, n)                                \
      template<typename Archive>                                              \
      static inline                                                           \
      void                                                                    \
      Serialize(Archive& archive,                                             \
                __ESA_TEMPLATE_TYPE(T, n)& value,                             \
                unsigned int version)                                         \
        {                                                                     \
          _ELLE_SERIALIZE_LOG_ACTION(                                         \
              __ESA_TEMPLATE_TYPE(T, n),                                      \
              version,                                                        \
              Archive::mode,                                                  \
              value                                                           \
          ) { _Serialize(archive, value, version); }                          \
        }                                                                     \
    private:                                                                  \
      template<typename Archive>                                              \
      static inline                                                           \
      void                                                                    \
      _Serialize(Archive&, __ESA_TEMPLATE_TYPE(T, n)&, unsigned int);         \
    };                                                                        \
}}                                                                            \
__ESA_TEMPLATE_DECL(n)                                                        \
template<typename Archive>                                                    \
void                                                                          \
elle::serialize::ArchiveSerializer<__ESA_TEMPLATE_TYPE(T, n)>::_Serialize(    \
    Archive& archive,                                                         \
    __ESA_TEMPLATE_TYPE(T, n)& value,                                         \
    unsigned int version)                                                     \
/**/

# define ELLE_SERIALIZE_BASE_CLASS_MIXIN_TN(__T, n)                           \
  template <typename Super>                                                   \
  static inline                                                               \
  Concrete<Super>                                                             \
  base_class(__ESA_TEMPLATE_TYPE(__T, n)& obj)                                \
  { return Concrete<Super>(static_cast<Super&>(obj)); }                       \
  template <typename Super>                                                   \
  static inline                                                               \
  Concrete<Super const>                                                       \
  base_class(__ESA_TEMPLATE_TYPE(__T, n) const& obj)                          \
  { return Concrete<Super>(static_cast<Super const&>(obj)); }                 \
/**/

//- internal macros -----------------------------------------------------------

# include <boost/preprocessor.hpp> // XXX

#define __ESA_TYPENAME_REPEAT(z, n, prefix)                                   \
  BOOST_PP_COMMA_IF(n) BOOST_PP_CAT(prefix T, BOOST_PP_INC(n))                \
/**/

// T0, ..., TN
#define __ESA_TEMPLATE_ARGS(n)                                                \
  BOOST_PP_REPEAT(n, __ESA_TYPENAME_REPEAT, BOOST_PP_EMPTY())                 \
/**/

// typename T0, ..., typename TN
#define __ESA_TEMPLATE_ARGS_DEF(n)                                            \
  BOOST_PP_REPEAT(n, __ESA_TYPENAME_REPEAT, typename)                         \
/**/

// template<typename T0, ..., typename Tn>
# define __ESA_TEMPLATE_DECL_N(n)                                             \
  template<__ESA_TEMPLATE_ARGS_DEF(n)>                                        \
/**/

# define __ESA_TEMPLATE_DECL(n)                                               \
  BOOST_PP_IF(n, __ESA_TEMPLATE_DECL_N(n), BOOST_PP_EMPTY())                  \
/**/

// T<T0, ..., TN>
# define __ESA_TEMPLATE_TYPE_TN(T, n)                                         \
  T<__ESA_TEMPLATE_ARGS(n)>                                                   \
/**/

// if (n > 0) T<T0, ..., TN> else T
# define __ESA_TEMPLATE_TYPE(T, n)                                            \
  BOOST_PP_IF(n, __ESA_TEMPLATE_TYPE_TN(T, n), T)                             \
/**/

/// Declare a split serializer for the type T
# define ELLE_SERIALIZE_SPLIT(T)                                                \
namespace elle { namespace serialize {                                          \
                                                                                \
  template<> struct ArchiveSerializer<T>                                        \
    : public BaseArchiveSerializer<T>                                           \
    {                                                                           \
    private:                                                                    \
      typedef T Type;                                                           \
                                                                                \
    public:                                                                     \
      template<typename Archive>                                                \
        static void Serialize(Archive& ar, Type& val, unsigned int version)     \
        {                                                                       \
          typedef detail::_SplitSerializerSelectMethod<                         \
              Type                                                              \
            , Archive::mode                                                     \
          > Method;                                                             \
          _ELLE_SERIALIZE_LOG_ACTION(T, version, Archive::mode, val)            \
            { Method::Serialize(ar, val, version); }                            \
        }                                                                       \
        template<typename Archive>                                              \
          static void Save(Archive& ar, Type const& val, unsigned int version); \
        template<typename Archive>                                              \
          static void Load(Archive& ar, Type& val, unsigned int version);       \
    };                                                                          \
}}                                                                              \
  /**/


/// Define the Save() method of the split serializer for the type T
# define ELLE_SERIALIZE_SPLIT_SAVE(T, archive, value, version)                  \
template<typename Archive>                                                      \
  void elle::serialize::ArchiveSerializer<T>::Save(                             \
                                              Archive& archive,                 \
                                              Type const& value,                \
                                              unsigned int version)             \
  /**/


/// Define the Load() method of the split serializer for the type T
# define ELLE_SERIALIZE_SPLIT_LOAD(T, archive, value, version)                  \
template<typename Archive>                                                      \
  void elle::serialize::ArchiveSerializer<T>::Load(                             \
                                              Archive& archive,                 \
                                              Type& value,                      \
                                              unsigned int version)             \
  /**/


/// Declare a split serializer for the type T<T1>
# define ELLE_SERIALIZE_SPLIT_T1(T)                                             \
namespace elle { namespace serialize {                                          \
                                                                                \
  template<typename T1> struct ArchiveSerializer<T<T1>>                         \
    : public BaseArchiveSerializer<T<T1>>                                       \
    {                                                                           \
    private:                                                                    \
      typedef T<T1> Type;                                                       \
                                                                                \
    public:                                                                     \
      template<typename Archive>                                                \
        static void Serialize(Archive& ar, Type& val, unsigned int version)     \
        {                                                                       \
          typedef detail::_SplitSerializerSelectMethod<                         \
              Type                                                              \
            , Archive::mode                                                     \
          > Method;                                                             \
          _ELLE_SERIALIZE_LOG_ACTION(T<T1>, version, Archive::mode, val)        \
            { Method::Serialize(ar, val, version); }                            \
        }                                                                       \
        template<typename Archive>                                              \
          static void Save(Archive& ar, Type const& val, unsigned int version); \
        template<typename Archive>                                              \
          static void Load(Archive& ar, Type& val, unsigned int version);       \
    };                                                                          \
}}                                                                              \
  /**/


/// Define the Save() method of the split serializer for the type T
# define ELLE_SERIALIZE_SPLIT_T1_SAVE(T, archive, value, version)               \
template<typename T1>                                                           \
template<typename Archive>                                                      \
  void elle::serialize::ArchiveSerializer<T<T1>>::Save(                         \
                                              Archive& archive,                 \
                                              Type const& value,                \
                                              unsigned int version)             \
  /**/


/// Define the Load() method of the split serializer for the type T
# define ELLE_SERIALIZE_SPLIT_T1_LOAD(T, archive, value, version)               \
template<typename T1>                                                           \
template<typename Archive>                                                      \
  void elle::serialize::ArchiveSerializer<T<T1>>::Load(                         \
                                              Archive& archive,                 \
                                              Type& value,                      \
                                              unsigned int version)             \
  /**/

# include <elle/idiom/Close.hh>
# include <elle/log.hh>
# include <elle/idiom/Close.hh>

#endif
