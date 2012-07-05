#ifndef  NUCLEUS_PROTON_OWNERKEYBLOCK_HXX
# define NUCLEUS_PROTON_OWNERKEYBLOCK_HXX

# include <cassert>

# include <nucleus/proton/OwnerKeyBlock.hh>

ELLE_SERIALIZE_SIMPLE(nucleus::proton::OwnerKeyBlock,
                      archive,
                      value,
                      version)
{
  assert(version == 0);

  archive & static_cast<nucleus::proton::MutableBlock&>(value);

  archive & value.K;
  archive & value.stamp;
  archive & value.owner.K;
  archive & value.owner.signature;
}

#endif
