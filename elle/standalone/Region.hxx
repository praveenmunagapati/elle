//
// ---------- header ----------------------------------------------------------
//
// project       elle
//
// license       infinit
//
// author        julien quintard   [sun may  2 12:28:06 2010]
//

#ifndef ELLE_STANDALONE_REGION_HXX
#define ELLE_STANDALONE_REGION_HXX

namespace elle
{
  namespace standalone
  {

//
// ---------- methods ---------------------------------------------------------
//

    ///
    /// this method recycles a region.
    ///
    template <typename T>
    Status              Region::Recycle(const T*                object)
    {
      // release the resources.
      this->~Region();

      if (object == NULL)
        {
          // initialize the object with default values.
          new (this) T;
        }
      else
        {
          // initialize the object with defined values.
          new (this) T(*object);
        }

      // return Status::Ok in order to avoid including Report, Status and Maid.
      return Status::Ok;
    }

  }
}

#endif
#ifndef  ELLE_SERIALIZE_REGION_SERIALIZER_HXX
# define ELLE_SERIALIZE_REGION_SERIALIZER_HXX

# include <elle/utility/Buffer.hh>

# include <elle/standalone/Region.hh>

ELLE_SERIALIZE_SPLIT(elle::standalone::Region)

ELLE_SERIALIZE_SPLIT_SAVE(elle::standalone::Region,
                          archive,
                          value,
                          version)
{
  assert(version == 0);
  archive << static_cast<uint64_t>(value.size);
  archive.SaveBinary(value.contents, value.size);
}

ELLE_SERIALIZE_SPLIT_LOAD(elle::standalone::Region,
                          archive,
                          value,
                          version)
{
  uint64_t size;
  assert(version == 0);
  archive >> size;
  if (value.Prepare(size) != elle::Status::Ok)
    throw std::runtime_error("Cannot prepare the region");
  archive.LoadBinary(value.contents, size);
  value.size = size;
}

#endif
